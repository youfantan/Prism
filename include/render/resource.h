#pragma once

#include <base.h>
#include <mlog.h>
#include <utils.h>
#include <render/queue.h>
#include <io/texture.h>

#include <unordered_map>
#include <optional>

#include "io/image.h"

class Resource {
protected:
    ResourceType type_;
    std::string name_;
    ComPtr<ID3D12Resource> resource_;
    Waitable waitable_;
    D3D12_RESOURCE_STATES states_;
    DXAllocator* allocator_;
    Resource(ResourceType type, std::string&& name, ComPtr<ID3D12Resource> resource, DXAllocator* allocator, Waitable&& empty, D3D12_RESOURCE_STATES states) : type_(type), name_(std::move(name)), resource_(resource), allocator_(allocator), waitable_(std::move(empty)), states_(states) {
        resource_->SetName(ConvertStringToWstring(name_.c_str()).c_str());
    }
    Resource(const Resource&) = delete;
    Resource(Resource&& r) noexcept : type_(r.type_), resource_(std::move(r.resource_)), waitable_(std::move(r.waitable_)), states_(r.states_), allocator_(r.allocator_) {

    }
public:
    ResourceType GetResourceType() {
        return type_;
    }

    const std::string& GetName() {
        return name_;
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
    requires std::derived_from<T, Resource>
    T* GetAs() {
        if (type_ != T::type) {
            return nullptr;
        }
        return reinterpret_cast<T*>(this);
    }

    ComPtr<ID3D12Resource> GetComPtr() {
        return resource_;
    }

    Waitable& GetWaitable() {
        return waitable_;
    }

    virtual ~Resource() {
        if (resource_ != nullptr) {
            waitable_.CPUWait();
            allocator_->FreeResource(resource_);
        }
    }
};

struct ResourceView {
    ResourceViewType type;
    union {
        D3D12_VERTEX_BUFFER_VIEW vb_view;
        D3D12_INDEX_BUFFER_VIEW ib_view;
        D3D12_CPU_DESCRIPTOR_HANDLE handle;
    } data;
};

class ConstantBuffer : public Resource {
private:
    void* mapping_;
public:
    constexpr static ResourceType Type = ResourceType::ConstBuffer;

    ConstantBuffer(std::string name, DXAllocator* allocator, ComPtr<ID3D12Resource> resource, Waitable&& empty) : Resource(Type, std::move(name), std::move(resource), allocator, std::move(empty), D3D12_RESOURCE_STATE_COMMON), mapping_(nullptr) {
        resource_->Map(0, nullptr, &mapping_);
    }
    ConstantBuffer(const ConstantBuffer&) = delete;
    ConstantBuffer(ConstantBuffer&& cb) noexcept : Resource(std::move(*this)), mapping_(cb.mapping_) {
        cb.mapping_ = nullptr;
    }

    template<typename T>
    T* GetMapping() {
        return static_cast<T*>(mapping_);
    }

    ~ConstantBuffer() override {
        if (mapping_ != nullptr) {
            resource_->Unmap(0, nullptr);
        }
    }
};

class StructuredBuffer : public Resource {
private:
    CopyQueue* queue_;
public:
    constexpr static ResourceType Type = ResourceType::ConstBuffer;

    StructuredBuffer(std::string name, DXAllocator* allocator, CopyQueue& queue, ComPtr<ID3D12Resource> resource) : Resource(Type, std::move(name), std::move(resource), allocator, Waitable(queue.GetCopyFence(), 0), D3D12_RESOURCE_STATE_COMMON), queue_(&queue) {

    }
    StructuredBuffer(const StructuredBuffer&) = delete;
    StructuredBuffer(StructuredBuffer&& sb) noexcept : Resource(std::move(*this)), queue_(sb.queue_) {
        sb.queue_ = nullptr;
    }

    void CopyToBuffer(UploadBuffer& ub) {
        queue_->CopyBuffer(this, ub);
    }
};

class Texture : public Resource {
private:
    CopyQueue* queue_;
public:
    constexpr static ResourceType Type = ResourceType::Texture;

