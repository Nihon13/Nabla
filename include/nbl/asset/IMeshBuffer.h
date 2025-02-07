// Copyright (C) 2018-2020 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine".
// For conditions of distribution and use, see copyright notice in nabla.h

#ifndef __NBL_ASSET_I_MESH_BUFFER_H_INCLUDED__
#define __NBL_ASSET_I_MESH_BUFFER_H_INCLUDED__

#include "nbl/asset/IRenderpassIndependentPipeline.h"
#include <algorithm>

namespace nbl
{
namespace asset
{

//! Where to move it so its not floating around scopeless?
enum E_INDEX_TYPE : uint32_t
{
    EIT_16BIT = 0,
    EIT_32BIT,
    EIT_UNKNOWN
};

template <class BufferType, class DescSetType, class PipelineType, class SkeletonType>
class IMeshBuffer : public virtual core::IReferenceCounted
{
    public:
        _NBL_STATIC_INLINE_CONSTEXPR size_t MAX_PUSH_CONSTANT_BYTESIZE = 128u;

        _NBL_STATIC_INLINE_CONSTEXPR size_t MAX_VERTEX_ATTRIB_COUNT = SVertexInputParams::MAX_VERTEX_ATTRIB_COUNT;
        _NBL_STATIC_INLINE_CONSTEXPR size_t MAX_ATTR_BUF_BINDING_COUNT = SVertexInputParams::MAX_ATTR_BUF_BINDING_COUNT;

    protected:
        virtual ~IMeshBuffer() = default;

        alignas(32) core::aabbox3df boundingBox;

        SBufferBinding<BufferType> m_vertexBufferBindings[MAX_ATTR_BUF_BINDING_COUNT];
        SBufferBinding<BufferType> m_indexBufferBinding;

        //! Skin
        SBufferBinding<BufferType> m_inverseBindPoseBufferBinding,m_jointAABBBufferBinding;
        core::smart_refctd_ptr<SkeletonType> m_skeleton;

        //! Descriptor set which goes to set=3
        core::smart_refctd_ptr<DescSetType> m_descriptorSet;

        alignas(64) uint8_t m_pushConstantsData[MAX_PUSH_CONSTANT_BYTESIZE]{};//by putting m_pushConstantsData here, alignas(64) takes no extra place

        //! Pipeline for drawing
        core::smart_refctd_ptr<PipelineType> m_pipeline;

	    // draw params
        uint32_t indexCount;
        uint32_t instanceCount;
	    int32_t baseVertex;
        uint32_t baseInstance;

        // others
        uint32_t maxJointsPerVx : 3;
        uint32_t indexType : 2;

    public:
	    //! Constructor.
	    IMeshBuffer(core::smart_refctd_ptr<PipelineType>&& _pipeline,
            core::smart_refctd_ptr<DescSetType>&& _ds,
            SBufferBinding<BufferType> _vtxBindings[MAX_ATTR_BUF_BINDING_COUNT],
            SBufferBinding<BufferType>&& _indexBinding
        ) : boundingBox(), m_indexBufferBinding(std::move(_indexBinding)),
            m_inverseBindPoseBufferBinding(), m_jointAABBBufferBinding(), m_skeleton(),
            m_descriptorSet(std::move(_ds)), m_pipeline(std::move(_pipeline)),
            indexCount(0u), instanceCount(1u), baseVertex(0), baseInstance(0u),
            maxJointsPerVx(0u), indexType(EIT_UNKNOWN)
	    {
            if (_vtxBindings)
                std::copy(_vtxBindings, _vtxBindings+MAX_ATTR_BUF_BINDING_COUNT, m_vertexBufferBindings);
	    }

