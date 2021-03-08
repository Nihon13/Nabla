#ifndef __NBL_C_OPENGL__QUEUE_H_INCLUDED__
#define __NBL_C_OPENGL__QUEUE_H_INCLUDED__

#include <variant>
#include "nbl/video/IGPUQueue.h"
#include "nbl/video/COpenGLSemaphore.h"
#include "nbl/video/COpenGLFence.h"
#include "nbl/video/COpenGLSync.h"
#include "nbl/video/COpenGLFramebuffer.h"
#include "nbl/system/IAsyncQueueDispatcher.h"
#include "nbl/video/CEGL.h"
#include "nbl/video/IOpenGL_FunctionTable.h"
#include "nbl/video/SOpenGLContextLocalCache.h"
#include "nbl/video/COpenGLCommandBuffer.h"
#include "nbl/video/COpenGL_Swapchain.h"

namespace nbl {
namespace video
{

template <typename FunctionTableType_>
class COpenGL_Queue final : public IGPUQueue
{
    public:
        using FunctionTableType = FunctionTableType_;
        using FeaturesType = typename FunctionTableType::features_t;

    private:
        static inline constexpr bool IsGLES = (FunctionTableType::EGL_API_TYPE == EGL_OPENGL_ES_API);

        static inline GLbitfield pipelineStageFlagsToMemBarrierBits(asset::E_PIPELINE_STAGE_FLAGS flags)
        {
            constexpr GLbitfield VertexInputBits = GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT;
            constexpr GLbitfield AnyShaderStageCommonBits = GL_UNIFORM_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT;
            constexpr GLbitfield TransferBits = GL_PIXEL_BUFFER_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT;
            constexpr GLbitfield HostBits = TransferBits | IOpenGL_FunctionTable::CLIENT_MAPPED_BUFFER_BARRIER_BIT;
            constexpr GLbitfield AllGraphicsBits = GL_COMMAND_BARRIER_BIT | VertexInputBits | AnyShaderStageCommonBits;
            constexpr GLbitfield AllCommandsBits = GL_ALL_BARRIER_BITS;
            constexpr uint32_t PipelineStageCount = 14u;
            const GLbitfield bits[PipelineStageCount] = {
                GL_ALL_BARRIER_BITS, // EPSF_TOP_OF_PIPE_BIT
                AnyShaderStageCommonBits | TransferBits | VertexInputBits | GL_COMMAND_BARRIER_BIT, // EPSF_DRAW_INDIRECT_BIT
                AnyShaderStageCommonBits | TransferBits | VertexInputBits, // EPSF_VERTEX_INPUT_BIT
                AnyShaderStageCommonBits | TransferBits, // EPSF_VERTEX_SHADER_BIT
                AnyShaderStageCommonBits | TransferBits, // EPSF_TESSELLATION_CONTROL_SHADER_BIT
                AnyShaderStageCommonBits | TransferBits, // EPSF_TESSELLATION_EVALUATION_SHADER_BIT
                AnyShaderStageCommonBits | TransferBits, // EPSF_GEOMETRY_SHADER_BIT
                AnyShaderStageCommonBits | TransferBits, // EPSF_FRAGMENT_SHADER_BIT
                AnyShaderStageCommonBits | TransferBits, // EPSF_EARLY_FRAGMENT_TESTS_BIT
                AnyShaderStageCommonBits | TransferBits, // EPSF_LATE_FRAGMENT_TESTS_BIT
                AnyShaderStageCommonBits | TransferBits, // EPSF_COLOR_ATTACHMENT_OUTPUT_BIT
                AnyShaderStageCommonBits | TransferBits, //EPSF_COMPUTE_SHADER_BIT
                TransferBits, // EPSF_TRANSFER_BIT
                0 // EPSF_BOTTOM_OF_PIPE_BIT
            };

            GLbitfield barrier = 0;
            if (flags & asset::EPSF_HOST_BIT)
                barrier |= HostBits;
            if (flags & asset::EPSF_ALL_GRAPHICS_BIT)
                barrier |= AllGraphicsBits;
            if (flags & asset::EPSF_ALL_COMMANDS_BIT)
                barrier |= AllCommandsBits;
            for (uint32_t i = 0u; i < PipelineStageCount; ++i)
                if (flags & (1u << i))
                    barrier |= bits[i];
            return barrier;
        }

        using ThreadHandlerInternalStateType = FunctionTableType;

        enum E_REQUEST_TYPE
        {
            ERT_SUBMIT,
            ERT_FENCE,
            ERT_DESTROY_FRAMEBUFFER,
            ERT_DESTROY_PIPELINE
        };
        template <E_REQUEST_TYPE ERT>
        struct SRequestParamsBase
        {
            static inline constexpr E_REQUEST_TYPE type = ERT;
        };
        struct SRequestParams_Submit : SRequestParamsBase<ERT_SUBMIT>
        {
            SSubmitInfo submit;
        };
        struct SRequestParams_Fence : SRequestParamsBase<ERT_FENCE>
        {
            core::smart_refctd_ptr<COpenGLFence> fence;
        };
        struct SRequestParams_DestroyFramebuffer : SRequestParamsBase<ERT_DESTROY_FRAMEBUFFER>
        {
            SOpenGLState::SFBOHash fbo_hash;
        };
        struct SRequestParams_DestroyPipeline : SRequestParamsBase<ERT_DESTROY_PIPELINE>
        {
            SOpenGLState::SGraphicsPipelineHash hash;
        };
        struct SRequest : public system::impl::IAsyncQueueDispatcherBase::request_base_t 
        {
            E_REQUEST_TYPE type;

