#pragma once

#include <array>
#include <render/resource.h>
#include <mlog.h>

namespace Prism
{
    template<typename V, typename I>
    struct Mesh {
    private:
        std::string mesh_name_;
        ResourceManager& mgr_;
        ResourceHandle vb_;
        ResourceHandle ib_;
        D3D12_VERTEX_BUFFER_VIEW vb_view_;
        D3D12_INDEX_BUFFER_VIEW ib_view_;
    public:
        Mesh(const std::string& mesh_name, std::vector<V>& vertices, std::vector<I>& indices, ResourceManager& rm) : mesh_name_(mesh_name), mgr_(rm) {
            std::string vertex_buffer_name = std::format("VertexBuffer_{}", mesh_name_);
            std::string index_buffer_name = std::format("IndexBuffer_{}", mesh_name_);
            vb_ = mgr_.CreateRemoteBuffer(vertex_buffer_name, vertices, D3D12_RESOURCE_STATE_COMMON);
            ib_ = mgr_.CreateRemoteBuffer(index_buffer_name, indices, D3D12_RESOURCE_STATE_COMMON);
            vb_view_ = {
                .BufferLocation = vb_->GetD3D12Resource()->GetGPUVirtualAddress(),
                .SizeInBytes = static_cast<uint32_t>(vb_->GetResourceMeta().Buffer.element_count  * vb_->GetResourceMeta().Buffer.element_size),
                .StrideInBytes = static_cast<uint32_t>(vb_->GetResourceMeta().Buffer.element_size)
            };
            ib_view_ = {
                .BufferLocation = ib_->GetD3D12Resource()->GetGPUVirtualAddress(),
                .SizeInBytes = static_cast<uint32_t>(ib_->GetResourceMeta().Buffer.element_count  * ib_->GetResourceMeta().Buffer.element_size),
                .Format = DXGI_FORMAT_R32_UINT
            };
        }

        Mesh(const Mesh&) = delete;
        Mesh(Mesh&& m) noexcept : mesh_name_(std::move(m.mesh_name_)), mgr_(m.mgr_), vb_(m.vb_), ib_(m.ib_), vb_view_(m.vb_view_), ib_view_(m.ib_view_) {
            m.vb_ = nullptr;
            m.ib_ = nullptr;
        }

        /*
         * At any times, a mesh could have the status by following:
         *      A. Copy process in progressing. (R/W)
         *      B. Render process in progressing. (Readonly)
         * So we only need GPU to wait for copy, and set render waitable to avoid release when inuse.
         *
         */
        void RenderSync(uint64_t fence_value) {
            vb_->GetCopyWaitable().GPUWait();
            ib_->GetCopyWaitable().GPUWait();
            vb_->GetRenderWaitable().GetFenceValue() = fence_value;
            ib_->GetRenderWaitable().GetFenceValue() = fence_value;
        }

        ResourceHandle GetVertexBuffer() const {
            return vb_;
        }

        ResourceHandle GetIndexBuffer() const {
            return ib_;
        }

        const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const {
            return vb_view_;
        }

        const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const {
            return ib_view_;
        }

        ~Mesh() {
            if (vb_ != nullptr) mgr_.MarkAsExpired(vb_);
            if (ib_ != nullptr) mgr_.MarkAsExpired(ib_);
        }
    };
}