#pragma once

#include <base.h>

#include <mlog.h>
#include <utils.h>

#include <render/queue.h>
#include <render/sync.h>

namespace Prism
{
    class DXDefaultAllocator : public DXAllocator {
    public:

        explicit DXDefaultAllocator(const ComPtr<ID3D12Device>& device) : DXAllocator(device) {

        }

        ID3D12Resource* CreateLocalResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES states, D3D12_CLEAR_VALUE* pclr) override {
            ID3D12Resource* res;
            D3D12_HEAP_PROPERTIES prop {
                .Type = D3D12_HEAP_TYPE_UPLOAD,
                .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                .CreationNodeMask = 1,
                .VisibleNodeMask = 1
            };
            device_->CreateCommittedResource(
                &prop,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                states,
                pclr,
                IID_PPV_ARGS(&res)
            );
            return res;
        }

        ID3D12Resource* CreateRemoteResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES states, D3D12_CLEAR_VALUE* pclr) override {
            ID3D12Resource* res;
            D3D12_HEAP_PROPERTIES prop {
                .Type = D3D12_HEAP_TYPE_DEFAULT,
                .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                .CreationNodeMask = 1,
                .VisibleNodeMask = 1
            };
            device_->CreateCommittedResource(
                &prop,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                states,
                pclr,
                IID_PPV_ARGS(&res)
            );
            return res;
        }
        void FreeResource(ID3D12Resource* resource) override {
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
            resource->Release();
        }

        ~DXDefaultAllocator() noexcept override {

        }
};

class Resource {
public:
    constexpr static size_t WAITABLE_COUNT = 2;
    constexpr static size_t RENDER_WAITABLE_INDEX = 0;
    constexpr static size_t COPY_WAITABLE_INDEX = 1;