        inline bool isAttributeEnabled(uint32_t attrId) const
        {
            if (attrId >= MAX_VERTEX_ATTRIB_COUNT)
                return false;
            if (!m_pipeline)
                return false;

            const auto* ppln = m_pipeline.get();
            const auto& vtxInputParams = ppln->getVertexInputParams();
            if (!(vtxInputParams.enabledAttribFlags & (1u<<attrId)))
                return false;
            return true;
        }
        inline bool isVertexAttribBufferBindingEnabled(uint32_t bndId) const
        {
            if (bndId >= MAX_ATTR_BUF_BINDING_COUNT)
                return false;
            if (!m_pipeline)
                return false;

            const auto* ppln = m_pipeline.get();
            const auto& vtxInputParams = ppln->getVertexInputParams();
            if (!(vtxInputParams.enabledBindingFlags & (1u<<bndId)))
                return false;
            return true;
        }
        //! WARNING: does not check whether attribute and binding are enabled!
        inline uint32_t getBindingNumForAttribute(uint32_t attrId) const
        {
            const auto* ppln = m_pipeline.get();
            const auto& vtxInputParams = ppln->getVertexInputParams();
            return vtxInputParams.attributes[attrId].binding;
        }
        inline E_FORMAT getAttribFormat(uint32_t attrId) const
        {
            const auto* ppln = m_pipeline.get();
            const auto& vtxInputParams = ppln->getVertexInputParams();
            return static_cast<E_FORMAT>(vtxInputParams.attributes[attrId].format);
        }
        inline uint32_t getAttribStride(uint32_t attrId) const
        {
            const auto* ppln = m_pipeline.get();
            const auto& vtxInputParams = ppln->getVertexInputParams();
            const uint32_t bnd = getBindingNumForAttribute(attrId);
            return vtxInputParams.bindings[bnd].stride;
        }
        inline uint32_t getAttribOffset(uint32_t attrId) const
        {
            const auto* ppln = m_pipeline.get();
            const auto& vtxInputParams = ppln->getVertexInputParams();
            return vtxInputParams.attributes[attrId].relativeOffset;
        }
        inline const SBufferBinding<const BufferType>& getAttribBoundBuffer(uint32_t attrId) const
        {
            const uint32_t bnd = getBindingNumForAttribute(attrId);
            return reinterpret_cast<const SBufferBinding<const BufferType>&>(m_vertexBufferBindings[bnd]);
        }
        inline uint64_t getAttribCombinedOffset(uint32_t attrId) const
        {
            const auto& buf = getAttribBoundBuffer(attrId);
            return buf.offset+static_cast<uint64_t>(getAttribOffset(attrId));
        }

        //
        inline const SBufferBinding<const BufferType>* getVertexBufferBindings() const
        {
            return reinterpret_cast<const SBufferBinding<const BufferType>*>(m_vertexBufferBindings);
        }
        inline const SBufferBinding<const BufferType>& getIndexBufferBinding() const
        {
            return reinterpret_cast<const SBufferBinding<const BufferType>&>(m_indexBufferBinding);
        }

        //!
        inline const SBufferBinding<const BufferType>& getInverseBindPoseBufferBinding() const
        {
            return reinterpret_cast<const SBufferBinding<const BufferType>&>(m_inverseBindPoseBufferBinding);
        }
        //!
        inline const SBufferBinding<const BufferType>& getJointAABBBufferBinding() const
        {
            return reinterpret_cast<const SBufferBinding<const BufferType>&>(m_jointAABBBufferBinding);
        }
        //!
        inline const SkeletonType* getSkeleton() const
        {
            return m_skeleton.get();
        }

        //! Returns max joint influences
        inline auto getMaxJointsPerVertex() const { return maxJointsPerVx; }

        //!
        virtual inline bool isSkinned() const
        {
            return  maxJointsPerVx>0u &&
                    m_skeleton.get() &&
                    m_inverseBindPoseBufferBinding.buffer &&
                    m_inverseBindPoseBufferBinding.offset+m_skeleton->getJointCount()*sizeof(core::matrix3x4SIMD)<=m_inverseBindPoseBufferBinding.buffer->getSize() &&
                    m_jointAABBBufferBinding.buffer &&
                    m_jointAABBBufferBinding.offset+m_skeleton->getJointCount()*sizeof(core::aabbox3df)<=m_jointAABBBufferBinding.buffer->getSize();
        }