    Texture(std::string name, DXAllocator* allocator, CopyQueue& queue, ComPtr<ID3D12Resource> resource) : Resource(Type, std::move(name), std::move(resource), allocator, Waitable(queue.GetCopyFence(), 0), D3D12_RESOURCE_STATE_COMMON), queue_(&queue) {

    }

    void CopyToTexture(UploadBuffer& tex, D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint) {
        queue_->CopyTexture(this, tex, footprint);
    }
};

class RenderTarget : public Resource {
private:
public:
    constexpr static ResourceType Type = ResourceType::RenderTarget;

    RenderTarget(std::string name, DXAllocator* allocator, ComPtr<ID3D12Resource> resource, Waitable&& empty) : Resource(Type, std::move(name), std::move(resource), allocator, std::move(empty), D3D12_RESOURCE_STATE_COMMON) {

    }
};

class DepthBuffer : public Resource {
private:
public:
    constexpr static ResourceType Type = ResourceType::DepthBuffer;

    DepthBuffer(std::string name, DXAllocator* allocator, ComPtr<ID3D12Resource> resource, Waitable&& empty) : Resource(Type, std::move(name), std::move(resource), allocator, std::move(empty), D3D12_RESOURCE_STATE_DEPTH_WRITE) {

    }
};

class VertexBuffer : public Resource {
private:
    CopyQueue* queue_;
public:
    constexpr static ResourceType Type = ResourceType::VertexBuffer;

    VertexBuffer(std::string name, DXAllocator* allocator, CopyQueue& queue, ComPtr<ID3D12Resource> resource) : Resource(Type, std::move(name), std::move(resource), allocator, Waitable(queue.GetCopyFence(), 0), D3D12_RESOURCE_STATE_COMMON), queue_(&queue) {

    }

    void CopyToBuffer(UploadBuffer& ub) {
        queue_->CopyBuffer(this, ub);
    }
};

class IndexBuffer : public Resource {
private:
    CopyQueue* queue_;
public:
    constexpr static ResourceType Type = ResourceType::IndexBuffer;

    IndexBuffer(std::string name, DXAllocator* allocator, CopyQueue& queue, ComPtr<ID3D12Resource> resource) : Resource(Type, std::move(name), std::move(resource), allocator, Waitable(queue.GetCopyFence(), 0), D3D12_RESOURCE_STATE_COMMON), queue_(&queue) {

    }

    void CopyToBuffer(UploadBuffer& ub) {
        queue_->CopyBuffer(this, ub);
    }

};

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

class ResourceMap {
private:
    struct ResourceHandle {
        Resource* ptr;
        std::unordered_map<std::string, ResourceView> views;
    };

    DXAllocator* allocator_;
    std::unordered_map<std::string, ResourceHandle> resources_;
public:
    ResourceMap(DXAllocator* allocator) : allocator_(allocator) {}

    template<typename T, typename... Args>
    requires std::derived_from<T, Resource>
    bool CreateResource(std::string name, Args&&... args) {
        if (resources_.contains(name)) {
            LFATAL("Failed to create resource {}; resource already exists.", name);
            return false;
        }
        T* ptr = new T(name, allocator_, std::forward<Args>(args)...);
        ResourceHandle handle {};
        handle.ptr = ptr;
        resources_[name] = handle;
        return true;
    }

    bool BindResourceView(const std::string& resource_name, std::string view_name, const ResourceView& rv) {
        if (!resources_.contains(resource_name)) {
            LFATAL("Failed to bind resource view {}::{}; resource not exists.", resource_name, view_name);
            return false;
        }
        auto& views = resources_[resource_name].views;
        if (views.contains(view_name)) {
            LFATAL("Failed to bind resource view {}::{}; resource view already exists.", resource_name, view_name);
            return false;
        }
        views[view_name] = rv;
        return true;
    }

    template<typename T>
    requires std::derived_from<T, Resource>
    std::optional<T*> QueryResource(const std::string& name) {
        if (!resources_.contains(name)) {
            LFATAL("Failed to query resource {}; resource not exists.", name);
            return std::nullopt;
        }
        auto* res = resources_[name].ptr;
        if (res->GetResourceType() != T::Type) {
            LFATAL("Failed to query resource {}; target resource type {} not equals to queried resource type {}", name, static_cast<int>(T::Type), static_cast<int>(res->GetResourceType()));
            return std::nullopt;
        }
        return reinterpret_cast<T*>(res);
    }