    enum class HeapType {
        UPLOAD,
        DEFAULT,
        READBACK,
    };
    union ResourceMeta {
        struct {
            HeapType type;
            size_t element_size;
            size_t element_count;
            size_t layers;
        } Buffer;
        struct {
            size_t stride;
            uint32_t bind_index;
        } Tex2D;
    };
protected:
    std::string name_;
    DXAllocator* allocator_;
    std::vector<ID3D12Resource*> resources_;
    WaitableSet<WAITABLE_COUNT> waitable_set_;
    D3D12_RESOURCE_STATES states_;
    ResourceMeta meta_;
public:
    Resource(const std::string& name, DXAllocator* allocator, const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initial_states, const ResourceMeta& meta, WaitableSet<WAITABLE_COUNT>&& ws, D3D12_CLEAR_VALUE* clr = nullptr) : name_(name), allocator_(allocator), states_(initial_states), waitable_set_(std::move(ws)), meta_(meta) {
        if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
            if (meta.Buffer.type == HeapType::UPLOAD) {
                for (int i = 0; i < meta.Buffer.layers; ++i) {
                    resources_.push_back(allocator_->CreateLocalResource(desc, states_, clr));
                    resources_.back()->SetName(ConvertStringToWstring(std::format("{} #{}", name, i)).c_str());
                }
            } else if (meta.Buffer.type == HeapType::DEFAULT) {
                resources_.push_back(allocator_->CreateRemoteResource(desc, states_, clr));
            }
        } else if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
            resources_.push_back(allocator_->CreateRemoteResource(desc, states_, clr));
        } else {
            LFATAL("Cannot create resource {}: unexcepted construct args", name);
        }
    }

    Resource(const std::string& name, DXAllocator* allocator, ID3D12Resource* res, D3D12_RESOURCE_STATES current_states, const ResourceMeta& meta, WaitableSet<WAITABLE_COUNT>&& ws) : allocator_(allocator), states_(current_states), waitable_set_(std::move(ws)), meta_(meta) {
        resources_.push_back(res);
        resources_[0]->SetName(ConvertStringToWstring(name).c_str());
    }

    Resource(const Resource&) = delete;
    Resource(Resource&& r) : allocator_(r.allocator_), resources_(std::move(r.resources_)), waitable_set_(std::move(r.waitable_set_)), states_(r.states_), meta_(r.meta_) {
        r.allocator_ = nullptr;
    }

    std::string GetResourceName(size_t layer = 0) {
        UINT size = 0;
        resources_[layer]->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, nullptr);
        std::wstring name;
        if (size == 0) {
            name = L"Unnamed Object";
        } else {
            name.resize(size / sizeof(wchar_t));
            resources_[layer]->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, &name[0]);
        }
        return ConvertWstringToString(name);
    }

    ResourceMeta& GetResourceMeta() {
        return meta_;
    }

    ID3D12Resource* GetD3D12Resource(size_t layer = 0) {
        return resources_[layer];
    }

    Waitable& GetRenderWaitable() {
        return waitable_set_.Get<RENDER_WAITABLE_INDEX>();
    }

    Waitable& GetCopyWaitable() {
        return waitable_set_.Get<COPY_WAITABLE_INDEX>();
    }

    void Transition(D3D12_RESOURCE_STATES new_state, ComPtr<ID3D12GraphicsCommandList>& list) {
        D3D12_RESOURCE_BARRIER barrier {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        D3D12_RESOURCE_TRANSITION_BARRIER transition {};
        barrier.Transition.pResource = resources_[0];
        barrier.Transition.StateBefore = states_;
        barrier.Transition.StateAfter = new_state;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        list->ResourceBarrier(1, &barrier);
        states_ = new_state;
    }

    ~Resource() {
        for (auto& r : resources_) {
            GetRenderWaitable().CPUWait();
            GetCopyWaitable().CPUWait();
            allocator_->FreeResource(r);
        }
    }

    static bool CopyToRemoteBuffer(RecordDispatcher& copy_dispatcher, Resource* src, Resource* dest) {
        // type validation
        if (src->GetD3D12Resource()->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER
            && dest->GetD3D12Resource()->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER
            && src->GetResourceMeta().Buffer.type == HeapType::UPLOAD
            && dest->GetResourceMeta().Buffer.type == HeapType::DEFAULT
            && src->GetResourceMeta().Buffer.element_count * src->GetResourceMeta().Buffer.element_size <= dest->GetResourceMeta().Buffer.element_count * dest->GetResourceMeta().Buffer.element_size
            ) {
            copy_dispatcher.PostRecordTask({{[=](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                src->GetRenderWaitable().GPUWait();
                src->GetCopyWaitable().GPUWait();
                src->GetCopyWaitable().GetFenceValue() = nfv;
                dest->GetRenderWaitable().GPUWait();
                dest->GetCopyWaitable().GPUWait();
                dest->GetCopyWaitable().GetFenceValue() = nfv;
                list->CopyResource(dest->GetD3D12Resource(), src->GetD3D12Resource());
                return nullptr;
            }, [&](void*) {}}});
            return true;
        }
        LFATAL("Cannot copy buffer {} to remote buffer {}: type validation failed", src->GetResourceName(), dest->GetResourceName());
        return false;
    }

    static bool CopyToTexture(RecordDispatcher& copy_dispatcher, Resource* src, Resource* dest, D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint) {
        // type validation
        if (src->GetD3D12Resource()->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER
            && dest->GetD3D12Resource()->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D
            && src->GetResourceMeta().Buffer.type == HeapType::UPLOAD) {
            copy_dispatcher.PostRecordTask({{[=](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                D3D12_TEXTURE_COPY_LOCATION src_loc {};
                src_loc.pResource = src->GetD3D12Resource();
                src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                src_loc.PlacedFootprint = footprint;
                D3D12_TEXTURE_COPY_LOCATION dest_loc {};
                dest_loc.pResource = dest->GetD3D12Resource();
                dest_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dest_loc.SubresourceIndex = 0;
                src->GetRenderWaitable().GPUWait();
                src->GetCopyWaitable().GPUWait();
                src->GetCopyWaitable().GetFenceValue() = nfv;
                dest->GetRenderWaitable().GPUWait();
                dest->GetCopyWaitable().GPUWait();
                dest->GetCopyWaitable().GetFenceValue() = nfv;
                list->CopyTextureRegion(&dest_loc, 0, 0, 0, &src_loc, nullptr);
                return nullptr;
            }, [](void*) {}}});
            return true;
        }
        LFATAL("Cannot copy buffer {} to texture {}: type validation failed", src->GetResourceName(), dest->GetResourceName());
        return false;
    }
};

using ResourceHandle = Resource*;

