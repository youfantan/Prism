#pragma once

#include <base.h>

#include <render/drawcall.h>
#include <render/queue.h>
#include <render/resource.h>
#include <io/shader.h>
#include <io/font.h>
#include <render/render_context.h>

namespace Prism
{
    class PrismApp;

    class Window {
    private:
        HWND hwnd_;
    public:
        static LRESULT CALLBACK RenderWindowProcess(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        Window(HINSTANCE hinstance, uint32_t width, uint32_t height, PrismApp* app) {
            std::wstring class_name = L"Prism Renderer";
            WNDCLASS wc {};
            wc.lpfnWndProc = RenderWindowProcess;
            wc.hInstance = hinstance;
            wc.lpszClassName = class_name.c_str();
            RegisterClass(&wc);
            hwnd_ = CreateWindowEx(0, class_name.c_str(), L" Prism Renderer", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, hinstance, app);
            if (hwnd_ == nullptr) {
                LERROR("Cannot create Win32 window");
            }
        }

        void Show() {
            ShowWindow(hwnd_, true);
        }

        bool FetchMessage() {
            MSG msg {};
            if (PeekMessage(&msg, hwnd_, 0, 0, PM_REMOVE) != 0) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                return true;
            }
            return false;
        }

        HWND GetHandle() {
            return hwnd_;
        }
    };


    class PrismApp {
    protected:
        const dx_init_t& init_;
        Device& device_;
        DXAllocator* allocator_;
        Window window_;
        RecordDispatcher copy_dispatcher_;
        RecordDispatcher render_dispatcher_;
        ResourceManager res_mgr_;
        RenderContext ctx_;
        ShaderLoader shader_loader_;
        MetronomeTimer mt_;
        PipelineManager pl_mgr_;
    public:
        PrismApp(const dx_init_t& init, Device& device, DXAllocator* allocator)
        : init_(init), device_(device), allocator_(allocator), window_(nullptr, init.width, init.height, this),
        render_dispatcher_("Render Dispatcher", device_, init_.render_threads_count, init.lists_per_render_thread),
        copy_dispatcher_("Copy Dispatcher", device_, init.copy_threads_count, init.lists_per_copy_thread),
        res_mgr_(device_.GetComPtr(), allocator_, render_dispatcher_, copy_dispatcher_, init_),
        ctx_(init, window_.GetHandle(), device_, res_mgr_, render_dispatcher_), shader_loader_(init.shaders_dir), mt_(1000 / init.fps_limit, [&](MetronomeTimer& mt) { Loop(mt); }) {
        }

        DXAllocator* GetAllocator() {
            return allocator_;
        }

        const dx_init_t& GetInitializeParams() const {
            return init_;
        }

        Window& GetWindow() {
            return window_;
        }

        Device& GetDevice() {
            return device_;
        }

        RecordDispatcher& GetRenderDispatcher() {
            return render_dispatcher_;
        }

        RecordDispatcher& GetCopyDispatcher() {
            return copy_dispatcher_;
        }

        ResourceManager& GetResourceManager() {
            return res_mgr_;
        }

        PipelineManager& GetPipelineManager() {
            return pl_mgr_;
        }

        RenderContext& GetRenderContext() {
            return ctx_;
        }

        ShaderLoader& GetShaderLoader() {
            return shader_loader_;
        }

        void RunLoop() {
            window_.Show();
            mt_.Start();
        }

        virtual void Loop(MetronomeTimer& mt) = 0;
        virtual void OnKeyUp(WPARAM wparam) = 0;
        virtual void OnKeyDown(WPARAM wparam) = 0;
        virtual void OnActive(WPARAM wparam) = 0;
        virtual void OnClose(WPARAM wparam) {
            mt_.Stop();
        }
    };
}