    std::optional<ResourceView*> QueryResourceView(const std::string& resource_name, const std::string& view_name) {
        if (!resources_.contains(resource_name)) {
            LFATAL("Failed to query resource view {}::{}; resource not exists.", resource_name, view_name);
            return std::nullopt;
        }
        auto& views = resources_[resource_name].views;
        if (!views.contains(view_name)) {
            LFATAL("Failed to query resource view {}::{}; resource view not exists.", resource_name, view_name);
            return std::nullopt;
        }
        return &views[view_name];
    }

    bool RemoveResource(const std::string& name) {
        if (!resources_.contains(name)) {
            LFATAL("Failed to remove resource {}; resource not exists.", name);
            return false;
        }
        auto& handle = resources_[name];
        delete handle.ptr;
        resources_.erase(name);
        return true;
    }

    ~ResourceMap() {
        for (auto& resource : resources_) {
            resource.second.views.clear();
            delete resource.second.ptr;
        }
    }
};

class ResourceManager {
private:
    ComPtr<ID3D12Device> device_;
    DXAllocator* allocator_;
    CopyQueue& copy_queue_;
    ResourceMap map_;
public:
    ResourceManager(ComPtr<ID3D12Device> device, DXAllocator* allocator, CopyQueue& copy_queue) : device_(device), allocator_(allocator), copy_queue_(copy_queue), map_(allocator) {

    }

    UploadBuffer CreateUploadBuffer(uint64_t size) {
        auto desc = UploadBuffer::GetDesc(size);
        ComPtr<ID3D12Resource> ub = allocator_->CreateLocalResource(desc);
        return { ub };
    }

    std::optional<RenderTarget*> CreateRenderTarget(const std::string& name, DXGI_FORMAT rt_fmt, uint32_t width, uint32_t height, MSAAType type) {
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
        ComPtr<ID3D12Resource> resource = allocator_->CreateRemoteResource(desc);
        if (!map_.CreateResource<RenderTarget>(name, resource, Waitable(copy_queue_.GetCopyFence(), 0))) return std::nullopt;
        return map_.QueryResource<RenderTarget>(name);
    }

    std::optional<RenderTarget*> CreateRenderTarget(const std::string& name, ComPtr<ID3D12Resource>& resource) {
        if (!map_.CreateResource<RenderTarget>(name, resource, Waitable(copy_queue_.GetCopyFence(), 0))) return std::nullopt;
        return map_.QueryResource<RenderTarget>(name);
    }

    std::optional<DepthBuffer*> CreateDepthBuffer(const std::string& name, uint32_t width, uint32_t height, MSAAType type) {
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
        ComPtr<ID3D12Resource> resource = allocator_->CreateRemoteResource(desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optclr);
        if (!map_.CreateResource<DepthBuffer>(name, resource, Waitable(copy_queue_.GetCopyFence(), 0))) return std::nullopt;
        return map_.QueryResource<DepthBuffer>(name);
    }

    template<typename V>
    std::optional<VertexBuffer*> CreateVertexBuffer(const std::string& name, const V* vertices, size_t vertices_count) {
        size_t size = vertices_count * sizeof(V);
        UploadBuffer upload_buffer(allocator_->CreateLocalResource(UploadBuffer::GetDesc(size)));
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
        ComPtr<ID3D12Resource> resource = allocator_->CreateRemoteResource(desc);
        if (!map_.CreateResource<VertexBuffer>(name, copy_queue_, resource)) return std::nullopt;
        auto query = map_.QueryResource<VertexBuffer>(name);
        query.value()->CopyToBuffer(upload_buffer);
        ResourceView view {};
        view.type = ResourceViewType::VBV;
        view.data.vb_view.BufferLocation = resource->GetGPUVirtualAddress();
        view.data.vb_view.SizeInBytes = vertices_count * sizeof(V);
        view.data.vb_view.StrideInBytes = sizeof(V);
        map_.BindResourceView(name, "default_vb_view", view);
        return query;
    }