template<D3D12_DESCRIPTOR_HEAP_TYPE TYPE>
class DescriptorHeap {
protected:
    ComPtr<ID3D12Device> device_;
    uint32_t capacity_;
    ComPtr<ID3D12DescriptorHeap> heap_;
    size_t increment_;
public:
    DescriptorHeap(ComPtr<ID3D12Device> device, uint32_t capacity, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE) : device_(device), capacity_(capacity) {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {
            .Type = TYPE,
            .NumDescriptors = capacity_,
            .Flags = flags,
            .NodeMask = 0
        };
        device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_));
        increment_ = device->GetDescriptorHandleIncrementSize(TYPE);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint64_t i) {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = heap_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += i * increment_;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint64_t i) {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = heap_->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += i * increment_;
        return handle;
    }

    ID3D12DescriptorHeap* GetD3D12Heap() {
        return heap_.Get();
    }
};

class TextureHeap : public DescriptorHeap<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV> {
    std::vector<bool> resident_;
    D3D12_DESCRIPTOR_RANGE ranges_[1];
public:
    TextureHeap(ComPtr<ID3D12Device> device, uint32_t capacity) : DescriptorHeap(device, capacity, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE), resident_(capacity) {
        ranges_[0] = {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            .NumDescriptors = capacity,
            .BaseShaderRegister = 0,
            .RegisterSpace = 1,
            .OffsetInDescriptorsFromTableStart = 0
        };
    }

    uint32_t BindTexture(ResourceHandle res) {
        for (uint32_t i = 0; i < resident_.size(); ++i) {
            if (!resident_[i]) {
                D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUHandle(i);
                auto desc = res->GetD3D12Resource()->GetDesc();
                D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
                    .Format = desc.Format,
                    .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
                    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                    .Texture2D = {
                        .MostDetailedMip = 0,
                        .MipLevels = desc.MipLevels,
                        .PlaneSlice = 0,
                        .ResourceMinLODClamp = 0.0f
                    }
                };
                device_->CreateShaderResourceView(res->GetD3D12Resource(), &srv_desc, handle);
                res->GetResourceMeta().Tex2D.bind_index = i;
                resident_[i] = true;
                return i;
            }
        }
        LFATAL("Cannot bind resource {} to texture heap: heap is full", res->GetResourceName());
    }

    std::pair<D3D12_DESCRIPTOR_RANGE*, uint32_t> GetDescriptorRanges() {
        return std::make_pair(ranges_, static_cast<uint32_t>(CountOf(ranges_)));
    }
};

class ResourceManager {
    ComPtr<ID3D12Device> device_;
    DXAllocator* allocator_;
    std::vector<ResourceHandle> alive_resources_;
    std::vector<ResourceHandle> expired_resource_;
    RecordDispatcher& render_dispatcher_;
    RecordDispatcher& copy_dispatcher_;
    const dx_init_t& init_;
    TextureHeap tex_heap_;
public:
    ResourceManager(ComPtr<ID3D12Device> device, DXAllocator* allocator, RecordDispatcher& render_dispatcher, RecordDispatcher& copy_dispatcher, const dx_init_t& init) : device_(device), allocator_(allocator), render_dispatcher_(render_dispatcher), copy_dispatcher_(copy_dispatcher), init_(init), tex_heap_(device, init.max_texture_count) {

    }

    ResourceHandle CreateLocalBuffer(const std::string& name, size_t element_size, size_t element_count, D3D12_RESOURCE_STATES initial_states, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_CLEAR_VALUE* pclr = nullptr, bool single_layer = false) {
        size_t bytes = element_size * element_count;
        D3D12_RESOURCE_DESC desc {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0,
            .Width = bytes,
            .Height = 1,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc = { 1, 0 },
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = flags,
        };
        Resource::ResourceMeta meta = {
            .Buffer = {
                .type = Resource::HeapType::UPLOAD,
                .element_size = element_size,
                .element_count = element_count,
                .layers = single_layer ? 1 : init_.buffer_count,
            }
        };
        ResourceHandle res = new Resource(name, allocator_, desc, initial_states, meta, WaitableSet<2>(Waitable(render_dispatcher_.GetFence(), render_dispatcher_.GetCommandQueue()), Waitable(copy_dispatcher_.GetFence(), copy_dispatcher_.GetCommandQueue())), pclr);
        alive_resources_.push_back(res);
        return res;
    }

    template<typename T>
    ResourceHandle CreateLocalBuffer(const std::string& name, std::vector<T>& vector, D3D12_RESOURCE_STATES initial_states, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_CLEAR_VALUE* pclr = nullptr, bool single_layer = false) {
        ResourceHandle rh = CreateLocalBuffer(name, sizeof(T), vector.size(), initial_states, flags, pclr, single_layer);
        void* mapping;
        rh->GetD3D12Resource()->Map(0, nullptr, &mapping);
        memcpy(mapping, &vector[0], vector.size() * sizeof(T));
        rh->GetD3D12Resource()->Unmap(0, nullptr);
        return rh;
    }

