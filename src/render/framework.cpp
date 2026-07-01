#pragma once
#include <format>
#include <render/framework.h>

LRESULT Prism::Window::RenderWindowProcess(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    PrismApp* app = nullptr;
    if (uMsg != WM_NCCREATE) {
        app = reinterpret_cast<PrismApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    switch (uMsg) {
        case WM_NCCREATE: {
            CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            app = reinterpret_cast<PrismApp*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
            break;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            if (app != nullptr) {
                app->OnClose(wParam);
            }
            return 0;
        }

        case WM_ACTIVATE: {
            if (app != nullptr) {
                app->OnActive(wParam);
            }
            break;
        }

        case WM_KEYDOWN: {
            if (app != nullptr) {
                app->OnKeyDown(wParam);
            }
            break;
        }

        case WM_KEYUP: {
            if (app != nullptr) {
                app->OnKeyUp(wParam);
            }
            break;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

Prism::Device::Device() {
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