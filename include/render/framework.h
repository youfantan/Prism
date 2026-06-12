#pragma once

#include <base.h>

#include "drawcall.h"
#include "queue.h"
#include "resource.h"

using dx_init_t = struct {
    uint32_t width;
    uint32_t height;
    HWND hwnd;
    uint32_t buffer_count;
    uint32_t copy_workers_count;
    MSAAType msaa_type;
    float rt_clear_color[4];
};

class Device {
private:
    ComPtr<IDXGIFactory7> factory_;
    ComPtr<IDXGIAdapter> adapter_;
    ComPtr<ID3D12Device> device_;
public:
    Device();

    ComPtr<ID3D12Device>& GetComPtr() {
        return device_;
    }

    ComPtr<IDXGIFactory7>& GetFactory() {
        return factory_;
    }

    ComPtr<IDXGIAdapter>& GetSelectedAdapter() {
        return adapter_;
    }
};

struct FrameResource {
    uint32_t index;
    Resource<ResourceType::RenderTarget> back_buffer;
    Resource<ResourceType::DepthBuffer> depth_buffer;
    Resource<ResourceType::RenderTarget> msaa_buffer;

    FrameResource(uint32_t i, Resource<ResourceType::RenderTarget>& bb, Resource<ResourceType::DepthBuffer>& db, Resource<ResourceType::RenderTarget>& mb) : index(i), back_buffer(std::move(bb)), depth_buffer(std::move(db)), msaa_buffer(std::move(mb)) {}
    FrameResource(const FrameResource&) = delete;
    FrameResource(FrameResource&& fr) noexcept : index(fr.index), back_buffer(std::move(fr.back_buffer)), depth_buffer(std::move(fr.depth_buffer)), msaa_buffer(std::move(fr.msaa_buffer)) {
        fr.index = UINT32_MAX;
    }

    template<typename Allocator>
    static void InitializeFrameResources(const dx_init_t& init, std::vector<FrameResource>& frs,
        ComPtr<IDXGISwapChain4>& swapchain, ResourceManager<Allocator>& res_mgr, ComPtr<ID3D12Device>& device,
        RTVHeap& rtv_heap, DSVHeap& dsv_heap, RTVHeap& msaa_heap) {
        for (int i = 0; i < init.buffer_count; ++i) {
            ComPtr<ID3D12Resource> back_buffer_resource;
            swapchain->GetBuffer(i, IID_PPV_ARGS(&back_buffer_resource));
            auto back_buffer = res_mgr.CreateFromExistsPtr<ResourceType::RenderTarget>(back_buffer_resource, rtv_heap.GetCPUHandle(i), D3D12_RESOURCE_STATE_PRESENT);
            auto depth_buffer = res_mgr.CreateDepthBuffer(init.width, init.height, init.msaa_type, dsv_heap.GetCPUHandle(i));
            device->CreateRenderTargetView(back_buffer.GetComPtr().Get(), nullptr, rtv_heap.GetCPUHandle(i));
            device->CreateDepthStencilView(depth_buffer.GetComPtr().Get(), nullptr, dsv_heap.GetCPUHandle(i));
            if (init.msaa_type != MSAAType::NONE) {
                auto msaa_buffer = res_mgr.CreateRenderTarget(DXGI_FORMAT_R8G8B8A8_UNORM, init.width, init.height, init.msaa_type, msaa_heap.GetCPUHandle(i));
                D3D12_RENDER_TARGET_VIEW_DESC mrtv_desc {};
                mrtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                mrtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
                device->CreateRenderTargetView(msaa_buffer.GetComPtr().Get(), &mrtv_desc, msaa_heap.GetCPUHandle(i));
            } else {
                auto msaa_buffer = res_mgr.CreateFromExistsPtr<ResourceType::RenderTarget>(nullptr);
                frs.emplace_back(i, back_buffer, depth_buffer, msaa_buffer);
            }
        }
    }
};

class RenderContext {
    template<typename Allocator>
    requires is_allocator<Allocator>
    friend class DXFramework;
public:
    using render_callback_t = std::function<void()>;
    using render_record_t = std::function<void(ComPtr<ID3D12GraphicsCommandList>&)>;
private:
    Device& device_;
    const dx_init_t& init_;
    std::vector<FrameResource> frame_resources_;
    ComPtr<IDXGISwapChain4> swapchain_;
    RenderQueue& render_queue_;
    DSVHeap dsv_heap_;
    RTVHeap rtv_heap_;
    RTVHeap msaa_heap_;
    std::vector<std::pair<Drawcall*, render_record_t>> records_;
public:
    RenderContext(Device& device, RenderQueue& rq, const dx_init_t& init);
    void Render(render_callback_t&& rcb);

    void AddRenderRecord(Drawcall* drawcall, render_record_t&& record) {
        records_.emplace_back(drawcall, std::move(record));
    }

    ComPtr<ID3D12Device>& GetDevice() {
        return device_.GetComPtr();
    }

    FrameResource& GetCurrentFrameResource() {
        return frame_resources_[swapchain_->GetCurrentBackBufferIndex()];
    }
};

template<typename Allocator = DXDefaultAllocator>
requires is_allocator<Allocator>
class DXFramework {
private:
    const dx_init_t& init_;
    Device device_;
    Fence fence_;
    RenderQueue render_queue_;
    CopyQueue copy_queue_;
    Allocator allocator_;
    ResourceManager<Allocator> res_mgr_;
    RenderContext ctx_;
public:
    DXFramework(const dx_init_t& init, const Allocator::init_t& allocator_init = {}) : init_(init), fence_(device_.GetComPtr()), copy_queue_(device_.GetComPtr(), init_.copy_workers_count), render_queue_(device_.GetComPtr(), init_.buffer_count), allocator_(device_.GetComPtr(), allocator_init), res_mgr_(device_.GetComPtr(), allocator_, copy_queue_), ctx_(device_, render_queue_, init_) {
        FrameResource::InitializeFrameResources<Allocator>(init_, ctx_.frame_resources_, ctx_.swapchain_, res_mgr_, device_.GetComPtr(), ctx_.rtv_heap_, ctx_.dsv_heap_, ctx_.msaa_heap_);
    }

    RenderContext& GetRenderContext() {
        return ctx_;
    }

    ResourceManager<Allocator>& GetResourceManager() {
        return res_mgr_;
    }

    CopyQueue& GetCopyQueue() {
        return copy_queue_;
    }

    RenderQueue& GetRenderQueue() {
        return render_queue_;
    }
};