        //!
        virtual inline bool setSkin(
            SBufferBinding<BufferType>&& _inverseBindPoseBufferBinding,
            SBufferBinding<BufferType>&& _jointAABBBufferBinding,
            core::smart_refctd_ptr<SkeletonType>&& _skeleton,
            const uint32_t _maxJointsPerVx
        )
        {
            if (!_inverseBindPoseBufferBinding.buffer || !_jointAABBBufferBinding.buffer || !_skeleton)
                return false;

            // a very arbitrary constraint
            if (_maxJointsPerVx==0u || _maxJointsPerVx>4u)
                return false;

            const auto jointCount = _skeleton->getJointCount();
            if (jointCount==0u)
                return false;
            if (_inverseBindPoseBufferBinding.offset+jointCount*sizeof(core::matrix3x4SIMD)>_inverseBindPoseBufferBinding.buffer->getSize())
                return false;
            if (_jointAABBBufferBinding.offset+jointCount*sizeof(core::aabbox3df)>_jointAABBBufferBinding.buffer->getSize())
                return false;

            m_inverseBindPoseBufferBinding = std::move(_inverseBindPoseBufferBinding);
            m_jointAABBBufferBinding = std::move(_jointAABBBufferBinding);
            m_skeleton = std::move(_skeleton);
            maxJointsPerVx = _maxJointsPerVx;
            return true;
        }

        //!
        inline const DescSetType* getAttachedDescriptorSet() const
        {
            return m_descriptorSet.get();
        }

        //!
        inline const PipelineType* getPipeline() const
        {
            return m_pipeline.get();
        }

	    //! Get type of index data which is stored in this meshbuffer.
	    /** \return Index type of this buffer. */
	    inline E_INDEX_TYPE getIndexType() const {return static_cast<E_INDEX_TYPE>(indexType);}
	    inline void setIndexType(const E_INDEX_TYPE type)
	    {
		    indexType = type;
	    }

	    //! Get amount of indices in this meshbuffer.
	    /** \return Number of indices in this buffer. */
	    inline auto getIndexCount() const {return indexCount;}
	    //! It sets amount of indices - value that is being passed to glDrawArrays as vertices amount or to glDrawElements as index amount.
	    /** @returns Whether set amount exceeds mapped buffer's size. Regardless of result the amount is set. */
	    inline bool setIndexCount(const uint32_t newIndexCount)
	    {
            indexCount = newIndexCount;
            if (m_indexBufferBinding.buffer)
            {
                switch (indexType)
                {
                    case EIT_16BIT:
                        return indexCount*sizeof(uint16_t)+m_indexBufferBinding.offset < m_indexBufferBinding.buffer->getSize();
                    case EIT_32BIT:
                        return indexCount*sizeof(uint32_t)+m_indexBufferBinding.offset < m_indexBufferBinding.buffer->getSize();
                    default:
                        return false;
                }
            }

            return true;
	    }

	    //! Accesses base vertex number.
	    /** @returns base vertex number. */
        inline int32_t getBaseVertex() const {return baseVertex;}
	    //! Sets base vertex.
        inline void setBaseVertex(const int32_t baseVx)
        {
            baseVertex = baseVx;
        }

	    inline uint32_t getInstanceCount() const {return instanceCount;}
	    inline void setInstanceCount(const uint32_t count)
	    {
		    instanceCount = count;
	    }

	    inline uint32_t getBaseInstance() const {return baseInstance;}
	    inline void setBaseInstance(const uint32_t base)
	    {
		    baseInstance = base;
	    }


	    //! Get the axis aligned bounding box of this meshbuffer.
	    /** \return Axis aligned bounding box of this buffer. */
	    inline const core::aabbox3df& getBoundingBox() const {return boundingBox;}

	    //! Set axis aligned bounding box
	    /** \param box User defined axis aligned bounding box to use
	    for this buffer. */
	    inline virtual void setBoundingBox(const core::aabbox3df& box)
	    {
		    boundingBox = box;
	    }

        uint8_t* getPushConstantsDataPtr() { return m_pushConstantsData; }
        const uint8_t* getPushConstantsDataPtr() const { return m_pushConstantsData; }
};

}}

#endif