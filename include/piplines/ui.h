#pragma once

#include <base.h>

#include <io/font.h>
#include <render/framework.h>
#include <render/mesh.h>

namespace Prism
{
    class UIFramework {
    public:
        struct CharacterVertex {
            struct {
                float x;
                float y;
                float z;
            } Triangle;
            struct {
                float x;
                float y;
            } RelativeUV;
        };

        using CharacterIndex = uint32_t;

        std::vector<CharacterVertex> vertices = {
            {
                .Triangle = { -1.0f, 1.0f, 0.0f },
                .RelativeUV = { 0.0f, 0.0f },
            },
            {
                .Triangle = { 1.0f, 1.0f, 0.0f },
                .RelativeUV = { 1.0f, 0.0f },
            },
            {
                .Triangle = { 1.0f, -1.0f, 0.0f },
                .RelativeUV = { 1.0f, 1.0f },
            },
            {
                .Triangle = { -1.0f, -1.0f, 0.0f },
                .RelativeUV = { 0.0f, 1.0f },
            }
        };

        std::vector<CharacterIndex> indices = {
            0, 1, 2,
            0, 2, 3
        };

        struct CharacterInfo {
            struct {
                uint32_t x;
                uint32_t y;
            } RelativePos;
            float ScaleFactor;
            uint32_t CharIdx;
            uint32_t TexIndex;
            uint32_t Color;
        };

        struct UIPresets {
            uint32_t width;
            uint32_t height;
        };

        constexpr static D3D12_INPUT_ELEMENT_DESC LAYOUT[7] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "RELPOS",   0, DXGI_FORMAT_R32G32_UINT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "SCALE",   0, DXGI_FORMAT_R32_FLOAT, 1, 8, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "CIDX",   0, DXGI_FORMAT_R32_UINT, 1, 12, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "TEXIDX",   0, DXGI_FORMAT_R32_UINT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "COLOR",   0, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 20, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        };

        constexpr static size_t LAYOUT_COUNT = sizeof(LAYOUT) / sizeof(D3D12_INPUT_ELEMENT_DESC);

        class TextPipeline : public Pipeline {
        private:
            PrismApp* app_;
        public:
            TextPipeline(PrismApp* app) : Pipeline(app->GetDevice().GetComPtr()), app_(app) {
                rs_
                .BindTextureHeap(app_->GetResourceManager().GetTextureHeap())
                .BindStructuredBuffer(0, 0)
                .BindConstantBuffer(0, 0)
                .BindStaticSampler(LINEAR_SAMPLER_DESC<0, 0>)
                .Build();
                pso_desc_.pRootSignature = rs_.GetD3D12Signature();
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
                auto [vs_ptr, vs_len] = app_->GetShaderLoader().LoadShader("text", ShaderType::VertexShader).value();
                auto [ps_ptr, ps_len] = app_->GetShaderLoader().LoadShader("text", ShaderType::PixelShader).value();
                pso_desc_.VS = {vs_ptr, vs_len};
                pso_desc_.PS = {ps_ptr, ps_len};
                pso_desc_.NumRenderTargets = 1;
                pso_desc_.RTVFormats[0] = app_->GetInitializeParams().rt_format;
                pso_desc_.DSVFormat = app_->GetInitializeParams().ds_format;
                pso_desc_.NodeMask = 0;
                pso_desc_.InputLayout = { LAYOUT, sizeof(LAYOUT) / sizeof(D3D12_INPUT_ELEMENT_DESC) };
                pso_desc_.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
                pso_desc_.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
                pso_desc_.SampleMask = UINT_MAX;
                app_->GetDevice().GetComPtr()->CreateGraphicsPipelineState(&pso_desc_, IID_PPV_ARGS(&pso_));
                pso_->SetName(L"Text Pipeline");
            }
        };
    private:
        struct StringResBinding {
            uint64_t len;
            ResourceHandle font_tex;
            ResourceHandle font_meta;
        };
        PrismApp* app_;
        Mesh<CharacterVertex, CharacterIndex> text_mesh_;
        ResourceHandle characters_;
        ResourceHandle ui_presets_;
        std::vector<std::vector<StringResBinding>> strings_;
        StructuredView<CharacterInfo> characters_view_;
        FontLoader loader_;
        constexpr static size_t MAX_CHARACTERS = 1024 * 1024;
        uint64_t sync_value_;
        TextPipeline* text_pipeline_;
    public:
        UIFramework(PrismApp* app) : app_(app), text_mesh_("UI_Text", vertices, indices, app->GetResourceManager()), loader_(app->GetInitializeParams().assets_dir, app->GetResourceManager()) {
            app_->GetPipelineManager().CreatePipeline<TextPipeline>("UITextPipeline", app);
            text_pipeline_ = app_->GetPipelineManager().GetPipeline<TextPipeline>("UITextPipeline");
            characters_ = app_->GetResourceManager().CreateLocalBuffer("UI_Text_CharactersInfo", sizeof(CharacterInfo), MAX_CHARACTERS, D3D12_RESOURCE_STATE_COMMON);
            characters_view_.Create(characters_);
            ui_presets_ = app_->GetResourceManager().CreateLocalBuffer<UIPresets>("UI_Presets", D3D12_RESOURCE_STATE_COMMON);
            StructuredView<UIPresets> viewer(ui_presets_);
            strings_.resize(app_->GetInitializeParams().buffer_count);
            for (int i = 0; i < ui_presets_->GetResourceMeta().Buffer.layers; ++i) {
                viewer.SelectLayer(i);
                viewer[0].width = app_->GetInitializeParams().width;
                viewer[0].height = app_->GetInitializeParams().height;
            }
            LDEBUG("UI Framework initialized");
        }

