// Copyright (C) 2018-2020 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine".
// For conditions of distribution and use, see copyright notice in nabla.h

#define _NBL_STATIC_LIB_
#include <nabla.h>

#include "../common/CommonAPI.h"
#include "CFileSystem.h"
using namespace nbl;
using namespace core;

struct UBOCompute
{
	//xyz - gravity point, w - dt
	core::vectorSIMDf gravPointAndDt;
};
int main()
{
	constexpr uint32_t WIN_W = 1280;
	constexpr uint32_t WIN_H = 720;
	constexpr uint32_t SC_IMG_COUNT = 3u;
	constexpr uint32_t FRAMES_IN_FLIGHT = 5u;
	static_assert(FRAMES_IN_FLIGHT>SC_IMG_COUNT);

	auto initOutp = CommonAPI::Init<WIN_W, WIN_H, SC_IMG_COUNT>(video::EAT_OPENGL, "Specialization constants");
	auto win = std::move(initOutp.window);
	auto gl = std::move(initOutp.apiConnection);
	auto surface = std::move(initOutp.surface);
	auto device = std::move(initOutp.logicalDevice);
	auto gpu = std::move(initOutp.physicalDevice);
	auto queue = std::move(initOutp.queue);
	auto sc = std::move(initOutp.swapchain);
	auto renderpass = std::move(initOutp.renderpass);
	auto fbo = std::move(initOutp.fbo);
	auto cmdpool = std::move(initOutp.commandPool);

	auto filesystem = core::make_smart_refctd_ptr<io::CFileSystem>("");
	auto am = core::make_smart_refctd_ptr<asset::IAssetManager>(std::move(filesystem));
	auto glslExts = device->getSupportedGLSLExtensions();
	asset::CShaderIntrospector introspector(am->getGLSLCompiler());
	core::smart_refctd_ptr<asset::ICPUShader> computeUnspec;
	{
		// auto file = am->getFileSystem()->createAndOpenFile("../particles.comp");
		// computeUnspec = am->getGLSLCompiler()->resolveIncludeDirectives(file, asset::ISpecializedShader::ESS_COMPUTE, file->getFileName().c_str());
		// file->drop();
	}

	auto lastTime = std::chrono::high_resolution_clock::now();
	constexpr uint32_t FRAME_COUNT = 500000u;
	constexpr uint64_t MAX_TIMEOUT = 99999999999999ull;

	core::smart_refctd_ptr<video::IGPUCommandBuffer> cmdbuf[FRAMES_IN_FLIGHT];
	device->createCommandBuffers(cmdpool.get(),video::IGPUCommandBuffer::EL_PRIMARY,FRAMES_IN_FLIGHT,cmdbuf);
	core::smart_refctd_ptr<video::IGPUFence> frameComplete[FRAMES_IN_FLIGHT] = { nullptr };
	core::smart_refctd_ptr<video::IGPUSemaphore> imageAcquire[FRAMES_IN_FLIGHT] = { nullptr };
	core::smart_refctd_ptr<video::IGPUSemaphore> renderFinished[FRAMES_IN_FLIGHT] = { nullptr };
	for (uint32_t i=0u; i<FRAMES_IN_FLIGHT; i++)
	{
		imageAcquire[i] = device->createSemaphore();
		renderFinished[i] = device->createSemaphore();
	}

	// render loop
	for (uint32_t i = 0u; i < FRAME_COUNT; ++i)
	{
		const auto resourceIx = i%FRAMES_IN_FLIGHT;
		auto& cb = cmdbuf[resourceIx];
		auto& fence = frameComplete[resourceIx];
		if (fence)
		while (device->waitForFences(1u,&fence.get(),false,MAX_TIMEOUT)==video::IGPUFence::ES_TIMEOUT)
		{
		}
		else
			fence = device->createFence(static_cast<video::IGPUFence::E_CREATE_FLAGS>(0));

		// safe to proceed
		cb->begin(0);

		{
			asset::SViewport vp;
			vp.minDepth = 1.f;
			vp.maxDepth = 0.f;
			vp.x = 0u;
			vp.y = 0u;
			vp.width = WIN_W;
			vp.height = WIN_H;
			cb->setViewport(0u, 1u, &vp);
		}
		// renderpass 
		uint32_t imgnum = 0u;
		sc->acquireNextImage(MAX_TIMEOUT,imageAcquire[resourceIx].get(),nullptr,&imgnum);
		{
			video::IGPUCommandBuffer::SRenderpassBeginInfo info;
			asset::SClearValue clear;
			asset::VkRect2D area;
			clear.color.float32[0] = 0.f;
			clear.color.float32[1] = 0.f;
			clear.color.float32[2] = 0.f;
			clear.color.float32[3] = 1.f;
			info.renderpass = renderpass;
			info.framebuffer = fbo[imgnum];
			info.clearValueCount = 1u;
			info.clearValues = &clear;
			info.renderArea.offset = { 0, 0 };
			info.renderArea.extent = { WIN_W, WIN_H };
			cb->beginRenderPass(&info,asset::ESC_INLINE);
		}
		
		cb->endRenderPass();
		cb->end();

		CommonAPI::Submit(device.get(), sc.get(), cb.get(), queue, imageAcquire[resourceIx].get(), renderFinished[resourceIx].get(), fence.get());
		CommonAPI::Present(device.get(), sc.get(), queue, renderFinished[resourceIx].get(), imgnum);
	}

	return 0;
}


// If you see this line of code, i forgot to remove it
// It forces the usage of NVIDIA GPU by OpenGL
extern "C" {  _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001; }