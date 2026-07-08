#pragma once

#include <base.h>
#include <render/framework.h>
#include <render/mesh.h>
#include <render/drawcall.h>
#include <render/render_pass.h>
#include <transform/camera.h>

namespace Prism
{
    struct RealisticSceneInfo {
        XMFLOAT4X4 vp;
        XMFLOAT4X4 light_vp;
        XMFLOAT4 camera_position;
        uint32_t dotlight_count;
        float _padding0[3];
        XMFLOAT4 dotlight_positions[16];
        XMFLOAT4 dotlight_colors[16];
    };

    struct RenderProperties {
        float x;
        float y;
        float z;
        float scale_x;
        float scale_y;
        float scale_z;
        float rot_x;
        float rot_y;
        float rot_z;
    };

    template<typename Properties>
    requires std::derived_from<Properties, RenderProperties>
    void MakeWorldMatrix(const Properties& prop, XMFLOAT4X4& dest) {
        XMMATRIX S = XMMatrixScaling(prop.scale_x, prop.scale_y, prop.scale_z);
        XMMATRIX R = XMMatrixRotationX(prop.rot_x) * XMMatrixRotationY(prop.rot_y) * XMMatrixRotationZ(prop.rot_z);
        XMMATRIX T = XMMatrixTranslation(prop.x, prop.y, prop.z);
        XMMATRIX world = S * R* T;
        XMStoreFloat4x4(&dest, world);
    }

    class LightSrcDrawcall : public Drawcall {
    public:
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
        };

        using Index = uint32_t;

        constexpr static D3D12_INPUT_ELEMENT_DESC LIGHT_SRC_LAYOUT[2] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        struct LightInfo {
            XMFLOAT4X4 world;
            XMFLOAT4 color;
        };

        struct LightSrcProperties : RenderProperties {
            XMFLOAT4 color;

            static LightSrcProperties MakeLightSrcProp(float x, float y, float z, float size, XMFLOAT4 color) {
                return {
                    {
                        .x = x,
                        .y = y,
                        .z = z,
                        .scale_x = size,
                        .scale_y = size,
                        .scale_z = size,
                        .rot_x = 0.0f,
                        .rot_y = 0.0f,
                        .rot_z = 0.0f,
                    }, color};
            }
        };

        class LightSrcPipeline : public Pipeline {
            PrismApp* app_;
        public:
            explicit LightSrcPipeline(PrismApp* app);
        };

    private:
        PrismApp* app_;
        LightSrcPipeline* pipeline_;
        Mesh<Vertex, Index> mesh_;
        ResourceHandle light_info_;
        ResourceHandle scene_info_;
        LightSrcProperties properties_;
        uint16_t bind_to_index_;

    public:
        LightSrcDrawcall(PrismApp* app, ResourceHandle scene_info, const std::string& name, std::vector<Vertex>& vertices, std::vector<Index>& indices, const LightSrcProperties& prop, uint16_t bind_to) : app_(app), scene_info_(scene_info), mesh_(name, vertices, indices, app_->GetResourceManager()), properties_(prop), bind_to_index_(bind_to) {
            pipeline_ = app_->GetPipelineManager().GetPipeline<LightSrcPipeline>("LightSrcPipeline");
            light_info_ = app_->GetResourceManager().CreateLocalBuffer<LightInfo>("LightInfo_" + name, D3D12_RESOURCE_STATE_COMMON);
            StructuredView<LightInfo> light_info(light_info_);
            StructuredView<RealisticSceneInfo> sc_info(scene_info_);
            for (size_t i = 0; i < app_->GetInitializeParams().buffer_count; ++i) {
                light_info.SelectLayer(i);
                light_info[0].color = properties_.color;
                MakeWorldMatrix(properties_, light_info[0].world);
                sc_info.SelectLayer(i);
                sc_info[0].dotlight_positions[bind_to_index_] = { properties_.x, properties_.y, properties_.z, 0.0f };
                sc_info[0].dotlight_colors[bind_to_index_] = properties_.color;
            }
            for (size_t i = 0; i < app_->GetInitializeParams().buffer_count; ++i) {
                sc_info.SelectLayer(i);
                sc_info[0].dotlight_count++;
            }
        }

        LightSrcDrawcall(const LightSrcDrawcall&) = delete;
        LightSrcDrawcall(LightSrcDrawcall&&) = delete;

        LightSrcProperties& GetProperties() {
            return properties_;
        }

