#pragma once

#include <new>

namespace Prism
{

    class RenderContext {
        const dx_init_t& init_;
        ComPtr<ID3D12Device> device_;
        ResourceManager& mgr_;
        RecordDispatcher& render_dispatcher_;

        ComPtr<IDXGISwapChain4> swapchain_;
        uint64_t index_ {};
    public:
        RenderContext(const dx_init_t& init, HWND hwnd, Device& device, ResourceManager& rm, RecordDispatcher& render_dispatcher) : init_(init), device_(device.GetComPtr()), mgr_(rm), render_dispatcher_(render_dispatcher) {
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
            sd.Flags = init.enable_vsync ? 0 : DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
            sd.Scaling = DXGI_SCALING_STRETCH;
            ComPtr<IDXGISwapChain1> sc;
            CHECKHR(device.GetFactory()->CreateSwapChainForHwnd(render_dispatcher_.GetCommandQueue().Get(), hwnd, &sd, nullptr, nullptr, &sc));
            CHECKHR(sc.As(&swapchain_));
        }

        void Present() {
            if (init_.enable_vsync) {
                CHECKHR(swapchain_->Present(1, 0));
            } else {
                CHECKHR(swapchain_->Present(0, DXGI_PRESENT_ALLOW_TEARING));
            }
        }

        void Swap() {
            ++index_;
            if (index_ == init_.buffer_count) index_ = 0;
        }

        const dx_init_t& GetInitializeParams() {
            return init_;
        }

        RecordDispatcher& GetRenderDispatcher() {
            return render_dispatcher_;
        }

        ResourceManager& GetResourceManager() {
            return mgr_;
        }

        ComPtr<IDXGISwapChain4> GetSwapchain() {
            return swapchain_;
        }

        ComPtr<ID3D12Device> GetDevice() {
            return device_;
        }

        uint64_t GetCurrentIndex() const {
            return index_;
        }

    };

}