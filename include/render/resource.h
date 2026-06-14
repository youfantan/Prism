#pragma once

#include <base.h>
#include <mlog.h>
#include <utils.h>
#include <render/queue.h>
#include <io/texture.h>

#include <unordered_map>
#include <optional>


template<D3D12_DESCRIPTOR_HEAP_TYPE TYPE>
class DescriptorHeap {
protected:
    D3D12_DESCRIPTOR_HEAP_DESC desc_;
    size_t size_;
    size_t elem_;
    ComPtr<ID3D12DescriptorHeap> heap_;
public:
    DescriptorHeap(ComPtr<ID3D12Device>& device, size_t size, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE) : size_(size), desc_() {
        desc_.Type = TYPE;
        desc_.NumDescriptors = size_;
        desc_.Flags = flags;
        device->CreateDescriptorHeap(&desc_, IID_PPV_ARGS(&heap_));
        elem_ = device->GetDescriptorHandleIncrementSize(TYPE);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(size_t i) {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = heap_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += i * elem_;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(size_t i) {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = heap_->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += i * elem_;
        return handle;
    }

    ComPtr<ID3D12DescriptorHeap>& GetComPtr() {
        return heap_;
    }
};

using RTVHeap = DescriptorHeap<D3D12_DESCRIPTOR_HEAP_TYPE_RTV>;
using DSVHeap = DescriptorHeap<D3D12_DESCRIPTOR_HEAP_TYPE_DSV>;

class BindlessHeap : public DescriptorHeap<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV> {
private:
    enum class Type {
        SRV,
        CBV,
        UAV,
    };

    ComPtr<ID3D12Device>& device_;
    uint32_t srv_scope_;
    uint32_t cbv_scope_;
    uint32_t uav_scope_;
    std::unordered_map<std::string_view, uint32_t> heap_mapping_;
    std::vector<bool> resident_;
    D3D12_DESCRIPTOR_RANGE ranges_[3];

    int32_t AssignIndex(Type type);
public:
    BindlessHeap(ComPtr<ID3D12Device>& device, uint32_t srv_scope, uint32_t cbv_scope, uint32_t uav_scope);
    std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> BindShaderResource(std::string_view name, Resource<ResourceType::Texture>& res);
    std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> BindConstantBuffer(std::string_view name, Resource<ResourceType::ConstBuffer>& res);
    int32_t QueryResourceIndex(std::string_view name);
    bool Unbind(std::string_view name);

    D3D12_DESCRIPTOR_RANGE* GetDescriptorRange() {
        return ranges_;
    }
};

template<ResourceType RT>
class Resource {
    template<typename Allocator>
    requires is_allocator<Allocator>
    friend class ResourceManager;
private:
    union resource_view {
        D3D12_CPU_DESCRIPTOR_HANDLE handle;
        D3D12_VERTEX_BUFFER_VIEW vbv;
        D3D12_INDEX_BUFFER_VIEW ibv;
    };

    ComPtr<ID3D12Resource> resource_;
    resource_view view_;
    Waitable waitable_;
    D3D12_RESOURCE_STATES states_;
    void* mapping_ptr_;

    Resource(ComPtr<ID3D12Resource>& resource, Waitable&& empty) : resource_(resource), waitable_(std::move(empty)) {
        if constexpr (RT == ResourceType::ConstBuffer) {
            resource_->Map(0, nullptr, &mapping_ptr_);
        }
    }
public:
    Resource(const Resource&) = delete;
    Resource(Resource&& r) noexcept : resource_(std::move(r.resource_)), view_(r.view_), waitable_(std::move(r.waitable_)), states_(r.states_), mapping_ptr_(r.mapping_ptr_) {
        r.mapping_ptr_ = nullptr;
        memset(&r.view_, 0, sizeof(resource_view));
    }

    ComPtr<ID3D12Resource>& GetComPtr() {
#ifndef NDEBUG
        if (resource_ == nullptr) {
            LFATAL("Resource is nullptr");
        }
#endif
        return resource_;
    }

    Waitable& GetWaitable() {
        return waitable_;
    }

    auto& GetView() {
        if constexpr (RT == ResourceType::VertexBuffer) {
            return view_.vbv;
        } else if constexpr (RT == ResourceType::IndexBuffer) {
            return view_.ibv;
        } else {
            return view_.handle;
        }
    }

    void Transition(D3D12_RESOURCE_STATES new_state, ComPtr<ID3D12GraphicsCommandList>& list) {
        D3D12_RESOURCE_BARRIER barrier {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        D3D12_RESOURCE_TRANSITION_BARRIER transition {};
        barrier.Transition.pResource = resource_.Get();
        barrier.Transition.StateBefore = states_;
        barrier.Transition.StateAfter = new_state;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        list->ResourceBarrier(1, &barrier);
        states_ = new_state;
    }

    template<typename T>
    requires (RT == ResourceType::ConstBuffer)
    T* GetMapping() {
        return reinterpret_cast<T*>(mapping_ptr_);
    }

    ~Resource() {
        if constexpr (RT == ResourceType::ConstBuffer) {
            if (mapping_ptr_ != nullptr) {
                resource_->Unmap(0, nullptr);
            }
        }
        if (resource_ != nullptr) {
            waitable_.CPUWait();
        }
    }
};

template<typename Allocator>
requires is_allocator<Allocator>
class ResourceManager {
private:
    ComPtr<ID3D12Device>& device_;
    Allocator& allocator_;
    CopyQueue& copy_queue_;
public:
    ResourceManager(ComPtr<ID3D12Device>& device, Allocator& allocator, CopyQueue& copy_queue) : device_(device), allocator_(allocator), copy_queue_(copy_queue) {

    }

    UploadBuffer CreateUploadBuffer(uint64_t size) {
        auto desc = UploadBuffer::GetDesc(size);
        ComPtr<ID3D12Resource> ub = allocator_.CreateLocalResource(desc);
        return { ub };
    }

    Resource<ResourceType::RenderTarget> CreateRenderTarget(DXGI_FORMAT rt_fmt, uint32_t width, uint32_t height, MSAAType type, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
        D3D12_RESOURCE_DESC desc {};
        desc.Alignment = 0;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = 1;
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Format = rt_fmt;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = GetSampleCount(type);
        desc.SampleDesc.Quality = 0;
        ComPtr<ID3D12Resource> resource = allocator_.CreateRemoteResource(desc);
        Resource<ResourceType::RenderTarget> rt(resource, Waitable(copy_queue_.GetCopyFence(), 0));
        rt.view_.handle = handle;
        rt.states_ = D3D12_RESOURCE_STATE_COMMON;
        return rt;
    }

    Resource<ResourceType::DepthBuffer> CreateDepthBuffer(uint32_t width, uint32_t height, MSAAType type, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
        D3D12_RESOURCE_DESC desc {};
        desc.Alignment = 0;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = 1;
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = GetSampleCount(type);
        desc.SampleDesc.Quality = 0;
        D3D12_CLEAR_VALUE optclr {};
        optclr.Format = DXGI_FORMAT_D32_FLOAT;
        optclr.DepthStencil.Depth = 1.0f;
        optclr.DepthStencil.Stencil = 0;
        ComPtr<ID3D12Resource> resource = allocator_.CreateRemoteResource(desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optclr);
        Resource<ResourceType::DepthBuffer> db(resource, Waitable(copy_queue_.GetCopyFence(), 0));
        db.view_.handle = handle;
        db.states_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        return db;
    }

    template<typename V>
    Resource<ResourceType::VertexBuffer> CreateVertexBuffer(const V* vertices, size_t vertices_count) {
        size_t size = vertices_count * sizeof(V);
        UploadBuffer upload_buffer(allocator_.CreateLocalResource(UploadBuffer::GetDesc(size)));
        upload_buffer.Write(vertices, size);
        D3D12_RESOURCE_DESC desc {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        ComPtr<ID3D12Resource> resource = allocator_.CreateRemoteResource(desc);
        Resource<ResourceType::VertexBuffer> vb(resource, Waitable(copy_queue_.GetCopyFence(), 0));
        vb.states_ = D3D12_RESOURCE_STATE_COMMON;
        copy_queue_.CopyBuffer(vb, upload_buffer);
        vb.view_.vbv.BufferLocation = resource->GetGPUVirtualAddress();
        vb.view_.vbv.SizeInBytes = vertices_count * sizeof(V);
        vb.view_.vbv.StrideInBytes = sizeof(V);
        return vb;
    }

    Resource<ResourceType::IndexBuffer> CreateIndexBuffer(const uint32_t* indices, size_t indices_count) {
        size_t size = indices_count * sizeof(uint32_t);
        UploadBuffer upload_buffer(allocator_.CreateLocalResource(UploadBuffer::GetDesc(size)));
        D3D12_RESOURCE_DESC desc {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        upload_buffer.Write(indices, size);
        ComPtr<ID3D12Resource> resource = allocator_.CreateRemoteResource(desc);
        Resource<ResourceType::IndexBuffer> ib(resource, Waitable(copy_queue_.GetCopyFence(), 0));
        ib.states_ = D3D12_RESOURCE_STATE_COMMON;
        copy_queue_.CopyBuffer(ib, upload_buffer);
        ib.view_.ibv.BufferLocation = resource->GetGPUVirtualAddress();
        ib.view_.ibv.SizeInBytes = indices_count * sizeof(uint32_t);
        ib.view_.ibv.Format = DXGI_FORMAT_R32_UINT;
        return ib;
    }

    template<typename C>
    Resource<ResourceType::ConstBuffer> CreateConstantBuffer() {
        D3D12_RESOURCE_DESC desc {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = AlignV<256>(sizeof(C));
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        ComPtr<ID3D12Resource> constant_buffer = allocator_.CreateLocalResource(desc);
        return { constant_buffer, Waitable(copy_queue_.GetCopyFence(), 0) };
    }

    Resource<ResourceType::Texture> CreateTexture(const TextureLoader::texture_in_memory_t& texture) {
        D3D12_RESOURCE_DESC tex_desc {};
        D3D12_RESOURCE_DESC buf_desc {};
        tex_desc.Width = texture.width;
        tex_desc.Height = texture.height;
        tex_desc.Format = texture.format;
        tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        tex_desc.MipLevels = 1;
        tex_desc.DepthOrArraySize = 1;
        tex_desc.SampleDesc.Count = 1;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
        uint32_t rows;
        uint64_t upload_size, total_size;
        device_->GetCopyableFootprints(&tex_desc, 0, 1, 0, &footprint, &rows, &upload_size, &total_size);
        UploadBuffer upload_buffer(allocator_.CreateLocalResource(UploadBuffer::GetDesc(footprint.Footprint.RowPitch * rows)));
        void* upload_buffer_mapping;
        CHECKHR(upload_buffer.GetComPtr()->Map(0, nullptr, &upload_buffer_mapping));
        auto* dest = static_cast<uint8_t *>(upload_buffer_mapping) + footprint.Offset;
        uint64_t dest_pitch = footprint.Footprint.RowPitch;
        for (int i = 0; i < rows; ++i) {
            memcpy(dest + i * dest_pitch, texture.data + i * texture.row_pitch, texture.row_pitch);
        }
        upload_buffer.GetComPtr()->Unmap(0, nullptr);
        ComPtr<ID3D12Resource> resource = allocator_.CreateRemoteResource(tex_desc);
        Resource<ResourceType::Texture> tex(resource, Waitable(copy_queue_.GetCopyFence(), 0));
        tex.states_ = D3D12_RESOURCE_STATE_COMMON;
        copy_queue_.CopyTexture(tex, upload_buffer, footprint);
        return tex;
    }

    template<ResourceType RT>
    Resource<RT> CreateFromExistsPtr(ComPtr<ID3D12Resource> resource, D3D12_CPU_DESCRIPTOR_HANDLE handle = {}, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON) {
        Resource<RT> res(resource, Waitable(copy_queue_.GetCopyFence(), 0));
        res.view_.handle = handle;
        res.states_ = state;
        return res;
    }

};

class DXDefaultAllocator {
private:
    ComPtr<ID3D12Device>& device_;
public:
    using init_t = struct {};
    DXDefaultAllocator(ComPtr<ID3D12Device>& device, const init_t& init) : device_(device) {

    }

    ComPtr<ID3D12Resource> CreateLocalResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON, D3D12_CLEAR_VALUE* pclr = nullptr) {
        D3D12_HEAP_PROPERTIES prop {};
        prop.Type = D3D12_HEAP_TYPE_UPLOAD;
        ComPtr<ID3D12Resource> resource;
        device_->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc, state, pclr, IID_PPV_ARGS(&resource));
        return resource;
    }

    ComPtr<ID3D12Resource> CreateRemoteResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON, D3D12_CLEAR_VALUE* pclr = nullptr) {
        D3D12_HEAP_PROPERTIES prop {};
        prop.Type = D3D12_HEAP_TYPE_DEFAULT;
        ComPtr<ID3D12Resource> resource;
        device_->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc, state, pclr, IID_PPV_ARGS(&resource));
        return resource;
    }

};