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
        XMFLOAT4 ambient_light;
        XMFLOAT4 camera_position;
        uint32_t dotlight_count;
        float _padding0[3];
        XMFLOAT4 dotlight_positions[4];
        XMFLOAT4 dotlight_colors[4];
        uint32_t shadow_maps[4];
    };

    class LightSrcDrawcall : public Drawcall {
    public:
        using Vertex = struct {
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
        using VertexAttrs = InputAttrs<PositionAttr<0>, TexCoord0Attr<0>>;
        using InstanceAttrs = InputAttrs<>;

        struct LightInfo {
            XMFLOAT4X4 world;
            XMFLOAT4 color;
            XMFLOAT4 position;
        };

        class LightSrcPipeline : public Pipeline {
            PrismApp* app_;
        public:
            explicit LightSrcPipeline(PrismApp* app);
        };

    private:
        PrismApp* app_;
        LightSrcPipeline* pipeline_;
        CompactMesh* mesh_;
        ResourceHandle light_info_buffer_;
        ResourceHandle scene_info_;
        uint16_t bind_to_index_;
        LightInfo light_info_ {};

    public:
        LightSrcDrawcall(PrismApp* app, ResourceHandle scene, const std::string& name, CompactMesh* mesh, uint16_t bind_to) : app_(app), scene_info_(scene), mesh_(mesh), bind_to_index_(bind_to) {
            pipeline_ = app_->GetPipelineManager().GetPipeline<LightSrcPipeline>("LightSrcPipeline");
            light_info_buffer_ = app_->GetResourceManager().CreateLocalBuffer<LightInfo>("LightInfo_" + name, D3D12_RESOURCE_STATE_COMMON);
            StructuredView<LightInfo> light_info(light_info_buffer_);
            StructuredView<RealisticSceneInfo> scene_info(scene_info_);
            for (size_t i = 0; i < app_->GetInitializeParams().buffer_count; ++i) {
                light_info.SelectLayer(i);
                light_info[0].color = light_info_.color;
                light_info[0].world = light_info_.world;
                scene_info.SelectLayer(i);
                scene_info[0].dotlight_positions[bind_to_index_] = light_info_.position;
                scene_info[0].dotlight_colors[bind_to_index_] = light_info_.color;
            }
            for (size_t i = 0; i < app_->GetInitializeParams().buffer_count; ++i) {
                scene_info.SelectLayer(i);
                scene_info[0].dotlight_count++;
            }
        }

        LightSrcDrawcall(const LightSrcDrawcall&) = delete;
        LightSrcDrawcall(LightSrcDrawcall&&) = delete;

        LightInfo& GetLightInfo() {
            return light_info_;
        }

        void ApplyProperties() {
            StructuredView<LightInfo> light_info(light_info_buffer_);
            light_info.SelectLayer(app_->GetRenderContext().GetCurrentIndex());
            light_info[0].color = light_info_.color;
            light_info[0].world = light_info_.world;
            StructuredView<RealisticSceneInfo> scene_info(scene_info_);
            scene_info.SelectLayer(app_->GetRenderContext().GetCurrentIndex());
            scene_info[0].dotlight_positions[bind_to_index_] = light_info_.position;
            scene_info[0].dotlight_colors[bind_to_index_] = light_info_.color;
        }

        RecordDispatcher::RecordProcess CreateRenderProcess() override {
            return {[&, layer = app_->GetRenderContext().GetCurrentIndex()](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                mesh_->RenderSync(nfv);
                light_info_buffer_->GetRenderWaitable().GetFenceValue() = nfv;
                list->SetPipelineState(pipeline_->GetPSO().Get());
                list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                ID3D12DescriptorHeap* heaps[] = { app_->GetResourceManager().GetTextureHeap().GetD3D12Heap() };
                list->SetDescriptorHeaps(1, heaps);
                list->SetGraphicsRootSignature(pipeline_->GetSignature());
                list->SetGraphicsRootDescriptorTable(0, app_->GetResourceManager().GetTextureHeap().GetD3D12Heap()->GetGPUDescriptorHandleForHeapStart());
                list->SetGraphicsRootConstantBufferView(1, scene_info_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                list->SetGraphicsRootConstantBufferView(2, light_info_buffer_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                mesh_->DrawMesh(list);
                return nullptr;
            }, [&](void* ptr) {}};
        }
    };

    class ObjectDrawcall : public Drawcall {
    public:

        using Vertex = struct {
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
        using Instance = struct {
            uint32_t ColorTexIndex;
            uint32_t NormalTexIndex;
            uint32_t RoughTexIndex;
            uint32_t EmissiveTexIndex;
            uint32_t DisplacementTexIndex;
            uint32_t Reserved0;
            uint32_t Reserved1;
            uint32_t Reserved2;
        };

        using VertexAttrs = InputAttrs<PositionAttr<0>, TexCoord0Attr<0>, NormalAttr<0>>;
        using InstanceAttrs = InputAttrs<TextureIndexAttr<1>>;

        struct ObjectInfo {
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

    private:
        PrismApp* app_;
        ShadowRenderPass& shadow_rp_;
        Pipeline* pipeline_;
        ShadowPipeline* shadow_pipeline_;
        ResourceHandle scene_info_;
        CompactMesh* mesh_;
        ResourceHandle object_info_buffer_;
        ObjectInfo object_info_ {};

    public:
        ObjectDrawcall(PrismApp* app, ShadowRenderPass& shadow_rp, ResourceHandle scene_info, const std::string& object_name, CompactMesh* mesh, bool use_pbr) : app_(app), shadow_rp_(shadow_rp), scene_info_(scene_info), mesh_(mesh) {
            shadow_pipeline_ = app_->GetPipelineManager().GetPipeline<ShadowPipeline>("ShadowPipeline");
            object_info_buffer_ = app_->GetResourceManager().CreateLocalBuffer<ObjectInfo>("ObjectInfo_" + object_name, D3D12_RESOURCE_STATE_COMMON);
            if (use_pbr) {
                pipeline_ = app_->GetPipelineManager().GetPipeline<PBRObjectPipeline>("PBRObjectPipeline");
            } else {
                pipeline_ = app_->GetPipelineManager().GetPipeline<ObjectPipeline>("ObjectPipeline");
            }
        }
        ObjectDrawcall(const ObjectDrawcall&) = delete;
        ObjectDrawcall(ObjectDrawcall&&) = delete;

        void ApplyProperties() {
            StructuredView<ObjectInfo> object_info(object_info_buffer_);
            object_info.SelectLayer(app_->GetRenderContext().GetCurrentIndex());
            object_info[0].world = object_info_.world;
        }

        ObjectInfo& GetObjectInfo() {
            return object_info_;
        }

        RecordDispatcher::RecordProcess CreateShadowProcess() {
            return {
                [&, layer = app_->GetRenderContext().GetCurrentIndex()](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                    object_info_buffer_->GetRenderWaitable().GetFenceValue() = nfv;
                    list->SetPipelineState(shadow_pipeline_->GetPSO().Get());
                    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    list->SetGraphicsRootSignature(shadow_pipeline_->GetSignature());
                    list->SetGraphicsRootConstantBufferView(0, scene_info_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                    list->SetGraphicsRootConstantBufferView(1, object_info_buffer_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                    mesh_->DrawMesh(list);
                    return nullptr;
                }
            };
        }

        RecordDispatcher::RecordProcess CreateRenderProcess() override {
            return {[&, layer = app_->GetRenderContext().GetCurrentIndex()](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                mesh_->RenderSync(nfv);
                object_info_buffer_->GetRenderWaitable().GetFenceValue() = nfv;
                scene_info_->GetRenderWaitable().GetFenceValue() = nfv;
                list->SetPipelineState(pipeline_->GetPSO().Get());
                list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                ID3D12DescriptorHeap* heaps[] = { app_->GetResourceManager().GetTextureHeap().GetD3D12Heap() };
                list->SetDescriptorHeaps(1, heaps);
                list->SetGraphicsRootSignature(pipeline_->GetSignature());
                list->SetGraphicsRootDescriptorTable(0, app_->GetResourceManager().GetTextureHeap().GetD3D12Heap()->GetGPUDescriptorHandleForHeapStart());
                list->SetGraphicsRootConstantBufferView(1, scene_info_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                list->SetGraphicsRootConstantBufferView(2, object_info_buffer_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                mesh_->DrawMesh(list);
                return nullptr;
            }};
        }

        ~ObjectDrawcall() override {
            app_->GetResourceManager().MarkAsExpired(object_info_buffer_);
        }
    };

    class ModelDrawcall : public Drawcall {
    public:
        using Vertex = ObjectDrawcall::Vertex;
        using Index = ObjectDrawcall::Index;
        using Instance = ObjectDrawcall::Instance;
        using VertexAttrs = ObjectDrawcall::VertexAttrs;
        using InstanceAttrs = ObjectDrawcall::InstanceAttrs;
        using ObjectInfo = ObjectDrawcall::ObjectInfo;
        using ObjectPipeline = ObjectDrawcall::ObjectPipeline;
        using PBRObjectPipeline = ObjectDrawcall::PBRObjectPipeline;
        using ShadowPipeline = ObjectDrawcall::ShadowPipeline;

    private:
        PrismApp* app_;
        ShadowRenderPass& shadow_rp_;
        Pipeline* pipeline_;
        ShadowPipeline* shadow_pipeline_;
        ResourceHandle scene_info_;
        std::vector<ModelLoader::Drawable> drawables_;
        std::vector<ResourceHandle> object_info_buffer_;
        ObjectInfo object_info_ {};

    public:
        ModelDrawcall(PrismApp* app, ShadowRenderPass& shadow_rp, ResourceHandle scene_info, const std::string& object_name, const std::vector<ModelLoader::Drawable>& drawables, bool use_pbr) : app_(app), shadow_rp_(shadow_rp), scene_info_(scene_info), drawables_(drawables), object_info_buffer_(drawables.size()) {
            shadow_pipeline_ = app_->GetPipelineManager().GetPipeline<ShadowPipeline>("ShadowPipeline");
            for (size_t i = 0; i <  drawables_.size(); ++i) {
                object_info_buffer_[i] = app_->GetResourceManager().CreateLocalBuffer<ObjectInfo>("ObjectInfo_" + object_name, D3D12_RESOURCE_STATE_COMMON);
            }
            if (use_pbr) {
                pipeline_ = app_->GetPipelineManager().GetPipeline<PBRObjectPipeline>("PBRObjectPipeline");
            } else {
                pipeline_ = app_->GetPipelineManager().GetPipeline<ObjectPipeline>("ObjectPipeline");
            }
        }
        ModelDrawcall(const ModelDrawcall&) = delete;
        ModelDrawcall(ModelDrawcall&&) = delete;

        void ApplyProperties() {
            XMMATRIX world = XMLoadFloat4x4(&object_info_.world);
            for (size_t i = 0; i < drawables_.size(); ++i) {
                StructuredView<ObjectInfo> object_info(object_info_buffer_[i]);
                object_info.SelectLayer(app_->GetRenderContext().GetCurrentIndex());
                XMMATRIX model = XMLoadFloat4x4(&drawables_[i].mesh_transform);
                XMStoreFloat4x4(&object_info[0].world, model * world);
            }
        }

        ObjectInfo& GetObjectInfo() {
            return object_info_;
        }

        RecordDispatcher::RecordProcess CreateShadowProcess() {
            return {
                [&, layer = app_->GetRenderContext().GetCurrentIndex()](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                    for (size_t i = 0; i < drawables_.size(); ++i) {
                        object_info_buffer_[i]->GetRenderWaitable().GetFenceValue() = nfv;
                        drawables_[i].mesh->RenderSync(nfv);
                    }
                    list->SetPipelineState(shadow_pipeline_->GetPSO().Get());
                    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    list->SetGraphicsRootSignature(shadow_pipeline_->GetSignature());
                    list->SetGraphicsRootConstantBufferView(0, scene_info_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                    for (size_t i = 0; i < drawables_.size(); ++i) {
                        list->SetGraphicsRootConstantBufferView(1, object_info_buffer_[i]->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                        drawables_[i].mesh->DrawMesh(list);
                    }
                    return nullptr;
                }
            };
        }

        RecordDispatcher::RecordProcess CreateRenderProcess() override {
            return {[&, layer = app_->GetRenderContext().GetCurrentIndex()](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                for (size_t i = 0; i < drawables_.size(); ++i) {
                    object_info_buffer_[i]->GetRenderWaitable().GetFenceValue() = nfv;
                    drawables_[i].mesh->RenderSync(nfv);
                }
                scene_info_->GetRenderWaitable().GetFenceValue() = nfv;
                list->SetPipelineState(pipeline_->GetPSO().Get());
                list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                ID3D12DescriptorHeap* heaps[] = { app_->GetResourceManager().GetTextureHeap().GetD3D12Heap() };
                list->SetDescriptorHeaps(1, heaps);
                list->SetGraphicsRootSignature(pipeline_->GetSignature());
                list->SetGraphicsRootDescriptorTable(0, app_->GetResourceManager().GetTextureHeap().GetD3D12Heap()->GetGPUDescriptorHandleForHeapStart());
                list->SetGraphicsRootConstantBufferView(1, scene_info_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                for (size_t i = 0; i < drawables_.size(); ++i) {
                    list->SetGraphicsRootConstantBufferView(2, object_info_buffer_[i]->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                    drawables_[i].mesh->DrawMesh(list);
                }
                return nullptr;
            }};
        }

        ~ModelDrawcall() override {
            for (size_t i = 0; i < drawables_.size(); ++i) {
                app_->GetResourceManager().MarkAsExpired(object_info_buffer_[i]);
            }
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
        XMFLOAT4 ambient_light_;
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
            scene_info[0].ambient_light = ambient_light_;
            scene_info[0].camera_position = camera_.GetCameraPos4();
            scene_info[0].shadow_maps[0] = shadow_rp_.GetFrameResource(app_->GetRenderContext().GetCurrentIndex()).shadow_map->GetResourceMeta().Tex2D.bind_index;
        }

        XMFLOAT4& GetAmbientLight() {
            return ambient_light_;
        }

        ObjectDrawcall* CreateObjectDrawcall(const std::string& name, CompactMesh* mesh, bool enable_pbr = true) {
            if (drawcalls.contains(name)) {
                LFATAL("Cannot create object drawcall {}: drawcall already exists", name);
            }
            auto* drawcall = new ObjectDrawcall(app_, shadow_rp_, scene_info_, name, mesh, enable_pbr);
            drawcalls[name] = drawcall;
            return dynamic_cast<ObjectDrawcall*>(drawcalls[name]);
        }

        ModelDrawcall* CreateModelDrawcall(const ModelLoader::Model& model, bool enable_pbr = true) {
            if (drawcalls.contains(model.name)) {
                LFATAL("Cannot create model drawcall {}: drawcall already exists", model.name);
            }
            auto* drawcall = new ModelDrawcall(app_, shadow_rp_, scene_info_, model.name, model.drawables, enable_pbr);
            drawcalls[model.name] = drawcall;
            return dynamic_cast<ModelDrawcall*>(drawcalls[model.name]);
        }

        LightSrcDrawcall* CreateLightSrcDrawcall(const std::string& name, CompactMesh* mesh) {
            if (drawcalls.contains(name)) {
                LFATAL("Cannot create light source drawcall {}: drawcall already exists", name);
            }
            uint16_t assigned_index = 0;
            {
                StructuredView<RealisticSceneInfo> sc_info(scene_info_);
                if (sc_info[0].dotlight_count >= 4) {
                    LFATAL("Cannot create light source drawcall {}: light source too much", name);
                }
                assigned_index = sc_info[0].dotlight_count;
            }
            auto* drawcall = new LightSrcDrawcall(app_, scene_info_, name, mesh, assigned_index);
            drawcalls[name] = drawcall;
            return dynamic_cast<LightSrcDrawcall*>(drawcalls[name]);
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
