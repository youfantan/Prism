#pragma once

#include <base.h>
#include <functional>
#include <render/sync.h>
#include <optional>
#include <vector>

class UploadBuffer {
    template<typename Allocator>
    requires is_allocator<Allocator>
    friend class ResourceManager;
private:
    uint64_t fence_value_;
    size_t size_;
    ComPtr<ID3D12Resource> resource_;

    constexpr static D3D12_RESOURCE_DESC GetDesc(uint64_t size) {
        D3D12_RESOURCE_DESC upload_desc{};
        upload_desc.Alignment = 0;
        upload_desc.DepthOrArraySize = 1;
        upload_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        upload_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        upload_desc.Format = DXGI_FORMAT_UNKNOWN;
        upload_desc.Width = size;
        upload_desc.Height = 1;
        upload_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        upload_desc.MipLevels = 1;
        upload_desc.SampleDesc.Count = 1;
        upload_desc.SampleDesc.Quality = 0;
        return upload_desc;
    };

    UploadBuffer(const ComPtr<ID3D12Resource>& resource) : resource_(resource), size_(resource->GetDesc().Width), fence_value_(0) {}
public:
    UploadBuffer(const UploadBuffer&) = delete;
    UploadBuffer(UploadBuffer&& ub) noexcept : fence_value_(ub.fence_value_), size_(ub.size_), resource_(ub.resource_) {
        ub.fence_value_ = 0;
        ub.size_ = 0;
        ub.resource_ = nullptr;
    }
    UploadBuffer& operator=(UploadBuffer&& ub) noexcept {
        fence_value_ = ub.fence_value_;
        size_ = ub.size_;
        ub.resource_ = std::move(ub.resource_);
        ub.fence_value_ = 0;
        ub.size_ = 0;
        return *this;
    }

    ComPtr<ID3D12Resource>& GetComPtr() {
        return resource_;
    }

    uint64_t& GetFenceValue() {
        return fence_value_;
    }

    bool Write(const void* data, size_t size) {
        if (resource_->GetDesc().Width != size) return false;
        void* dest;
        HRESULT hr = resource_->Map(0, nullptr, &dest);
        if (!SUCCEEDED(hr)) return false;
        memcpy(dest, data, size);
        resource_->Unmap(0, nullptr);
        return true;
    }

    bool Read(void* dest, size_t size) {
        if (resource_->GetDesc().Width != size) return false;
        void* data;
        HRESULT hr = resource_->Map(0, nullptr, &data);
        if (!SUCCEEDED(hr)) return false;
        memcpy(dest, data, size);
        resource_->Unmap(0, nullptr);
        return true;
    }

    ~UploadBuffer() {
        if (resource_.Get() != nullptr) {
            resource_.Reset();
        }
    }
};

class RenderQueue {
public:
    using queue_worker_t = struct {
        ComPtr<ID3D12GraphicsCommandList> list;
        ComPtr<ID3D12CommandAllocator> alloc;
        Waitable waitable;
        bool dispatched;
    };
private:
    ComPtr<ID3D12Device>& device_;
    ComPtr<ID3D12CommandQueue> queue_;
    std::vector<queue_worker_t> workers_;
    Fence fence_;
public:
    RenderQueue(ComPtr<ID3D12Device>& device, uint32_t n);

    ComPtr<ID3D12GraphicsCommandList>& PrepareRenderQueue(uint32_t index);
    uint64_t CommitRenderQueue(uint32_t index);

    Fence& GetRenderFence() {
        return fence_;
    }

    ComPtr<ID3D12CommandQueue>& GetComPtr() {
        return queue_;
    }

    ~RenderQueue();
};

class CopyQueue {
public:
    using queue_worker_t = struct {
        ComPtr<ID3D12GraphicsCommandList> list;
        ComPtr<ID3D12CommandAllocator> alloc;
        Waitable waitable;
        bool dispatched;
    };
private:
    ComPtr<ID3D12Device>& device_;
    ComPtr<ID3D12CommandQueue> queue_;
    std::vector<queue_worker_t> workers_;
    Fence fence_;
    std::vector<UploadBuffer> upload_buffers_;
    queue_worker_t& AssignWorker();
public:
    CopyQueue(ComPtr<ID3D12Device>& device, size_t n);

    Fence& GetCopyFence() {
        return fence_;
    }

    template<ResourceType RT>
    void CopyBuffer(Resource<RT>& dest, UploadBuffer& src) {
        auto& worker = AssignWorker();
        worker.waitable.CPUWait();
        worker.alloc->Reset();
        worker.list->Reset(worker.alloc.Get(), nullptr);
        worker.list->CopyResource(dest.GetComPtr().Get(), src.GetComPtr().Get());
        worker.list->Close();
        ID3D12CommandList* lists[] = { worker.list.Get() };
        dest.GetWaitable().GPUWait(queue_);
        queue_->ExecuteCommandLists(1, lists);
        worker.waitable = fence_.AllocateWaitable();
        dest.GetWaitable() = Waitable(fence_, worker.waitable.GetFenceValue());
        fence_.GPUSync(worker.waitable, queue_);
        src.GetFenceValue() = worker.waitable.GetFenceValue();
        upload_buffers_.push_back(std::move(src));
    }

    void CopyTexture(Resource<ResourceType::Texture>& dest, UploadBuffer& src, D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint);

    ComPtr<ID3D12CommandQueue>& GetComPtr() {
        return queue_;
    }

    void DeferredRelease(bool final_release = false);
    ~CopyQueue();
};
