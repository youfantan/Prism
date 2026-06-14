#include <functional>
#include <string>
#include <Windows.h>

#include "mlog.h"
#include "io/shader.h"
#include "io/texture.h"
#include "render/framework.h"

#include "transform/camera.h"
#include "transform/helper.h"

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

// Vertex v_LightCube[8] = {
//     {{-1.0f, -1.0f, -1.0f}, {0,0}, {0,0,0}},
//     {{ 1.0f, -1.0f, -1.0f}, {0,0}, {0,0,0}},
//     {{ 1.0f, -1.0f,  1.0f}, {0,0}, {0,0,0}},
//     {{-1.0f, -1.0f,  1.0f}, {0,0}, {0,0,0}},
//     {{-1.0f,  1.0f, -1.0f}, {0,0}, {0,0,0}},
//     {{ 1.0f,  1.0f, -1.0f}, {0,0}, {0,0,0}},
//     {{ 1.0f,  1.0f,  1.0f}, {0,0}, {0,0,0}},
//     {{-1.0f,  1.0f,  1.0f}, {0,0}, {0,0,0}},
// };
//
// uint32_t i_LightCube[36] = {
//     2, 6, 7,
//     2, 7, 3,
//
//     0, 4, 5,
//     0, 5, 1,
//
//     4, 7, 6,
//     4, 6, 5,
//
//     0, 3, 2,
//     0, 2, 1,
//
//     1, 5, 6,
//     1, 6, 2,
//
//     0, 7, 4,
//     0, 3, 7,
// };

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
    KMInput::keyboard_control_key_mappings_t kmapping {
        .forward_vk = 'W',
        .backward_vk = 'S',
        .left_vk = 'A',
        .right_vk = 'D',
        .escape_vk = VK_ESCAPE,
    };
    KMInput km(hwnd, kmapping);
    kmi = &km;
    dx_init_t init = {
        .width = WIDTH,
        .height = HEIGHT,
        .hwnd = hwnd,
        .buffer_count = 2,
        .copy_workers_count = 5,
        .msaa_type = MSAAType::NONE,
        .rt_clear_color = {0.2f, 0.2f, 0.2f, 1.0f},
        .shaders_dir = "shaders",
        .textures_dir = "textures",
        .cbv_count = 16,
        .srv_count = 16,
        .uav_count = 16,
    };
    DXFramework dxfw(init);
    p_dxfw = &dxfw;

    auto& resmgr = dxfw.GetResourceManager();
    // Load and bind textures
    auto metal_img = dxfw.GetTextureLoader().LoadTextureIntoMemory("metal");
    auto stone_img = dxfw.GetTextureLoader().LoadTextureIntoMemory("stone");
    auto metal_tex = resmgr.CreateTexture(metal_img.value());
    auto stone_tex = resmgr.CreateTexture(stone_img.value());
    dxfw.GetBindlessHeap().BindShaderResource("metal", metal_tex);
    dxfw.GetBindlessHeap().BindShaderResource("stone", stone_tex);

    using ObjectDrawcall = decltype(dxfw)::ObjectDrawcall;

    // Declare vertices and indices
    ObjectDrawcall::Vertex v_Pyramid[16] = {
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

    ObjectDrawcall::Index i_Pyramid[18] = {
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
    for (int i = 0; i < count_of(i_Pyramid) / 3; ++i) {
        uint32_t a = i_Pyramid[i * 3 + 0];
        uint32_t b = i_Pyramid[i * 3 + 1];
        uint32_t c = i_Pyramid[i * 3 + 2];
        XMFLOAT3 normal;
        GenerateNormal(reinterpret_cast<XMFLOAT3*>(&v_Pyramid[a].Position), reinterpret_cast<XMFLOAT3*>(&v_Pyramid[b].Position), reinterpret_cast<XMFLOAT3*>(&v_Pyramid[c].Position), &normal);
        memcpy(&v_Pyramid[a].Normal, &normal, sizeof(float) * 3);
        memcpy(&v_Pyramid[b].Normal, &normal, sizeof(float) * 3);
        memcpy(&v_Pyramid[c].Normal, &normal, sizeof(float) * 3);
    }

    // Create and bind const buffers
    auto scene = resmgr.CreateConstantBuffer<ObjectDrawcall::Scene>();
    auto* p_Scene = scene.GetMapping<ObjectDrawcall::Scene>();
    p_Scene->dotlight_count = 1;
    p_Scene->dotlight_positions[0] = { 4.0f, 2.0f, 0.0f, 0.0f };
    p_Scene->dotlight_colors[0] = { 0.5f, 0.4f, 0.4f, 0.0f };
    p_Scene->camera_position = camera.GetCameraPos4();
    camera.MakeViewAndProjection(p_Scene->vp);
    dxfw.GetBindlessHeap().BindConstantBuffer("scene", scene);

    // Create Objects
    ObjectDrawcall pyramid(v_Pyramid, count_of(v_Pyramid), i_Pyramid, count_of(i_Pyramid), dxfw, "stone");

    PerformanceCounter pc;
    RunLoop([&] {
        float delta = pc.DeltaMs();
        auto& render_context = dxfw.GetRenderContext();
        km.UpdateFreeCamera(camera);
        camera.MakeViewAndProjection(p_Scene->vp);
        p_Scene->camera_position = camera.GetCameraPos4();
        render_context.Render([&] {
            pyramid(0, 0, 0, 1);
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
                    ShowCursor(true);
                }
            }
            else {
                if (frc != nullptr) {
                    frc->IsFocus() = true;
                    ShowCursor(false);
                }
            }
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}