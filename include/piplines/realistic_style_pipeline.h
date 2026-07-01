#pragma once

#include <base.h>
#include <render/framework.h>
#include <render/mesh.h>
#include <render/drawcall.h>

namespace Prism
{
    struct RealisticSceneInfo {
        XMFLOAT4X4 vp;
        XMFLOAT4 camera_position;
        uint32_t dotlight_count;
        float _padding0[3];
        XMFLOAT4 dotlight_positions[16];
        XMFLOAT4 dotlight_colors[16];
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
        };

        class ObjectPipeline : public Pipeline {
        private:
            PrismApp* app_;
        public:
            explicit ObjectPipeline(PrismApp* app) : app_(app) {
                D3D12_ROOT_SIGNATURE_DESC rs_desc {};
                D3D12_STATIC_SAMPLER_DESC samplers[1] = { LINEAR_SAMPLER_DESC<0, 0> };
                rs_desc.pStaticSamplers = &samplers[0];
                rs_desc.NumStaticSamplers = CountOf(samplers);

                std::vector<D3D12_ROOT_PARAMETER> params;
                auto [pranges, nranges] = app_->GetResourceManager().GetTextureHeap().GetDescriptorRanges();
                D3D12_ROOT_PARAMETER bindless_rp {};
                bindless_rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                bindless_rp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                bindless_rp.DescriptorTable.pDescriptorRanges = pranges;
                bindless_rp.DescriptorTable.NumDescriptorRanges = nranges;
                D3D12_ROOT_PARAMETER scene_rp {};
                scene_rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
                scene_rp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                scene_rp.Descriptor.ShaderRegister = 0;
                D3D12_ROOT_PARAMETER obj_info_rp {};
                obj_info_rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
                obj_info_rp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                obj_info_rp.Descriptor.ShaderRegister = 1;
                params.push_back(bindless_rp);
                params.push_back(scene_rp);
                params.push_back(obj_info_rp);
                rs_desc.pParameters = &params[0];
                rs_desc.NumParameters = params.size();
                rs_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                    | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
                ComPtr<ID3DBlob> err, rs;
                HRESULT hr = D3D12SerializeRootSignature(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rs, &err);
                if (!SUCCEEDED(hr)) {
                    LFATAL("Cannot serialize root signature when create object drawcall: {}", static_cast<char*>(err->GetBufferPointer()));
                    exit(EXIT_FAILURE);
                }
                app_->GetDevice().GetComPtr()->CreateRootSignature(0, rs->GetBufferPointer(), rs->GetBufferSize(), IID_PPV_ARGS(&sign_));
                pso_desc_.pRootSignature = sign_.Get();
                pso_desc_.BlendState = {
                    .AlphaToCoverageEnable = FALSE,
                    .IndependentBlendEnable = FALSE,
                    .RenderTarget = {
                        D3D12_RENDER_TARGET_BLEND_DESC {
                            .BlendEnable = true,
                            .LogicOpEnable = false,
                            .SrcBlend = D3D12_BLEND_SRC_ALPHA,
                            .DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
                            .BlendOp = D3D12_BLEND_OP_ADD,
                            .SrcBlendAlpha = D3D12_BLEND_ONE,
                            .DestBlendAlpha = D3D12_BLEND_ZERO,
                            .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                            .LogicOp = D3D12_LOGIC_OP_NOOP,
                            .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
                        },
                    },
                };
                pso_desc_.RasterizerState = {
                    .FillMode = D3D12_FILL_MODE_SOLID,
                    .CullMode = D3D12_CULL_MODE_BACK,
                    .FrontCounterClockwise = false,
                    .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
                    .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
                    .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
                    .DepthClipEnable = true,
                    .MultisampleEnable = app_->GetInitializeParams().msaa_type != MSAAType::NONE,
                    .AntialiasedLineEnable = false,
                    .ForcedSampleCount = 0,
                    .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
                };
                pso_desc_.DepthStencilState = {
                    .DepthEnable = true,
                    .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
                    .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
                    .StencilEnable = false,
                    .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
                    .StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
                    .FrontFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS },
                    .BackFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS }
                };
                pso_desc_.SampleDesc = GetSampleDesc(app_->GetInitializeParams().msaa_type);
                auto [vs_ptr, vs_len] = app_->GetShaderLoader().LoadShader("object", ShaderType::VertexShader).value();
                auto [ps_ptr, ps_len] = app_->GetShaderLoader().LoadShader("object", ShaderType::PixelShader).value();
                pso_desc_.VS = {vs_ptr, vs_len};
                pso_desc_.PS = {ps_ptr, ps_len};
                pso_desc_.NumRenderTargets = 1;
                pso_desc_.RTVFormats[0] = app_->GetInitializeParams().rt_format;
                pso_desc_.DSVFormat = app_->GetInitializeParams().ds_format;
                pso_desc_.NodeMask = 0;
                pso_desc_.InputLayout = { OBJECT_LAYOUT, CountOf(OBJECT_LAYOUT) };
                pso_desc_.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
                pso_desc_.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
                pso_desc_.SampleMask = UINT_MAX;
                app_->GetDevice().GetComPtr()->CreateGraphicsPipelineState(&pso_desc_, IID_PPV_ARGS(&pso_));
                pso_->SetName(L"Object Pipeline");
            }
        };

        struct ObjectProperties {
            float x;
            float y;
            float z;
            float scale_x;
            float scale_y;
            float scale_z;
            float rot_x;
            float rot_y;
            float rot_z;
            ResourceHandle texture;

            static ObjectProperties MakeObjectProp(float x, float y, float z, float size, ResourceHandle texture) {
                return {
                    .x = x,
                    .y = y,
                    .z = z,
                    .scale_x = size,
                    .scale_y = size,
                    .scale_z = size,
                    .rot_x = 0.0f,
                    .rot_y = 0.0f,
                    .rot_z = 0.0f,
                    .texture = texture
                };
            }
        };

        void MakeWorldMatrix(const ObjectProperties& prop, XMFLOAT4X4& dest) {
            XMMATRIX S = XMMatrixScaling(prop.scale_x, prop.scale_y, prop.scale_z);
            XMMATRIX R = XMMatrixRotationX(prop.rot_x) * XMMatrixRotationY(prop.rot_y) * XMMatrixRotationZ(prop.rot_z);
            XMMATRIX T = XMMatrixTranslation(prop.x, prop.y, prop.z);
            XMMATRIX world = S * R* T;
            XMStoreFloat4x4(&dest, world);
        }

    private:
        PrismApp* app_;
        ObjectPipeline& pipeline_;
        ResourceHandle scene_info_;
        Mesh<Vertex, Index> mesh_;
        ResourceHandle object_info_;
        ObjectProperties properties_;
        uint64_t sync_value_;

    public:
        ObjectDrawcall(PrismApp* app, ObjectPipeline& pipeline, ResourceHandle scene_info, const std::string& object_name, std::vector<Vertex>& vertices, std::vector<Index>& indices, const ObjectProperties& prop) : app_(app), pipeline_(pipeline), scene_info_(scene_info), mesh_(object_name, vertices, indices, app->GetResourceManager()), properties_(prop) {
            object_info_ = app_->GetResourceManager().CreateLocalBuffer<ObjectInfo>("ObjectInfo_" + object_name, D3D12_RESOURCE_STATE_COMMON);
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
            MakeWorldMatrix(properties_, object_info[0].world);
        }

        RecordDispatcher::GPUProcess CreateRenderProcess() override {
            return {[&, layer = app_->GetRenderContext().GetCurrentIndex()](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                sync_value_ = nfv;
                mesh_.RenderSync(nfv);
                properties_.texture->GetCopyWaitable().GPUWait();
                properties_.texture->GetRenderWaitable().GetFenceValue() = nfv;
                object_info_->GetRenderWaitable().GetFenceValue() = nfv;
                scene_info_->GetRenderWaitable().GetFenceValue() = nfv;
                list->SetPipelineState(pipeline_.GetPSO().Get());
                list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                ID3D12DescriptorHeap* heaps[] = { app_->GetResourceManager().GetTextureHeap().GetD3D12Heap() };
                list->SetDescriptorHeaps(1, heaps);
                list->SetGraphicsRootSignature(pipeline_.GetSignature().Get());
                list->SetGraphicsRootDescriptorTable(0, app_->GetResourceManager().GetTextureHeap().GetD3D12Heap()->GetGPUDescriptorHandleForHeapStart());
                list->SetGraphicsRootConstantBufferView(1, scene_info_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                list->SetGraphicsRootConstantBufferView(2, object_info_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                list->IASetVertexBuffers(0, 1, &mesh_.GetVertexBufferView());
                list->IASetIndexBuffer(&mesh_.GetIndexBufferView());
                list->DrawIndexedInstanced(mesh_.GetIndexBuffer()->GetResourceMeta().Buffer.element_count, 1, 0, 0, 0);
                return nullptr;
            }, [&](void* ptr) {}};
        }

        ~ObjectDrawcall() override {
            app_->GetResourceManager().MarkAsExpired(object_info_);
        }
    };

    class RealisticScene {
        PrismApp* app_;
        ObjectDrawcall::ObjectPipeline pipeline_;
        ResourceHandle scene_info_;
        std::unordered_map<std::string, Drawcall*> drawcalls;
        FreeCamera camera_;
        KMInput input_;
    public:
        RealisticScene(PrismApp* app) : app_(app), pipeline_(app), camera_(app->GetWindow().GetHandle(), app->GetInitializeParams().width, app->GetInitializeParams().height, 45),
        input_(app_->GetWindow().GetHandle(), {
                .forward_vk = 'W',
                .backward_vk = 'S',
                .left_vk = 'A',
                .right_vk = 'D',
                .escape_vk = VK_ESCAPE,
            })
        {
            scene_info_ = app_->GetResourceManager().CreateLocalBuffer<RealisticSceneInfo>("RealisticSceneInfo", D3D12_RESOURCE_STATE_COMMON);
        }

        RealisticScene(const RealisticScene&) = delete;
        RealisticScene(RealisticScene&&) = delete;

        void Update() {
            input_.UpdateFreeCamera(camera_);
            StructuredView<RealisticSceneInfo> viewer(scene_info_);
            viewer.SelectLayer(app_->GetRenderContext().GetCurrentIndex());
            camera_.MakeViewAndProjection(viewer[0].vp);
            viewer[0].dotlight_count = 1;
            viewer[0].camera_position = camera_.GetCameraPos4();
            viewer[0].dotlight_colors[0] = { 0.7f, 0.7f, 0.7f, 0.0f };
            viewer[0].dotlight_positions[0] = { 0.0f, 4.0f, 0.0f, 0.0f };
        }

        ObjectDrawcall* CreateObjectDrawcall(const std::string& name, std::vector<ObjectDrawcall::Vertex>& vertices, std::vector<ObjectDrawcall::Index>& indices, const ObjectDrawcall::ObjectProperties& prop) {
            if (drawcalls.contains(name)) {
                LFATAL("Cannot create object drawcall {}: drawcall already exists");
            }
            auto* drawcall = new ObjectDrawcall(app_, pipeline_, scene_info_, name, vertices, indices, prop);
            drawcalls[name] = drawcall;
            return static_cast<ObjectDrawcall*>(drawcalls[name]);
        }

        template<typename T>
        requires std::derived_from<T, Drawcall>
        T* GetDrawcall(const std::string& name) {
            return static_cast<T*>(drawcalls.at(name));
        }

        FreeCamera& GetCamera() {
            return camera_;
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