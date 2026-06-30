#include <base.h>
#include <render/framework.h>

#include "piplines/realistic_style_pipeline.h"
#include "piplines/ui_pipeline.h"
#include "transform/helper.h"

using namespace Prism;

class PipelineDemoApp : public PrismApp {
private:
    PerformanceCounter perf_;
    UIFramework ui_;
    RealisticScene scene_;
public:
    PipelineDemoApp(const dx_init_t& init, Device& device, DXAllocator* allocator) : PrismApp(init, device, allocator), ui_(this), scene_(this) {
        ImageLoader loader;
        auto metal_jpg = loader.LoadJPG<ImageFormatRGBA>("textures/metal.jpg").value();
        auto stone_jpg = loader.LoadJPG<ImageFormatRGBA>("textures/stone.jpg").value();
        ResourceHandle metal_tex = res_mgr_.CreateTexture2DFromImage("Texture_Metal", metal_jpg);
        ResourceHandle stone_tex = res_mgr_.CreateTexture2DFromImage("Texture_Stone", stone_jpg);

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

        ObjectDrawcall::ObjectProperties prop = ObjectDrawcall::ObjectProperties::MakeObjectProp(0, 0, 0, 1, metal_tex);
        scene_.CreateObjectDrawcall("Pyramid", v_Pyramid, i_Pyramid, prop);
    }

    void Loop(MetronomeTimer& mt) override {
        float delta = perf_.DeltaMs();
        auto* pyramid_drawcall = scene_.GetDrawcall<ObjectDrawcall>("Pyramid");
        ui_.DrawString("UbuntuMono", "Prism Renderer using DirectX12 API", 30, 30, 16, { 1.0f, 1.0f, 1.0f, 1.0f });
        ui_.DrawString("UbuntuMono", std::format("Current FPS: {}", perf_.QueryFPS()), 30, 50, 16, { 0.0f, 1.0f, 0.0f, 1.0f });
        ui_.DrawString("UbuntuMono", "This is a demo that shows basic pipeline. To learn more, visit https://github.com/youfantan/Prism", 30, 70, 16, { 1.0f, 1.0f, 0.0f, 1.0f });
        ctx_.Render({pyramid_drawcall->CreateRenderProcess(), ui_.CreateRenderProcess()});
    }
    void OnKeyUp(WPARAM wparam) override {

    }

    void OnKeyDown(WPARAM wparam) override {

    }

    void OnActive(WPARAM wparam) override {
        if (LOWORD(wparam) == WA_INACTIVE) {
            scene_.GetCamera().IsFocus() = false;
            ShowCursor(true);
        }
        else {
            scene_.GetCamera().IsFocus() = true;
            ShowCursor(false);
        }

    }

    void OnClose(WPARAM wparam) override {

    }
};

int main() {
#ifndef NDEBUG
    mlog_enable_win32_vansi();
#endif
    mlog_sth_init_t mlog_init {
        .log_directory = "logs",
        .log_file_name = "logs.txt"
    };
    mlog_sth_init(mlog_init);

    dx_init_t dx_init = {
        .width = 1280,
        .height = 720,
        .buffer_count = 2,
        .render_threads_count = 1,
        .copy_threads_count = 1,
        .lists_per_render_thread = 1,
        .lists_per_copy_thread = 5,
        .fps_limit = 240,
        .max_texture_count = 128,
        .msaa_type = MSAAType::MSAA_4X,
        .rt_format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .ds_format = DXGI_FORMAT_D32_FLOAT,
        .rt_clear_color = {0.2f, 0.2f, 0.2f, 1.0f},
        .enable_vsync = true,
        .shaders_dir = "shaders",
        .textures_dir = "textures",
        .assets_dir = "assets",
    };
    Device device;
    DXDefaultAllocator allocator(device);
    PipelineDemoApp demo(dx_init, device, &allocator);
    demo.RunLoop();
    mlog_sth_close();
    return 0;
}