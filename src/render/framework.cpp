#include <format>
#include <render/framework.h>

Device::Device() {
        CHECKHR(CreateDXGIFactory1(IID_PPV_ARGS(&factory_)));
        CHECKHR(factory_->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter_)));
        DXGI_ADAPTER_DESC desc;
        CHECKHR(adapter_->GetDesc(&desc));
#ifdef _DEBUG
        ComPtr<ID3D12Debug> debug_controller;
        CHECKHR(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)));
        debug_controller->EnableDebugLayer();
#endif
        D3D12CreateDevice(adapter_.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device_));
#ifdef _DEBUG
        ComPtr<ID3D12InfoQueue> info_queue;
        CHECKHR(device_->QueryInterface(IID_PPV_ARGS(&info_queue)));
        info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
#endif
}

RenderContext::RenderContext(Device& device, RenderQueue& rq, const dx_init_t& init) : device_(device), init_(init), render_queue_(rq), rtv_heap_(device_.GetComPtr(), init.buffer_count), dsv_heap_(device_.GetComPtr(), init.buffer_count), msaa_heap_(device_.GetComPtr(), init.buffer_count) {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS ms_lv;
        ms_lv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        ms_lv.SampleCount = 4;
        ms_lv.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        ms_lv.NumQualityLevels = 0;
        CHECKHR(device_.GetComPtr()->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &ms_lv, sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS )));
        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.BufferCount = init_.buffer_count;
        sd.Width = init_.width;
        sd.Height = init_.height;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.SampleDesc.Count = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        sd.Scaling = DXGI_SCALING_NONE;
        ComPtr<IDXGISwapChain1> sc;
        CHECKHR(device_.GetFactory()->CreateSwapChainForHwnd(render_queue_.GetComPtr().Get(), init_.hwnd, &sd, nullptr, nullptr, &sc));
        CHECKHR(sc.As(&swapchain_));
}

void RenderContext::Render(render_callback_t&& rcb) {
        records_.clear();
        rcb();
        auto& fr = GetCurrentFrameResource();
        auto& list = render_queue_.PrepareRenderQueue(swapchain_->GetCurrentBackBufferIndex());
        D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)init_.width, (float)init_.height, 0.0f, 1.0f };
        D3D12_RECT scissor_rect = { 0, 0, static_cast<LONG>(init_.width), static_cast<LONG>(init_.height) };
        auto rtv = fr.back_buffer.GetView();
        auto dsv = fr.depth_buffer.GetView();
        list->RSSetViewports(1, &viewport);
        list->RSSetScissorRects(1, &scissor_rect);
        fr.back_buffer.Transition(D3D12_RESOURCE_STATE_RENDER_TARGET, list);
        list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        list->ClearRenderTargetView(rtv, init_.rt_clear_color, 0, nullptr);
        for (const auto& [drawcall, record] : records_) {
                record(list);
        }
        fr.back_buffer.Transition(D3D12_RESOURCE_STATE_PRESENT, list);
        uint64_t fence_value = render_queue_.CommitRenderQueue(swapchain_->GetCurrentBackBufferIndex());
        for (const auto& [drawcall, record] : records_) {
                drawcall->waitable_ = Waitable(render_queue_.GetRenderFence(), fence_value);
        }
        swapchain_->Present(1, 0);
}