    template<typename T>
    ResourceHandle CreateLocalBuffer(const std::string& name, D3D12_RESOURCE_STATES initial_states, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_CLEAR_VALUE* pclr = nullptr) {
        ResourceHandle rh = CreateLocalBuffer(name, sizeof(T), 1, initial_states, flags, pclr);
        return rh;
    }

    ResourceHandle CreateRemoteBuffer(const std::string& name, size_t element_size, size_t element_count, D3D12_RESOURCE_STATES initial_states, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_CLEAR_VALUE* pclr = nullptr) {
        size_t bytes = element_size * element_count;
        D3D12_RESOURCE_DESC desc {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0,
            .Width = bytes,
            .Height = 1,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc = { 1, 0 },
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = flags,
        };
        Resource::ResourceMeta meta = {
            .Buffer = {
                .type = Resource::HeapType::DEFAULT,
                .element_size = element_size,
                .element_count = element_count,
                .layers = 1,
            }
        };
        ResourceHandle res = new Resource(name, allocator_, desc, initial_states, meta, WaitableSet<2>(Waitable(render_dispatcher_.GetFence(), render_dispatcher_.GetCommandQueue()), Waitable(copy_dispatcher_.GetFence(), copy_dispatcher_.GetCommandQueue())), pclr);
        alive_resources_.push_back(res);
        return res;
    }

    template<typename T>
    ResourceHandle CreateRemoteBuffer(const std::string& name, std::vector<T>& vector, D3D12_RESOURCE_STATES initial_states, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_CLEAR_VALUE* pclr = nullptr) {
        ResourceHandle upload = CreateLocalBuffer("UploadBuffer_" + name, vector, initial_states, flags, pclr, true);
        ResourceHandle rh = CreateRemoteBuffer(name, sizeof(T), vector.size(), initial_states, flags, pclr);
        Resource::CopyToRemoteBuffer(copy_dispatcher_, upload, rh);
        MarkAsExpired(upload);
        return rh;
    }

    ResourceHandle CreateTexture2D(const std::string& name, uint32_t width, uint32_t height, uint16_t mip_levels, DXGI_FORMAT fmt, D3D12_RESOURCE_STATES initial_states, const DXGI_SAMPLE_DESC& multi_sample_desc = { 1, 0 },  bool bind_tex = true, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_CLEAR_VALUE* pclr = nullptr) {
        D3D12_RESOURCE_DESC desc {
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Alignment = 0,
            .Width = width,
            .Height = height,
            .DepthOrArraySize = 1,
            .MipLevels = mip_levels,
            .Format = fmt,
            .SampleDesc = multi_sample_desc,
            .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
            .Flags = flags,
        };
        Resource::ResourceMeta meta = {
            .Tex2D = {
                .stride = BitsPerPixel(fmt) / 8,
                .bind_index = 0
            }
        };
        ResourceHandle res = new Resource(name, allocator_, desc, initial_states, meta, WaitableSet<2>(Waitable(render_dispatcher_.GetFence(), render_dispatcher_.GetCommandQueue()), Waitable(copy_dispatcher_.GetFence(), copy_dispatcher_.GetCommandQueue())), pclr);
        if (bind_tex) tex_heap_.BindTexture(res);
        alive_resources_.push_back(res);
        return res;
    }

    template<typename ImageFormat>
    requires is_image_format<ImageFormat>
    ResourceHandle CreateTexture2DFromImage(const std::string& name, Image<ImageFormat>& img) {
        ResourceHandle texture = CreateTexture2D(name, img.width, img.height, 1, ImageFormat::DXGIFormat, D3D12_RESOURCE_STATE_COMMON);
        auto tex_desc = texture->GetD3D12Resource()->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
        uint32_t rows;
        uint64_t row_pitch;
        uint64_t total_bytes;
        device_->GetCopyableFootprints(&tex_desc, 0, 1, 0, &footprint, &rows, &row_pitch, &total_bytes);
        void* mapping;
        ResourceHandle upload = CreateLocalBuffer("UploadBuffer_" + name, 1, footprint.Footprint.RowPitch * rows, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE, nullptr, true);
        upload->GetD3D12Resource()->Map(0, nullptr, &mapping);
        CHECKHR(upload->GetD3D12Resource()->Map(0, nullptr, &mapping));
        auto* dest = static_cast<uint8_t *>(mapping) + footprint.Offset;
        uint64_t dest_pitch = footprint.Footprint.RowPitch;
        for (int i = 0; i < rows; ++i) {
            memcpy(dest + i * dest_pitch, &img.At(0, i), img.width * img.stride);
        }
        upload->GetD3D12Resource()->Unmap(0, nullptr);
        Resource::CopyToTexture(copy_dispatcher_, upload, texture, footprint);
        MarkAsExpired(upload);
        return texture;
    }