        UIFramework(const UIFramework&) = delete;
        UIFramework(UIFramework&&) = delete;

        void DrawString(const std::string& font_name, const std::string& str, uint32_t x, uint32_t y, uint32_t size, const XMFLOAT4& color) {
            characters_view_.SelectLayer(app_->GetRenderContext().GetCurrentIndex());
            auto font = loader_.GetFont(font_name).value();
            auto mapped = font->GetMappedString(str).value();
            auto tex_index = font->GetFontTex()->GetResourceMeta().Tex2D.bind_index;
            auto uint_color = RGBA32(color);
            for (auto& idx : mapped) {
                CharacterInfo info {
                    .RelativePos = { x, y },
                    .ScaleFactor = static_cast<float>(size) / static_cast<float>(font->GetAtlas()),
                    .CharIdx = idx,
                    .TexIndex = tex_index,
                    .Color = uint_color
                };
                characters_view_.Append(info);
                x += static_cast<uint32_t>(font->GetCharacterMeta(idx).advance) * info.ScaleFactor;
            }
            strings_[app_->GetRenderContext().GetCurrentIndex()].emplace_back(str.size(), font->GetFontTex(), font->GetFontMeta());
        }

        RecordDispatcher::RecordProcess CreateRenderProcess() {
            RecordDispatcher::RecordProcess proc = {
                [&, layer = app_->GetRenderContext().GetCurrentIndex(), strings = strings_[app_->GetRenderContext().GetCurrentIndex()]](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                    sync_value_ = nfv;
                    text_mesh_.RenderSync(nfv);
                    for (auto& str : strings) {
                        str.font_meta->GetCopyWaitable().GPUWait();
                        str.font_tex->GetCopyWaitable().GPUWait();
                        str.font_meta->GetRenderWaitable().GetFenceValue() = nfv;
                        str.font_tex->GetRenderWaitable().GetFenceValue() = nfv;
                    }
                    characters_->GetRenderWaitable().GetFenceValue() = nfv; // As for local buffer, only sync op is to avoid release when inuse.
                    list->SetPipelineState(text_pipeline_->GetPSO().Get());
                    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    ID3D12DescriptorHeap* heaps[1] = { app_->GetResourceManager().GetTextureHeap().GetD3D12Heap() };
                    list->SetDescriptorHeaps(1, heaps);
                    list->SetGraphicsRootSignature(text_pipeline_->GetSignature());
                    uint64_t instance_off = 0;
                    for (auto& string : strings) {
                        list->SetGraphicsRootDescriptorTable(0, app_->GetResourceManager().GetTextureHeap().GetD3D12Heap()->GetGPUDescriptorHandleForHeapStart());
                        list->SetGraphicsRootShaderResourceView(1, string.font_meta->GetD3D12Resource()->GetGPUVirtualAddress());
                        list->SetGraphicsRootConstantBufferView(2, ui_presets_->GetD3D12Resource(layer)->GetGPUVirtualAddress());
                        D3D12_VERTEX_BUFFER_VIEW views[2] {};
                        views[0] = text_mesh_.GetVertexBufferView();
                        views[1].BufferLocation = characters_->GetD3D12Resource(layer)->GetGPUVirtualAddress();
                        views[1].StrideInBytes = characters_->GetResourceMeta().Buffer.element_size;
                        views[1].SizeInBytes = characters_->GetResourceMeta().Buffer.element_count * characters_->GetResourceMeta().Buffer.element_size;
                        auto& ibv = text_mesh_.GetIndexBufferView();
                        list->IASetVertexBuffers(0, 2, views);
                        list->IASetIndexBuffer(&ibv);
                        list->DrawIndexedInstanced(text_mesh_.GetIndexBuffer()->GetResourceMeta().Buffer.element_count, string.len, 0, 0, instance_off);
                        instance_off += string.len;
                    }
                    return new uint64_t(layer);
                },
                [&](void* data) {
                    uint64_t* index = static_cast<uint64_t*>(data);
                    auto old = characters_view_.SelectLayer(*index);
                    characters_view_.ResetWritePos();
                    characters_view_.SelectLayer(old);
                    strings_[*index].clear();
                    delete index;
                }
            };
            return proc;
        }

        ~UIFramework() {
            app_->GetResourceManager().MarkAsExpired(characters_);
            app_->GetCopyDispatcher().ReleaseSync();
            app_->GetRenderDispatcher().ReleaseSync();
        }
    };
}