            std::variant<SRequestParams_Submit, SRequestParams_Fence, SRequestParams_DestroyFramebuffer, SRequestParams_DestroyPipeline> params;
        };

        struct CThreadHandler final : public system::IAsyncQueueDispatcher<CThreadHandler, SRequest, 256u, ThreadHandlerInternalStateType>
        {
        public:
            CThreadHandler(const egl::CEGL* _egl, FeaturesType* _features, EGLContext _master, EGLConfig _config, EGLint _major, EGLint _minor, uint32_t _ctxid) :
                egl(_egl),
                masterCtx(_master), config(_config),
                major(_major), minor(_minor),
                thisCtx(EGL_NO_CONTEXT), pbuffer(EGL_NO_SURFACE),
                features(_features),
                m_ctxid(_ctxid)
            {

            }

        protected:
            using base_t = system::IAsyncQueueDispatcher<CThreadHandler, SRequest, 256u, ThreadHandlerInternalStateType>;
            friend base_t;

            ThreadHandlerInternalStateType init()
            {
                egl->call.peglBindAPI(FunctionTableType::EGL_API_TYPE);

                const EGLint ctx_attributes[] = {
                    EGL_CONTEXT_MAJOR_VERSION, major,
                    EGL_CONTEXT_MINOR_VERSION, minor,
                    //EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, // core profile is default setting

                    EGL_NONE
                };

                thisCtx = egl->call.peglCreateContext(egl->display, config, masterCtx, ctx_attributes);

                // why not 1x1?
                const EGLint pbuffer_attributes[] = {
                    EGL_WIDTH, 128,
                    EGL_HEIGHT, 128,

                    EGL_NONE
                };
                pbuffer = egl->call.peglCreatePbufferSurface(egl->display, config, pbuffer_attributes);

                egl->call.peglMakeCurrent(egl->display, pbuffer, pbuffer, thisCtx);

                auto gl = ThreadHandlerInternalStateType(egl, features);

                // defaults once set and not tracked by engine (should never change)
                gl.glGeneral.pglEnable(IOpenGL_FunctionTable::FRAMEBUFFER_SRGB);
                gl.glFragment.pglDepthRangef(1.f, 0.f);
                if constexpr (IsGLES)
                {
                    if (gl.getFeatures()->isFeatureAvailable(COpenGLFeatureMap::NBL_EXT_clip_control)) // if not supported, modifications to spir-v will be applied to emulate this
                        gl.extGlClipControl(IOpenGL_FunctionTable::UPPER_LEFT, IOpenGL_FunctionTable::ZERO_TO_ONE);
                }
                else
                {
                    // on desktop GL clip control is assumed to be always supported
                    gl.extGlClipControl(IOpenGL_FunctionTable::UPPER_LEFT, IOpenGL_FunctionTable::ZERO_TO_ONE);
                }

                if constexpr (!IsGLES)
                {
                    gl.glGeneral.pglEnable(IOpenGL_FunctionTable::TEXTURE_CUBE_MAP_SEAMLESS);
                }

                // default values tracked by engine
                m_ctxlocal.nextState.rasterParams.multisampleEnable = 0;
                m_ctxlocal.nextState.rasterParams.depthFunc = GL_GEQUAL;
                m_ctxlocal.nextState.rasterParams.frontFace = GL_CCW;

                return gl;
            }

            template <typename RequestParams>
            void request_impl(SRequest& req, RequestParams&& params)
            {
                req.type = params.type;
                req.params = std::move(params);
            }