    std::optional<IndexBuffer*> CreateIndexBuffer(const std::string& name, const uint32_t* indices, size_t indices_count) {
        size_t size = indices_count * sizeof(uint32_t);
        UploadBuffer upload_buffer(allocator_->CreateLocalResource(UploadBuffer::GetDesc(size)));
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
        ComPtr<ID3D12Resource> resource = allocator_->CreateRemoteResource(desc);
        if (!map_.CreateResource<IndexBuffer>(name, copy_queue_, resource)) return std::nullopt;
        auto query = map_.QueryResource<IndexBuffer>(name);
        query.value()->CopyToBuffer(upload_buffer);
        ResourceView view {};
        view.type = ResourceViewType::IBV;
        view.data.ib_view.BufferLocation = resource->GetGPUVirtualAddress();
        view.data.ib_view.SizeInBytes = indices_count * sizeof(uint32_t);
        view.data.ib_view.Format = DXGI_FORMAT_R32_UINT;
        map_.BindResourceView(name, "default_ib_view", view);
        return query;
    }

    template<typename S>
    std::optional<StructuredBuffer*> CreateStructuredBuffer(const std::string& name, const S* data, size_t count) {
        size_t size = count * sizeof(S);
        UploadBuffer upload_buffer(allocator_->CreateLocalResource(UploadBuffer::GetDesc(size)));
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
        upload_buffer.Write(data, size);
        ComPtr<ID3D12Resource> resource = allocator_->CreateRemoteResource(desc);
        if (!map_.CreateResource<StructuredBuffer>(name, copy_queue_, resource)) return std::nullopt;
        auto query = map_.QueryResource<StructuredBuffer>(name);
        query.value()->CopyToBuffer(upload_buffer);
        return query.value();
    }

    template<typename S>
    std::optional<StructuredBuffer*> CreateStructuredBuffer(const std::string& name, std::vector<S>& data) {
        return CreateStructuredBuffer(name, &data[0], data.size());
    }

    template<typename C>
    std::optional<ConstantBuffer*> CreateConstantBuffer(const std::string& name) {
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
        ComPtr<ID3D12Resource> constant_buffer = allocator_->CreateLocalResource(desc);
        if (!map_.CreateResource<ConstantBuffer>(name, constant_buffer, Waitable(copy_queue_.GetCopyFence(), 0))) {
            LERROR("Cannot create constant buffer {}", name);
            return std::nullopt;
        }
        return map_.QueryResource<ConstantBuffer>(name);
    }


    std::optional<Texture*> CreateTexture(const std::string& name, const TextureLoader::texture_in_memory_t& texture) {
        D3D12_RESOURCE_DESC tex_desc {};
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
        UploadBuffer upload_buffer(allocator_->CreateLocalResource(UploadBuffer::GetDesc(footprint.Footprint.RowPitch * rows)));
        void* upload_buffer_mapping;
        CHECKHR(upload_buffer.GetComPtr()->Map(0, nullptr, &upload_buffer_mapping));
        auto* dest = static_cast<uint8_t *>(upload_buffer_mapping) + footprint.Offset;
        uint64_t dest_pitch = footprint.Footprint.RowPitch;
        for (int i = 0; i < rows; ++i) {
            memcpy(dest + i * dest_pitch, texture.data + i * texture.row_pitch, texture.row_pitch);
        }
        upload_buffer.GetComPtr()->Unmap(0, nullptr);
        ComPtr<ID3D12Resource> resource = allocator_->CreateRemoteResource(tex_desc);
        if (!map_.CreateResource<Texture>(name, copy_queue_, resource)) return std::nullopt;
        auto query = map_.QueryResource<Texture>(name);
        query.value()->CopyToTexture(upload_buffer, footprint);
        return query;
    }

