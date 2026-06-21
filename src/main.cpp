#include <functional>
#include <string>
#include <Windows.h>

#include "mlog.h"
#include "io/font.h"
#include "io/shader.h"
#include "io/texture.h"
#include "render/framework.h"
#include "render/ui.h"

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

FreeCamera* frc = nullptr;
KMInput* kmi;

DXFramework<DXDefaultAllocator>* p_dxfw;

int main() {
#ifndef NDEBUG
    mlog_enable_win32_vansi();
#endif

    mlog_sth_init_t mlog_init {
        .log_directory = "logs",
        .log_file_name = "logs.txt"
    };
    mlog_sth_init(mlog_init);
    FontTexGenerator loader("assets", "UbuntuMono");
    loader.GenerateFontTexAndUV(128);
    HWND hwnd = InitWin32Window(nullptr, true);
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
    dx_init_t dx_init = {
        .width = WIDTH,
        .height = HEIGHT,
        .hwnd = hwnd,
        .buffer_count = 2,
        .copy_workers_count = 5,
        .msaa_type = MSAAType::MSAA_4X,
        .rt_format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .ds_format = DXGI_FORMAT_D32_FLOAT,
        .rt_clear_color = {0.2f, 0.2f, 0.2f, 1.0f},
        .enable_vsync = true,
        .shaders_dir = "shaders",
        .textures_dir = "textures",
        .assets_dir = "assets",
        .cbv_count = 16,
        .srv_count = 16,
        .uav_count = 16,
    };

    ui_init_t ui_init {
        .load_fonts = {
            "UbuntuMono"
        }
    };

    DXFramework<DXDefaultAllocator> dxfw(dx_init);
    UIFramework uifw(ui_init, &dxfw);
    p_dxfw = &dxfw;

    auto& resmgr = dxfw.GetResourceManager();
    // Load and bind textures
    auto metal_img = dxfw.GetTextureLoader().LoadTextureIntoMemory("metal");
    auto stone_img = dxfw.GetTextureLoader().LoadTextureIntoMemory("stone");
    auto metal_tex = resmgr.CreateTexture("tex_metal", metal_img.value());
    auto stone_tex = resmgr.CreateTexture("tex_stone", stone_img.value());
    dxfw.GetBindlessHeap().BindTexture("tex_metal");
    dxfw.GetBindlessHeap().BindTexture("tex_stone");

    using ObjectDrawcall = decltype(dxfw)::ObjectDrawcall;

    // Declare vertices and indices
    std::vector<ObjectDrawcall::Vertex> v_Pyramid = {
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

    std::vector<ObjectDrawcall::Index> i_Pyramid = {
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

    std::vector<ObjectDrawcall::Vertex> v_Ground = {
        {{ -0.5f, 0.0f, 0.5f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }},
        {{ 0.5f, 0.0f, 0.5f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }},
        {{ 0.5f, 0.0f, -0.5f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }},
        {{ -0.5f, 0.0f, -0.5f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }},
    };

    std::vector<ObjectDrawcall::Index> i_Ground = {
        0, 1, 2,
        0, 2, 3
    };

    auto GenNormal = [](std::vector<ObjectDrawcall::Vertex>& v, std::vector<ObjectDrawcall::Index>& idx) {
        for (int i = 0; i < idx.size() / 3; ++i) {
            uint32_t a = idx[i * 3 + 0];
            uint32_t b = idx[i * 3 + 1];
            uint32_t c = idx[i * 3 + 2];
            XMFLOAT3 normal;
            GenerateNormal(reinterpret_cast<XMFLOAT3*>(&v[a].Position), reinterpret_cast<XMFLOAT3*>(&v[b].Position), reinterpret_cast<XMFLOAT3*>(&v[c].Position), &normal);
            memcpy(&v[a].Normal, &normal, sizeof(float) * 3);
            memcpy(&v[b].Normal, &normal, sizeof(float) * 3);
            memcpy(&v[c].Normal, &normal, sizeof(float) * 3);
        }
    };
    GenNormal(v_Pyramid, i_Pyramid);
    GenNormal(v_Ground, i_Ground);

    // Create and bind const buffers
    auto scene = resmgr.CreateConstantBuffer<ObjectDrawcall::Scene>("scene");
    auto* p_Scene = scene.value()->GetMapping<ObjectDrawcall::Scene>();
    p_Scene->dotlight_count = 1;
    p_Scene->dotlight_positions[0] = { 0.0f, 4.0f, 0.0f, 0.0f };
    p_Scene->dotlight_colors[0] = { 0.7f, 0.7f, 0.7f, 0.0f };
    p_Scene->camera_position = camera.GetCameraPos4();
    camera.MakeViewAndProjection(p_Scene->vp);
    dxfw.GetBindlessHeap().BindConstantBuffer("scene");
    uint32_t tex_stone_index = dxfw.GetBindlessHeap().QueryResourceIndex("tex_stone");
    uint32_t tex_metal_index = dxfw.GetBindlessHeap().QueryResourceIndex("tex_metal");
    uint32_t cb_scene_index = dxfw.GetBindlessHeap().QueryResourceIndex("scene");

    // Create Objects
    ObjectDrawcall pyramid("pyramid", v_Pyramid, i_Pyramid, &dxfw, tex_metal_index, cb_scene_index);
    ObjectDrawcall ground("ground", v_Ground, i_Ground, &dxfw, tex_stone_index, cb_scene_index);
    auto& text_draw = uifw.GetTextDrawcall();
    PerformanceCounter pc;
    RunLoop([&] {
        float delta = pc.DeltaMs();
        auto& render_context = dxfw.GetRenderContext();
        km.UpdateFreeCamera(camera);
        camera.MakeViewAndProjection(p_Scene->vp);
        p_Scene->camera_position = camera.GetCameraPos4();
        render_context.Render([&](RenderPass& rp) {
            pyramid(rp, 0, 0, 0, 1);
            ground(rp, 0, -2, 0, 5);
            text_draw(rp, "UbuntuMono", L"Prism Renderer using DirectX12 API", 30, 30, 16, { 1.0f, 1.0f, 1.0f, 1.0f });
            text_draw(rp, "UbuntuMono", std::format(L"Current FPS: {}", pc.QueryFPS()), 30, 50, 16, { 0.0f, 1.0f, 0.0f, 1.0f });
            text_draw(rp, "UbuntuMono", L"This is a demo that shows basic pipeline. To learn more, visit https://github.com/youfantan/Prism", 30, 70, 16, { 1.0f, 1.0f, 0.0f, 1.0f });
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