        void ApplyProperties() {
            StructuredView<LightInfo> light_info(light_info_);
            light_info.SelectLayer(app_->GetRenderContext().GetCurrentIndex());
            light_info[0].color = properties_.color;
            MakeWorldMatrix(properties_, light_info[0].world);
            StructuredView<RealisticSceneInfo> scene_info(scene_info_);
            scene_info.SelectLayer(app_->GetRenderContext().GetCurrentIndex());
            scene_info[0].dotlight_positions[bind_to_index_] = { properties_.x, properties_.y, properties_.z, 0.0f };
            scene_info[0].dotlight_colors[bind_to_index_] = properties_.color;
        }

        RecordDispatcher::RecordProcess CreateRenderProcess() override {
            return {[&, layer = app_->GetRenderContext().GetCurrentIndex()](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                mesh_.RenderSync(nfv);
                light_info_->GetRenderWaitable().GetFenceValue() = nfv;
                list->SetPipelineState(pipeline_->GetPSO().Get());
                list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                ID3D12DescriptorHeap* heaps[] = { app_->GetResourceManager().GetTextureHeap().GetD3D12Heap() };
                list->SetDescriptorHeaps(1, heaps);
                list->SetGraphicsRootSignature(pipeline_->GetSignature());
                list->SetGraphicsRootDescriptorTable(0, app_->GetResourceManager().GetTextureHeap().GetD3D12Heap()->GetGPUDescriptorHandleForHeapStart());
                list->SetGraphicsRootConstantBufferView(1, scene_info_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                list->SetGraphicsRootConstantBufferView(2, light_info_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                list->IASetVertexBuffers(0, 1, &mesh_.GetVertexBufferView());
                list->IASetIndexBuffer(&mesh_.GetIndexBufferView());
                list->DrawIndexedInstanced(mesh_.GetIndexBuffer()->GetResourceMeta().Buffer.element_count, 1, 0, 0, 0);
                return nullptr;
            }, [&](void* ptr) {}};
        }
    };

    class ObjectDrawcall : public Drawcall {
    public:
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

        using Index = uint32_t;

        constexpr static D3D12_INPUT_ELEMENT_DESC OBJECT_LAYOUT[3] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        struct ObjectInfo {
            XMFLOAT4X4 world;
            uint32_t tex_index;
            uint32_t shadow_index;
            uint32_t normal_index;
        };

        struct ShadowInfo {
            XMFLOAT4X4 world;
        };

        class ObjectPipeline : public Pipeline {
            PrismApp* app_;
        public:
            explicit ObjectPipeline(PrismApp* app);
        };

        class PBRObjectPipeline : public Pipeline {
            PrismApp* app_;
        public:
            explicit PBRObjectPipeline(PrismApp* app);
        };

        class ShadowPipeline : public Pipeline {
            PrismApp* app_;
        public:
            explicit ShadowPipeline(PrismApp* app);
        };

        struct ObjectProperties : RenderProperties {
            ResourceHandle texture;
            bool pbr_enable;
            ResourceHandle normal_tex;

            static ObjectProperties MakeObjectProp(float x, float y, float z, float size, ResourceHandle texture) {
                return {{
                    .x = x,
                    .y = y,
                    .z = z,
                    .scale_x = size,
                    .scale_y = size,
                    .scale_z = size,
                    .rot_x = 0.0f,
                    .rot_y = 0.0f,
                    .rot_z = 0.0f,
                }, texture, false, nullptr };
            }
            static ObjectProperties MakePBRObjectProp(float x, float y, float z, float size, ResourceHandle texture, ResourceHandle normal_tex) {
                return {{
                    .x = x,
                    .y = y,
                    .z = z,
                    .scale_x = size,
                    .scale_y = size,
                    .scale_z = size,
                    .rot_x = 0.0f,
                    .rot_y = 0.0f,
                    .rot_z = 0.0f,
                }, texture, true, normal_tex };
            }
        };

    private:
        PrismApp* app_;
        ShadowRenderPass& shadow_rp_;
        Pipeline* pipeline_;
        ShadowPipeline* shadow_pipeline_;
        ResourceHandle scene_info_;
        Mesh<Vertex, Index> mesh_;
        ResourceHandle object_info_;
        ResourceHandle shadow_info_;
        ObjectProperties properties_;
        uint64_t sync_value_;

    public:
        ObjectDrawcall(PrismApp* app, ShadowRenderPass& shadow_rp, ResourceHandle scene_info, const std::string& object_name, std::vector<Vertex>& vertices, std::vector<Index>& indices, const ObjectProperties& prop) : app_(app), shadow_rp_(shadow_rp), scene_info_(scene_info), mesh_(object_name, vertices, indices, app->GetResourceManager()), properties_(prop) {
            shadow_pipeline_ = app_->GetPipelineManager().GetPipeline<ShadowPipeline>("ShadowPipeline");
            object_info_ = app_->GetResourceManager().CreateLocalBuffer<ObjectInfo>("ObjectInfo_" + object_name, D3D12_RESOURCE_STATE_COMMON);
            shadow_info_ = app_->GetResourceManager().CreateLocalBuffer<ShadowInfo>("ShadowInfo_" + object_name, D3D12_RESOURCE_STATE_COMMON);
            if (prop.pbr_enable) {
                pipeline_ = app_->GetPipelineManager().GetPipeline<PBRObjectPipeline>("PBRObjectPipeline");
            } else {
                pipeline_ = app_->GetPipelineManager().GetPipeline<ObjectPipeline>("ObjectPipeline");
            }
        }
        ObjectDrawcall(const ObjectDrawcall&) = delete;
        ObjectDrawcall(ObjectDrawcall&&) = delete;

        ObjectProperties& GetProperties() {
            return properties_;
        }

        void ApplyProperties() {
            StructuredView<ObjectInfo> object_info(object_info_);
            object_info.SelectLayer(app_->GetRenderContext().GetCurrentIndex());
            object_info[0].tex_index = properties_.texture->GetResourceMeta().Tex2D.bind_index;
            object_info[0].shadow_index = shadow_rp_.GetFrameResource(app_->GetRenderContext().GetCurrentIndex()).shadow_map->GetResourceMeta().Tex2D.bind_index;
            if (properties_.pbr_enable) {
                object_info[0].normal_index = properties_.normal_tex->GetResourceMeta().Tex2D.bind_index;
            }
            MakeWorldMatrix(properties_, object_info[0].world);
            StructuredView<ShadowInfo> shadow_info(shadow_info_);
            shadow_info.SelectLayer(app_->GetRenderContext().GetCurrentIndex());
            MakeWorldMatrix(properties_, shadow_info[0].world);
        }

        RecordDispatcher::RecordProcess CreateShadowProcess() {
            return {
                [&, layer = app_->GetRenderContext().GetCurrentIndex()](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                    shadow_info_->GetRenderWaitable().GetFenceValue() = nfv;
                    list->SetPipelineState(shadow_pipeline_->GetPSO().Get());
                    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    list->SetGraphicsRootSignature(shadow_pipeline_->GetSignature());
                    list->SetGraphicsRootConstantBufferView(0, scene_info_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                    list->SetGraphicsRootConstantBufferView(1, shadow_info_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                    list->IASetVertexBuffers(0, 1, &mesh_.GetVertexBufferView());
                    list->IASetIndexBuffer(&mesh_.GetIndexBufferView());
                    list->DrawIndexedInstanced(mesh_.GetIndexBuffer()->GetResourceMeta().Buffer.element_count, 1, 0, 0, 0);
                    return nullptr;
                }
            };
        }

        RecordDispatcher::RecordProcess CreateRenderProcess() override {
            return {[&, layer = app_->GetRenderContext().GetCurrentIndex()](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                sync_value_ = nfv;
                mesh_.RenderSync(nfv);
                properties_.texture->GetCopyWaitable().GPUWait();
                properties_.texture->GetRenderWaitable().GetFenceValue() = nfv;
                object_info_->GetRenderWaitable().GetFenceValue() = nfv;
                scene_info_->GetRenderWaitable().GetFenceValue() = nfv;
                list->SetPipelineState(pipeline_->GetPSO().Get());
                list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                ID3D12DescriptorHeap* heaps[] = { app_->GetResourceManager().GetTextureHeap().GetD3D12Heap() };
                list->SetDescriptorHeaps(1, heaps);
                list->SetGraphicsRootSignature(pipeline_->GetSignature());
                list->SetGraphicsRootDescriptorTable(0, app_->GetResourceManager().GetTextureHeap().GetD3D12Heap()->GetGPUDescriptorHandleForHeapStart());
                list->SetGraphicsRootConstantBufferView(1, scene_info_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                list->SetGraphicsRootConstantBufferView(2, object_info_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                list->IASetVertexBuffers(0, 1, &mesh_.GetVertexBufferView());
                list->IASetIndexBuffer(&mesh_.GetIndexBufferView());
                list->DrawIndexedInstanced(mesh_.GetIndexBuffer()->GetResourceMeta().Buffer.element_count, 1, 0, 0, 0);
                return nullptr;
            }};
        }

        ~ObjectDrawcall() override {
            app_->GetResourceManager().MarkAsExpired(object_info_);
        }
    };

    class RealisticScene {
        PrismApp* app_;
        BackBufferRenderPass rp_;
        ShadowRenderPass shadow_rp_;
        LightSrcDrawcall::LightSrcPipeline light_src_pipeline_;
        ResourceHandle scene_info_;
        std::unordered_map<std::string, Drawcall*> drawcalls;
        FreeCamera camera_;
        KMInput input_;
        ShadowCamera sc_;
    public:
        RealisticScene(PrismApp* app) : app_(app), rp_(app->GetRenderContext()), shadow_rp_(app->GetRenderContext()), light_src_pipeline_(app), camera_(app->GetWindow().GetHandle(), app->GetInitializeParams().width, app->GetInitializeParams().height, 45),
        input_(app_->GetWindow().GetHandle(), {
                .forward_vk = 'W',
                .backward_vk = 'S',
                .left_vk = 'A',
                .right_vk = 'D',
                .escape_vk = VK_ESCAPE,
            })
        {
            scene_info_ = app_->GetResourceManager().CreateLocalBuffer<RealisticSceneInfo>("RealisticSceneInfo", D3D12_RESOURCE_STATE_COMMON);
            app_->GetPipelineManager().CreatePipeline<ObjectDrawcall::ObjectPipeline>("ObjectPipeline", app);
            app_->GetPipelineManager().CreatePipeline<ObjectDrawcall::PBRObjectPipeline>("PBRObjectPipeline", app);
            app_->GetPipelineManager().CreatePipeline<ObjectDrawcall::ShadowPipeline>("ShadowPipeline", app);
            app_->GetPipelineManager().CreatePipeline<LightSrcDrawcall::LightSrcPipeline>("LightSrcPipeline", app);
        }

        RealisticScene(const RealisticScene&) = delete;
        RealisticScene(RealisticScene&&) = delete;

        void Update() {
            input_.UpdateFreeCamera(camera_);
            StructuredView<RealisticSceneInfo> scene_info(scene_info_);
            scene_info.SelectLayer(app_->GetRenderContext().GetCurrentIndex());
            camera_.MakeViewAndProjection(scene_info[0].vp);
            sc_.SetSceneBounds(0, -1.5, 0, 15);
            sc_.Update(camera_, scene_info[0].dotlight_positions[0]);
            sc_.MakeLightVP(scene_info[0].light_vp);
            scene_info[0].camera_position = camera_.GetCameraPos4();
        }

        ObjectDrawcall* CreateObjectDrawcall(const std::string& name, std::vector<ObjectDrawcall::Vertex>& vertices, std::vector<ObjectDrawcall::Index>& indices, const ObjectDrawcall::ObjectProperties& prop) {
            if (drawcalls.contains(name)) {
                LFATAL("Cannot create object drawcall {}: drawcall already exists");
            }
            auto* drawcall = new ObjectDrawcall(app_, shadow_rp_, scene_info_, name, vertices, indices, prop);
            drawcalls[name] = drawcall;
            return static_cast<ObjectDrawcall*>(drawcalls[name]);
        }

        LightSrcDrawcall* CreateLightSrcDrawcall(const std::string& name, std::vector<LightSrcDrawcall::Vertex>& vertices, std::vector<LightSrcDrawcall::Index>& indices, const LightSrcDrawcall::LightSrcProperties& prop) {
            if (drawcalls.contains(name)) {
                LFATAL("Cannot create light source drawcall {}: drawcall already exists");
            }
            uint16_t assigned_index = 0;
            {
                StructuredView<RealisticSceneInfo> sc_info(scene_info_);
                if (sc_info[0].dotlight_count >= 16) {
                    LFATAL("Cannot create light source drawcall {}: light source too much");
                }
                assigned_index = sc_info[0].dotlight_count;
            }
            auto* drawcall = new LightSrcDrawcall(app_, scene_info_, name, vertices, indices, prop, assigned_index);
            drawcalls[name] = drawcall;
            return static_cast<LightSrcDrawcall*>(drawcalls[name]);
        }

        template<typename T>
        requires std::derived_from<T, Drawcall>
        T* GetDrawcall(const std::string& name) {
            return static_cast<T*>(drawcalls.at(name));
        }

        FreeCamera& GetCamera() {
            return camera_;
        }

        BackBufferRenderPass& GetRenderPass() {
            return rp_;
        }

        ShadowRenderPass& GetShadowPass() {
            return shadow_rp_;
        }

        ~RealisticScene() {
            app_->GetResourceManager().MarkAsExpired(scene_info_);
            app_->GetCopyDispatcher().ReleaseSync();
            app_->GetRenderDispatcher().ReleaseSync();
            for (auto& pair : drawcalls) {
                delete pair.second;
                LDEBUG("Drawcall {} is released", pair.first);
            }
        }
    };
}