    template<typename ImageFormat>
    requires is_image_format<ImageFormat>
    std::optional<Texture*> CreateTextureFromImage(const std::string& name, const Image<ImageFormat>& image) {
        D3D12_RESOURCE_DESC tex_desc {};
        tex_desc.Width = image.width;
        tex_desc.Height = image.height;
        tex_desc.Format = ImageFormat::DXGIFormat;
        tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        tex_desc.MipLevels = 1;
        tex_desc.DepthOrArraySize = 1;
        tex_desc.SampleDesc.Count = 1;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
        uint32_t rows;
        uint64_t upload_size, total_size;
        device_->GetCopyableFootprints(&tex_desc, 0, 1, 0, &footprint, &rows, &upload_size, &total_size);
        UploadBuffer upload_buffer(allocator_->CreateLocalResource(UploadBuffer::GetDesc(footprint.Footprint.RowPitch * rows)));
        void* upload_buffer_mapping;
        CHECKHR(upload_buffer.GetComPtr()->Map(0, nullptr, &upload_buffer_mapping));
        auto* dest = static_cast<uint8_t *>(upload_buffer_mapping) + footprint.Offset;
        uint64_t dest_pitch = footprint.Footprint.RowPitch;
        for (int i = 0; i < rows; ++i) {
            memcpy(dest + i * dest_pitch, &image.At(0, i), image.width * image.stride);
        }
        upload_buffer.GetComPtr()->Unmap(0, nullptr);
        ComPtr<ID3D12Resource> resource = allocator_->CreateRemoteResource(tex_desc);
        if (!map_.CreateResource<Texture>(name, copy_queue_, resource)) return std::nullopt;
        auto query = map_.QueryResource<Texture>(name);
        query.value()->CopyToTexture(upload_buffer, footprint);
        return query;
    }

    ResourceMap& GetMap() {
        return map_;
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
    ResourceManager& res_mgr_;
    uint32_t srv_scope_;
    uint32_t cbv_scope_;
    uint32_t uav_scope_;
    std::unordered_map<std::string, uint32_t> heap_mapping_;
    std::vector<bool> resident_;
    D3D12_DESCRIPTOR_RANGE ranges_[3];

    int32_t AssignIndex(Type type);
public:
    BindlessHeap(ComPtr<ID3D12Device>& device, ResourceManager& res_mgr, uint32_t srv_scope, uint32_t cbv_scope, uint32_t uav_scope);
    std::optional<std::string> BindTexture(const std::string& name);
    std::optional<std::string> BindStructuredBuffer(const std::string& name);
    std::optional<std::string> BindConstantBuffer(const std::string& name);
    int32_t QueryResourceIndex(const std::string& name);
    bool Unbind(const std::string& name);

    D3D12_DESCRIPTOR_RANGE* GetDescriptorRange() {
        return ranges_;
    }
};

class DXDefaultAllocator : public DXAllocator {
public:
    using init_t = struct {};
    DXDefaultAllocator(ComPtr<ID3D12Device> device) : DXAllocator(device) {

    }

    ComPtr<ID3D12Resource> CreateLocalResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON, D3D12_CLEAR_VALUE* pclr = nullptr) override {
        D3D12_HEAP_PROPERTIES prop {};
        prop.Type = D3D12_HEAP_TYPE_UPLOAD;
        ComPtr<ID3D12Resource> resource;
        device_->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc, state, pclr, IID_PPV_ARGS(&resource));
        return resource;
    }

    ComPtr<ID3D12Resource> CreateRemoteResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON, D3D12_CLEAR_VALUE* pclr = nullptr) override {
        D3D12_HEAP_PROPERTIES prop {};
        prop.Type = D3D12_HEAP_TYPE_DEFAULT;
        ComPtr<ID3D12Resource> resource;
        device_->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc, state, pclr, IID_PPV_ARGS(&resource));
        return resource;
    }

    void FreeResource(ComPtr<ID3D12Resource> resource) override {
        UINT size = 0;
        resource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, nullptr);
        std::wstring name;
        if (size == 0) {
            name = L"Unnamed Object";
        } else {
            name.resize(size / sizeof(wchar_t));
            resource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, &name[0]);
        }
        LDEBUG("Resource {} is released", ConvertWstringToString(name));
    }
};