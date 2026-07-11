#include <base.h>
#include <render/framework.h>

#include "io/model.h"
#include "piplines/realistic_scene.h"
#include "piplines/realistic_scene.h"
#include "piplines/ui.h"
#include "transform/helper.h"

using namespace Prism;

class PipelineDemoApp : public PrismApp {
private:
    PerformanceCounter perf_;
    UIFramework ui_;
    RealisticScene scene_;
public:
    PipelineDemoApp(const dx_init_t& init, Device& device, DXAllocator* allocator) : PrismApp(init, device, allocator), ui_(this), scene_(this) {
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

        std::vector<ObjectDrawcall::Vertex> v_Cube = {
            // Front face (+Z)
            {{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f}, { 0.0f,  0.0f,  1.0f}},  // 0
            {{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f}, { 0.0f,  0.0f,  1.0f}},  // 1
            {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f}, { 0.0f,  0.0f,  1.0f}},  // 2
            {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f}, { 0.0f,  0.0f,  1.0f}},  // 3
            // Back face (-Z)
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}, { 0.0f,  0.0f, -1.0f}},  // 4
            {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f}, { 0.0f,  0.0f, -1.0f}},  // 5
            {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f}, { 0.0f,  0.0f, -1.0f}},  // 6
            {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f}, { 0.0f,  0.0f, -1.0f}},  // 7
            // Right face (+X)
            {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}, { 1.0f,  0.0f,  0.0f}},  // 8
            {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f}, { 1.0f,  0.0f,  0.0f}},  // 9
            {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f}, { 1.0f,  0.0f,  0.0f}},  // 10
            {{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f}, { 1.0f,  0.0f,  0.0f}},  // 11
            // Left face (-X)
            {{-0.5f, -0.5f,  0.5f}, {1.0f, 1.0f}, {-1.0f,  0.0f,  0.0f}},  // 12
            {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f}, {-1.0f,  0.0f,  0.0f}},  // 13
            {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f}, {-1.0f,  0.0f,  0.0f}},  // 14
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}, {-1.0f,  0.0f,  0.0f}},  // 15
            // Top face (+Y)
            {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f}, { 0.0f,  1.0f,  0.0f}},  // 16
            {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f}, { 0.0f,  1.0f,  0.0f}},  // 17
            {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f}, { 0.0f,  1.0f,  0.0f}},  // 18
            {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f}, { 0.0f,  1.0f,  0.0f}},  // 19
            // Bottom face (-Y)
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}, { 0.0f, -1.0f,  0.0f}},  // 20
            {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f}, { 0.0f, -1.0f,  0.0f}},  // 21
            {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f}, { 0.0f, -1.0f,  0.0f}},  // 22
            {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f}, { 0.0f, -1.0f,  0.0f}},  // 23
        };

        std::vector<ObjectDrawcall::Index> i_Cube = {
            0, 1, 2,  0, 2, 3,
            4, 5, 6,  4, 6, 7,
            8, 9,10,  8,10,11,
           12,13,14, 12,14,15,
           16,17,18, 16,18,19,
           20,21,22, 20,22,23,
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

        auto [v_Sphere, i_Sphere] = GeometryGenerator::GenUVSphere(30);
        std::vector<LightSrcDrawcall::Index> i_LightSrc = std::move(i_Sphere);
        std::vector<LightSrcDrawcall::Vertex> v_LightSrc(v_Sphere.size());
        for (size_t i = 0; i < v_Sphere.size(); ++i) {
            v_LightSrc[i].Position = { v_Sphere[i].x, v_Sphere[i].y, v_Sphere[i].z };
        }

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
        auto* oak_tex = tex_loader_.Get("oak").value();
        auto* marble_tex = tex_loader_.Get("marble").value();
        auto* bricks_tex = tex_loader_.Get("bricks").value();

        GenNormal(v_Pyramid, i_Pyramid);
        GenNormal(v_Cube, i_Cube);
        GenNormal(v_Ground, i_Ground);
        scene_.GetAmbientLight() = { 0.0f, 0.0f, 0.0f, 1.0f };
        ObjectDrawcall::ObjectProperties pyramid_prop = ObjectDrawcall::ObjectProperties::MakeObjectProp(0, 0, 0, 2, marble_tex);
        scene_.CreateObjectDrawcall("Pyramid", v_Pyramid, i_Pyramid, pyramid_prop);
        ObjectDrawcall::ObjectProperties cube_prop = ObjectDrawcall::ObjectProperties::MakeObjectProp(2, 1, -1, 1, oak_tex);
        scene_.CreateObjectDrawcall("Cube", v_Cube, i_Cube, cube_prop);
        ObjectDrawcall::ObjectProperties ground_prop = ObjectDrawcall::ObjectProperties::MakeObjectProp(0, -3, 0, 20, bricks_tex);
        scene_.CreateObjectDrawcall("Ground", v_Ground, i_Ground, ground_prop);
        LightSrcDrawcall::LightSrcProperties light_src_prop = LightSrcDrawcall::LightSrcProperties::MakeLightSrcProp(5, 3, 2, 1, { 1.0f, 1.0f, 1.0f, 1.0f });
        scene_.CreateLightSrcDrawcall("LightSrc", v_LightSrc, i_LightSrc, light_src_prop);
    }

    void Loop(MetronomeTimer& mt) override {
        while (window_.FetchMessage()) {}
        scene_.Update();
        auto* light_src_drawcall = scene_.GetDrawcall<LightSrcDrawcall>("LightSrc");
        auto* pyramid_drawcall = scene_.GetDrawcall<ObjectDrawcall>("Pyramid");
        auto* cube_drawcall = scene_.GetDrawcall<ObjectDrawcall>("Cube");
        auto* ground_drawcall = scene_.GetDrawcall<ObjectDrawcall>("Ground");
        pyramid_drawcall->ApplyProperties();
        cube_drawcall->ApplyProperties();
        ground_drawcall->ApplyProperties();
        ui_.DrawString("UbuntuMono", "Realistic PBR Pipeline Demo", 30, 24, 24, { 1.0f, 1.0f, 1.0f, 1.0f });
        ui_.DrawString("UbuntuMono", "Prism Renderer using DirectX12 API", 30, 50, 16, { 1.0f, 1.0f, 1.0f, 1.0f });
        ui_.DrawString("UbuntuMono", std::format("Current FPS: {}", perf_.QueryFPS()), 30, 70, 16, { 0.0f, 1.0f, 0.0f, 1.0f });
        ui_.DrawString("UbuntuMono", "This is a demo that shows basic pipeline. Visit https://github.com/youfantan/Prism to learn more", 30, 90, 16, { 1.0f, 1.0f, 0.0f, 1.0f });
        auto& render_pass = scene_.GetRenderPass();
        auto& shadow_pass = scene_.GetShadowPass();
        RecordDispatcher::RecordProcess perf_sync = {
            [&](auto list, auto nfv) {
                return nullptr;
            },
            [&](void*) {
                perf_.DeltaMs();
            }
        };
        shadow_pass({ shadow_pass.GetInitProc(),
            pyramid_drawcall->CreateShadowProcess(),
            cube_drawcall->CreateShadowProcess(),
            shadow_pass.GetSyncProc() });
        render_pass({ render_pass.GetInitProc(),
            light_src_drawcall->CreateRenderProcess(),
            pyramid_drawcall->CreateRenderProcess(),
            cube_drawcall->CreateRenderProcess(),
            ground_drawcall->CreateRenderProcess(),
            ui_.CreateRenderProcess(),
            render_pass.GetSyncProc(),
            perf_sync
        });
        ctx_.Swap();
    }

    void OnKeyUp(WPARAM wparam) override {

    }

    void OnKeyDown(WPARAM wparam) override {

    }

    void OnActive(WPARAM wparam) override {
        if (LOWORD(wparam) == WA_INACTIVE) {
            scene_.GetCamera().IsFocus() = false;
            ShowCursor(true);
        } else {
            scene_.GetCamera().IsFocus() = true;
            ShowCursor(false);
        }

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
        .lists_per_render_thread = 2,
        .lists_per_copy_thread = 5,
        .fps_limit = 1000,
        .max_texture_count = 128,
        .msaa_type = MSAAType::MSAA_4X,
        .rt_format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .ds_format = DXGI_FORMAT_D32_FLOAT,
        .rt_clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
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