    ResourceHandle CreateAdoptedBackBuffer(const std::string& name, ID3D12Resource* backbuffer, D3D12_RESOURCE_STATES current_states) {
        Resource::ResourceMeta meta {
            .Tex2D = {
                .stride = BitsPerPixel(backbuffer->GetDesc().Format) / 8,
                .bind_index = 0
            }
        };
        ResourceHandle res = new Resource(name, allocator_, backbuffer, current_states, meta, WaitableSet<2>(Waitable(render_dispatcher_.GetFence(), render_dispatcher_.GetCommandQueue()), Waitable(copy_dispatcher_.GetFence(), copy_dispatcher_.GetCommandQueue())));
        alive_resources_.push_back(res);
        return res;
    }

    TextureHeap& GetTextureHeap() {
        return tex_heap_;
    }

    bool MarkAsExpired(ResourceHandle rh) {
        auto iter = std::remove_if(alive_resources_.begin(), alive_resources_.end(), [&](const auto& h) {
                return h == rh;
        });
        if (iter == alive_resources_.end()) return false;
        alive_resources_.erase(iter, alive_resources_.end());
        expired_resource_.push_back(rh);
        return true;
    }

    void Cleanup() {
        for (auto it = expired_resource_.begin(); it != expired_resource_.end();) {
            if ((*it)->GetCopyWaitable().Completed() && (*it)->GetRenderWaitable().Completed()) {
                delete *it;
                it = expired_resource_.erase(it);
            } else {
                ++it;
            }
        }
    }
};

    template<typename S>
    class StructuredView {
        ResourceHandle rh_;
        size_t layer_;
        size_t capacity_;
        struct IOPtr {
            size_t wpos;
            size_t rpos;
            S* mapping;
        };
        std::vector<IOPtr> io_ptrs_;
    public:
        StructuredView() = default;
        StructuredView(ResourceHandle rh) {
            Create(rh);
        }
        StructuredView(const StructuredView&) = delete;
        StructuredView(StructuredView&&) = delete;

        void Create(ResourceHandle rh) {
            if (rh->GetResourceMeta().Buffer.type != Resource::HeapType::UPLOAD) {
                LFATAL("Cannot create StructureView of resource{}: type validation failed", rh->GetResourceName());
            }
            rh_ = rh;
            for (size_t i = 0; i < rh_->GetResourceMeta().Buffer.layers; ++i) {
                S* ptr;
                rh_->GetD3D12Resource(i)->Map(0, nullptr, reinterpret_cast<void**>(&ptr));
                io_ptrs_.emplace_back(0, 0, ptr);
            }
            layer_ = 0;
            capacity_ = rh->GetResourceMeta().Buffer.element_count;
        }

        uint64_t SelectLayer(size_t layer) {
            uint64_t old_layer = layer_;
            if (layer != layer_) {
                layer_ = layer;
            }
            return old_layer;
        }

        void Append(const S& s) {
            auto& ioptr = io_ptrs_[layer_];
            if (ioptr.wpos >= capacity_) {
                LFATAL("Cannot append data to StructureView when modify resource {}: out of bound", rh_->GetResourceName(layer_));
            }
            ioptr.mapping[ioptr.wpos] = s;
            ++ioptr.wpos;
        }

        S& operator[](uint64_t i) {
            if (i > capacity_) {
                LFATAL("Cannot access StructureView when modify resource {}: out of bound", rh_->GetResourceName(layer_));
            }
            return io_ptrs_[layer_].mapping[i];
        }

        void ResetWritePos() {
            io_ptrs_[layer_].wpos = 0;
        }

        ~StructuredView() {
            for (size_t i = 0; i < rh_->GetResourceMeta().Buffer.layers; ++i) {
                rh_->GetD3D12Resource(i)->Unmap(0, nullptr);
            }
        }
    };
}