#include <functional>
#include <string>
#include <Windows.h>

#include "mlog.h"
#include "io/shader.h"
#include "io/texture.h"
#include "render/drawcall.h"
#include "render/framework.h"
#include <io.h>

#include "transform/camera.h"

#define WIDTH 1280
#define HEIGHT 720

LRESULT CALLBACK Callback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
HWND InitWin32Window(HINSTANCE h, int show)
{
    std::wstring class_name = L"Prism Renderer";
    WNDCLASS wc {};
    wc.lpfnWndProc = Callback;
    wc.hInstance = h;
    wc.lpszClassName = class_name.c_str();
    RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(0, class_name.c_str(), L" Prism Renderer", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, WIDTH, HEIGHT, nullptr, nullptr, h, nullptr);
    if (hwnd == nullptr) {
        LERROR("Cannot create Win32 window");
        return nullptr;
    }
    ShowWindow(hwnd, show);
    return hwnd;
}

bool running_flag_ = true;

void RunLoop(std::function<void()> loop)
{
    MSG msg {};
    while (running_flag_ && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        loop();
    }
}

struct alignas(256) ObjectMapping {
    uint32_t tex_index;
};

struct alignas(256) ObjectTransformation {
    float4x4 wvp;
};

struct Vertex {
    struct {
        float X;
        float Y;
        float Z;
    } Position;
    struct {
        float U;
        float V;
    } Tex;
    struct {
        float X;
        float Y;
        float Z;
    } Normal;
};

Vertex v_Rectangle[16] = {
    // Bottom
    {{ -0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }},
    {{ 0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }},
    {{ 0.5f, -0.5f, 0.5f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }},
    {{ -0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }},

    // Front
    {{ -0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }},
    {{ 0.0f, 0.5f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f }},
    {{ 0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }},
    // Left
    {{ -0.5f, -0.5f, 0.5f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }},
    {{ 0.0f, 0.5f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f }},
    {{ -0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }},
    // Right
    {{ 0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }},
    {{ 0.0f, 0.5f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f }},
    {{ 0.5f, -0.5f, 0.5f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }},
    // Back
    {{ 0.5f, -0.5f, 0.5f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }},
    {{ 0.0f, 0.5f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f }},
    {{ -0.5f, -0.5f, 0.5f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }},
};

uint32_t i_Rectangle[18] = {
    // Bottom
    0, 1, 2,
    0, 2, 3,
    // Front
    4, 5, 6,
    // Left
    7, 8, 9,
    // Right
    10, 11, 12,
    // Back
    13, 14, 15
};

FreeCamera* frc = nullptr;
KMInput* kmi;

DXFramework<DXDefaultAllocator>* p_dxfw;

int WINAPI wWinMain(HINSTANCE h, HINSTANCE p, PWSTR cmdline, int show) {
#ifndef NDEBUG
    mlog_enable_win32_console();
    mlog_enable_win32_vansi();
#endif

    mlog_sth_init_t mlog_init {
        .log_directory = "logs",
        .log_file_name = "log.txt"
    };
    mlog_sth_init(mlog_init);
    HWND hwnd = InitWin32Window(h, show);

    FreeCamera camera(hwnd, WIDTH, HEIGHT, 45);
    frc = &camera;
    FreeCamera::object_params_t op {};
    KMInput::keyboard_control_key_mappings_t kmapping {
        .forward_vk = 'W',
        .backward_vk = 'S',
        .left_vk = 'A',
        .right_vk = 'D',
        .escape_vk = VK_ESCAPE,
    };
    KMInput km(hwnd, kmapping);
    kmi = &km;

    ShaderLoader sl("shaders");
    auto object_vs = sl.LoadShaderIntoMemory("object", ShaderType::VertexShader);
    auto object_ps = sl.LoadShaderIntoMemory("object", ShaderType::PixelShader);
    TextureLoader tl("textures");
    auto metal_img = tl.LoadTextureIntoMemory("metal");
    auto stone_img = tl.LoadTextureIntoMemory("stone");
    dx_init_t init = {
        .width = WIDTH,
        .height = HEIGHT,
        .hwnd = hwnd,
        .buffer_count = 2,
        .copy_workers_count = 5,
        .msaa_type = MSAAType::NONE,
        .rt_clear_color = {0.4f, 0.7f, 0.5f, 1.0f},
    };

    DXFramework dxfw(init);
    p_dxfw = &dxfw;
    auto& render_context = dxfw.GetRenderContext();
    auto& resmgr = dxfw.GetResourceManager();
    auto vertex_buffer = resmgr.CreateVertexBuffer(v_Rectangle, count_of(v_Rectangle));
    auto index_buffer = resmgr.CreateIndexBuffer(i_Rectangle, count_of(i_Rectangle));
    auto transformation = resmgr.CreateConstantBuffer<ObjectTransformation>();
    ObjectTransformation* t_Obj = transformation.GetMapping<ObjectTransformation>();
    camera.MakeWVP(op, t_Obj->wvp);
    auto mapping = resmgr.CreateConstantBuffer<ObjectMapping>();
    ObjectMapping* m_Obj = mapping.GetMapping<ObjectMapping>();
    BindlessHeap heap(render_context.GetDevice(), 8, 8, 8);
    auto metal_tex = resmgr.CreateTexture(metal_img.value());
    auto stone_tex = resmgr.CreateTexture(stone_img.value());
    heap.BindConstantBuffer("transformation", transformation);
    heap.BindShaderResource("metal", metal_tex);
    heap.BindShaderResource("stone", stone_tex);
    D3D12_INPUT_ELEMENT_DESC iv_layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    StaticSamplers ssamplers;
    ssamplers.Add(StaticSamplers::LINEAR_FILTER(0));
    DrawcallResource object_draw_res = {
        .vs_bytecode = object_vs.blob,
        .ps_bytecode = object_ps.blob,
        .rasterizer_desc = DefaultRasterizerDesc,
        .blend_desc = DefaultBlendDesc,
        .ds_desc = DefaultDepthStencilDesc,
        .sample_desc = {1, 0},
        .iv_layout = {iv_layout, 3},
        .samplers = ssamplers
    };
    Drawcall object_draw(render_context.GetDevice(), dxfw.GetRenderQueue(), heap, object_draw_res);
    m_Obj->tex_index = 0;
    PerformanceCounter pc;

    RunLoop([&] {
        float delta = pc.DeltaMs();
        km.UpdateFreeCamera(camera);
        camera.MakeWVP(op, t_Obj->wvp);
        render_context.Render([&] {
            object_draw(render_context, mapping, vertex_buffer, index_buffer);
        });
        dxfw.GetCopyQueue().DeferredRelease();
    });
    mlog_sth_close();
    return 0;
}

LRESULT CALLBACK Callback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_DESTROY: {
            running_flag_ = false;
            return 0;
        }

        case WM_PAINT: {
            return 0;
        }

        case WM_ACTIVATE: {
            if (LOWORD(wParam) == WA_INACTIVE) {
                if (frc != nullptr) {
                    frc->IsFocus() = false;
                    ShowCursor(TRUE);
                }
            }
            else {
                if (frc != nullptr) {
                    frc->IsFocus() = true;
                    ShowCursor(FALSE);
                }
            }
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}