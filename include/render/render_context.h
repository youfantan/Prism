#pragma once

#include <render/render_target.h>
#include <new>

namespace Prism
{
    class RenderContext {
        const dx_init_t& init_;
        std::vector<RenderTarget> rts_;
        ComPtr<ID3D12Device> device_;
        ResourceManager& mgr_;
        RecordDispatcher& render_dispatcher_;
        DescriptorHeap<D3D12_DESCRIPTOR_HEAP_TYPE_RTV> rtv_heap_;
        DescriptorHeap<D3D12_DESCRIPTOR_HEAP_TYPE_DSV> dsv_heap_;
        DescriptorHeap<D3D12_DESCRIPTOR_HEAP_TYPE_RTV> msaa_heap_;

        ComPtr<IDXGISwapChain4> swapchain_;
        uint64_t index_ {};
    public:
        RenderContext(const dx_init_t& init, HWND hwnd, Device& device, ResourceManager& rm, RecordDispatcher& render_dispatcher) : init_(init), device_(device.GetComPtr()), mgr_(rm), render_dispatcher_(render_dispatcher), rtv_heap_(device_, init.buffer_count), dsv_heap_(device_, init.buffer_count), msaa_heap_(device_, 1) {
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS ms_lv;
            ms_lv.Format = init.rt_format;
            ms_lv.SampleCount = 4;
            ms_lv.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
            ms_lv.NumQualityLevels = 0;
            CHECKHR(device_->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &ms_lv, sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS )));
            DXGI_SWAP_CHAIN_DESC1 sd{};
            sd.BufferCount = init.buffer_count;
            sd.Width = init.width;
            sd.Height = init.height;
            sd.Format = init.rt_format;
            sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            sd.SampleDesc.Count = 1;
            sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
            sd.Scaling = DXGI_SCALING_NONE;
            ComPtr<IDXGISwapChain1> sc;
            CHECKHR(device.GetFactory()->CreateSwapChainForHwnd(render_dispatcher_.GetCommandQueue().Get(), hwnd, &sd, nullptr, nullptr, &sc));
            CHECKHR(sc.As(&swapchain_));
            D3D12_CLEAR_VALUE depth_clr {
                .Format = init.ds_format,
                .DepthStencil = {
                    .Depth = 1.0f,
                    .Stencil = 0
                }
            };
            D3D12_CLEAR_VALUE rt_clr = {
                .Format = init.rt_format,
                .Color = { init.rt_clear_color[0], init.rt_clear_color[1], init.rt_clear_color[2], init.rt_clear_color[3] }
            };
            rts_.reserve(init.buffer_count);
            ResourceHandle msaa_buffer = nullptr;
            D3D12_CPU_DESCRIPTOR_HANDLE mrtv_handle {};
            std::string msaa_buffer_name("MSAA Buffer");
            if (init.msaa_type != MSAAType::NONE) {
                msaa_buffer = mgr_.CreateTexture2D(msaa_buffer_name, init.width, init.height, 1, init.rt_format, D3D12_RESOURCE_STATE_COMMON, GetSampleDesc(init_.msaa_type), false, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, &rt_clr);
                device_->CreateRenderTargetView(msaa_buffer->GetD3D12Resource(), nullptr, msaa_heap_.GetCPUHandle(0));
                mrtv_handle = msaa_heap_.GetCPUHandle(0);
            }
            for (int i = 0; i < init.buffer_count; ++i) {
                std::string back_buffer_name = std::format("BackBuffer #{}", i);
                std::string depth_buffer_name = std::format("DepthBuffer #{}", i);
                ID3D12Resource* back_buffer_resource;
                swapchain_->GetBuffer(i, IID_PPV_ARGS(&back_buffer_resource));
                ResourceHandle back_buffer = mgr_.CreateAdoptedBackBuffer(back_buffer_name, back_buffer_resource, D3D12_RESOURCE_STATE_PRESENT);
                ResourceHandle depth_buffer = mgr_.CreateTexture2D(depth_buffer_name, init.width, init.height, 1, init.ds_format, D3D12_RESOURCE_STATE_DEPTH_WRITE, GetSampleDesc(init_.msaa_type), false, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, &depth_clr);
                device_->CreateRenderTargetView(back_buffer->GetD3D12Resource(), nullptr, rtv_heap_.GetCPUHandle(i));
                device_->CreateDepthStencilView(depth_buffer->GetD3D12Resource(), nullptr, dsv_heap_.GetCPUHandle(i));
                rts_.emplace_back(init_, back_buffer, depth_buffer, msaa_buffer, rtv_heap_.GetCPUHandle(i), dsv_heap_.GetCPUHandle(i), mrtv_handle);
            }
        }

        void Render(std::initializer_list<RecordDispatcher::GPUProcess> processes) {
            std::vector<RecordDispatcher::GPUProcess> all;
            all.reserve(processes.size() + 3);
            RenderTarget& rt = rts_[index_];
            all.push_back(rt.BeginRender());
            for (size_t i = 0; i < processes.size(); ++i) {
                all.push_back(processes.data()[i]);
            }
            all.push_back(rt.FinishRender());
            all.push_back({[&](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                if (init_.enable_vsync) {
                    swapchain_->Present(1, 0);
                } else {
                    swapchain_->Present(0, DXGI_PRESENT_ALLOW_TEARING);
                }
            }, [&] {
                mgr_.Cleanup();
            }});
            render_dispatcher_.PostRecordTask(std::move(all));
            ++index_;
            if (index_ == init_.buffer_count) index_ = 0;
        }

    };

}