            void process_request(SRequest& req, ThreadHandlerInternalStateType& _gl)
            {
                static_assert(std::is_same_v<ThreadHandlerInternalStateType, FunctionTableType>);
                // a cast to common base so that intellisense knows function set (can and should be removed after code gets written)
                IOpenGL_FunctionTable& gl = static_cast<IOpenGL_FunctionTable&>(_gl);

                switch (req.type)
                {
                case ERT_SUBMIT:
                {
                    auto& p = std::get<SRequestParams_Submit>(req.params);
                    auto& submit = p.submit;

                    // wait semaphores
                    for (uint32_t i = 0; i < submit.waitSemaphoreCount; ++i)
                    {
                        GLbitfield barrierBits = pipelineStageFlagsToMemBarrierBits(submit.pWaitDstStageMask[i]);
                        gl.glSync.pglMemoryBarrier(barrierBits);

                        IGPUSemaphore* sem = submit.pWaitSemaphores[i];
                        COpenGLSemaphore* glsem = static_cast<COpenGLSemaphore*>(sem);
                        assert(glsem->isWaitable());
                        glsem->wait(&gl);
                    }

                    for (uint32_t i = 0u; i < submit.commandBufferCount; ++i)
                    {
                        //dynamic_cast because of virtual base
                        auto* cmdbuf = dynamic_cast<COpenGLCommandBuffer*>(submit.commandBuffers[i]);
                        cmdbuf->executeAll(&gl, &m_ctxlocal, m_ctxid);
                    }

                    for (uint32_t i = 0u; i < submit.signalSemaphoreCount; ++i)
                    {
                        IGPUSemaphore* sem = submit.pSignalSemaphores[i];
                        COpenGLSemaphore* glsem = static_cast<COpenGLSemaphore*>(sem);
                        glsem->signal(&gl);
                    }
                }
                break;
                case ERT_FENCE:
                {
                    auto& p = std::get<SRequestParams_Fence>(req.params);
                    core::smart_refctd_ptr<COpenGLFence> fence = std::move(p.fence);
                    fence->signal(&gl);
                }
                break;
                case ERT_DESTROY_FRAMEBUFFER:
                {
                    auto& p = std::get<SRequestParams_DestroyFramebuffer>(req.params);
                    auto fbo_hash = p.fbo_hash;
                    m_ctxlocal.removeFBOEntry(&gl, fbo_hash);
                }
                break;
                case ERT_DESTROY_PIPELINE:
                {
                    auto& p = std::get<SRequestParams_DestroyPipeline>(req.params);
                    auto hash = p.hash;
                    m_ctxlocal.removePipelineEntry(&gl, hash);
                }
                break;
                }
            }

            void exit(internal_state_t& gl)
            {
                egl->call.peglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT); // detach ctx from thread
                egl->call.peglDestroyContext(egl->display, thisCtx);
                egl->call.peglDestroySurface(egl->display, pbuffer);
            }

        private:
            const egl::CEGL* egl;
            EGLContext masterCtx;
            EGLConfig config;
            EGLint major, minor;
            EGLContext thisCtx;
            EGLSurface pbuffer;
            FeaturesType* features;
            uint32_t m_ctxid;

            mutable SOpenGLContextLocalCache m_ctxlocal;
        };

    public:
        COpenGL_Queue(ILogicalDevice* dev, const egl::CEGL* _egl, FeaturesType* _features, uint32_t _ctxid, EGLContext _masterCtx, EGLConfig _config, EGLint _gl_major, EGLint _gl_minor, uint32_t _famIx, E_CREATE_FLAGS _flags, float _priority) :
            IGPUQueue(dev, _famIx, _flags, _priority),
            threadHandler(_egl, _features, _masterCtx, _config, _gl_major, _gl_minor, _ctxid)
        {

        }

        void submit(uint32_t _count, const SSubmitInfo* _submits, IGPUFence* _fence) override
        {
            for (uint32_t i = 0u; i < _count; ++i)
            {
                SRequestParams_Submit params;
                const SSubmitInfo& submit = _submits[i];
                for (uint32_t i = 0u; i < submit.signalSemaphoreCount; ++i)
                {
                    COpenGLSemaphore* sem = static_cast<COpenGLSemaphore*>(submit.pSignalSemaphores[i]);
                    sem->setToBeSignaled();
                }
                params.submit = _submits[i];

                threadHandler.request(std::move(params));
                // wait on completion ?
            }
            if (_fence)
            {
                SRequestParams_Fence params;
                COpenGLFence* fence = static_cast<COpenGLFence*>(_fence);
                fence->setToBeSignaled();
                params.fence = core::smart_refctd_ptr<COpenGLFence>(fence);

                threadHandler.request(std::move(params));
                // wait on completion ?
            }
        }

        bool present(const SPresentInfo& info) override
        {
            for (uint32_t i = 0u; i < info.waitSemaphoreCount; ++i)
                if (!this->isCompatibleDevicewise(info.waitSemaphores[i]))
                    return false;
            if (uint32_t i = 0u; i < info.swapchainCount; ++i)
                if (!this->isCompatibleDevicewise(info.swapchains[i]))
                    return false;

            using swapchain_t = COpenGL_Swapchain<FunctionTableType_>;
            bool retval = true;
            if (uint32_t i = 0u; i < info.swapchainCount; ++i)
            {
                swapchain_t* sc = static_cast<swapchain_t*>(info.swapchains[i]);
                const uint32_t imgix = info.imgIndices[i];
                retval &= sc->present(imgix, info.waitSemaphoreCount, info.waitSemaphores);
            }

            return retval;
        }

        void destroyFramebuffer(COpenGLFramebuffer* fbo)
        {
            SRequestParams_DestroyFramebuffer params;
            params.fbo_hash = fbo->getHashValue();

            threadHandler.request(std::move(params));
        }

        void destroyPipeline(COpenGLRenderpassIndependentPipeline* pipeline)
        {
            SRequestParams_DestroyPipeline params;
            params.hash = pipeline->getPipelineHash();

            threadHandler.request(std::move(params));
        }

    protected:
        ~COpenGL_Queue()
        {
            threadHandler.terminate(thread);
        }

    private:
        CThreadHandler threadHandler;
};

}}

#endif