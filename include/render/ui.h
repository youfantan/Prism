#pragma once

#include <base.h>

#include <io/font.h>
#include <render/framework.h>

struct ui_init_t {
    std::vector<std::string> load_fonts;
};

template<typename Allocator>
requires std::derived_from<Allocator, DXAllocator>
class UIFramework {
public:
    class TextDrawcall;
private:
    ui_init_t init_;
    DXFramework<Allocator>* dxfw_;
    TextDrawcall text_drawcall_;
public:
    UIFramework(const ui_init_t& init, DXFramework<Allocator>* dxfw) : init_(init), dxfw_(dxfw), text_drawcall_(dxfw, init.load_fonts) {

    }

    TextDrawcall& GetTextDrawcall() {
        return text_drawcall_;
    }
};

template<typename Allocator>
requires std::derived_from<Allocator, DXAllocator>
class UIFramework<Allocator>::TextDrawcall : Drawcall {
public:
    struct Vertex {
        struct {
            float x;
            float y;
            float z;
        } Triangle;
        struct {
            float x;
            float y;
        } RelativeUV;
        struct {
            uint32_t x;
            uint32_t y;
        } CharPosition;
        uint32_t CharIndex;
    };

    using Index = uint32_t;

    struct UIPresets {
        uint32_t tex_index;
        uint32_t sb_index;
        uint32_t width;
        uint32_t height;
        XMFLOAT4 text_color;
        float scale_factor;
    };

    struct CachedStringView {
        std::string hash;
        VertexBuffer* vb;
        IndexBuffer* ib;
        ConstantBuffer* cb;
        ResourceManager* res_mgr;

        CachedStringView() = default;
        CachedStringView(const CachedStringView&) = delete;
        CachedStringView(CachedStringView&& csv) noexcept : hash(std::move(csv.hash)), cb(csv.cb), vb(csv.vb), ib(csv.ib), res_mgr(csv.res_mgr) {
            csv.res_mgr = nullptr;
            csv.vb = nullptr;
            csv.ib = nullptr;
            csv.cb = nullptr;
        }

       ~CachedStringView() {
           if (res_mgr != nullptr) {
               res_mgr->GetMap().RemoveResource(vb->GetName());
               res_mgr->GetMap().RemoveResource(ib->GetName());
               res_mgr->GetMap().RemoveResource(cb->GetName());
           }
       }
    };

    constexpr static uint64_t GENERATE_FONT_SIZE = 128;
private:
    std::vector<FontTexLoader> fonts_;
    DXFramework<Allocator>* dxfw_;

    std::optional<std::pair<std::vector<Vertex>, std::vector<Index>>> GenerateTriangles(FontTexLoader& ld, const std::wstring& str, uint32_t draw_x, uint32_t draw_y, float scale_factor) {
        auto idx = ld.GetMappedString(str);
        if (!idx.has_value()) {
            LFATAL("Cannot generate triangles when drawing string. font: {}, string: {}", ld.GetFontName(), ConvertWstringToString(str));
            return std::nullopt;
        }
        std::vector<Vertex> vertices;
        std::vector<Index> indices;
        for (int i = 0; i < idx->size(); ++i) {
            Vertex a = {
                .Triangle = { -1.0f, 1.0f, 0.0f },
                .RelativeUV = { 0.0f, 0.0f },
                .CharPosition = { draw_x, draw_y } ,
                .CharIndex = idx.value()[i],
            }; // Top Left
            Vertex b = {
                .Triangle = { 1.0f, 1.0f, 0.0f },
                .RelativeUV = { 1.0f, 0.0f },
                .CharPosition = { draw_x, draw_y } ,
                .CharIndex = idx.value()[i],
            }; // Top Right
            Vertex c = {
                .Triangle = { 1.0f, -1.0f, 0.0f },
                .RelativeUV = { 1.0f, 1.0f },
                .CharPosition = { draw_x, draw_y } ,
                .CharIndex = idx.value()[i],
            }; // Bottom Right
            Vertex d = {
                .Triangle = { -1.0f, -1.0f, 0.0f },
                .RelativeUV = { 0.0f, 1.0f },
                .CharPosition = { draw_x, draw_y } ,
                .CharIndex = idx.value()[i],
            }; // Bottom Left
            Index index[6] = {
                0, 1, 2,
                0, 2, 3
            };
            indices.push_back(index[0] + vertices.size());
            indices.push_back(index[1] + vertices.size());
            indices.push_back(index[2] + vertices.size());
            indices.push_back(index[3] + vertices.size());
            indices.push_back(index[4] + vertices.size());
            indices.push_back(index[5] + vertices.size());
            vertices.push_back(a);
            vertices.push_back(b);
            vertices.push_back(c);
            vertices.push_back(d);
            draw_x += ld.GetCharacterMeta(idx.value()[i]).advance * scale_factor;
        }
        return std::make_pair(vertices, indices);
    }
public:
    TextDrawcall(DXFramework<Allocator>* dxfw, const std::vector<std::string>& load_fonts) : Drawcall(dxfw->GetDevice().GetComPtr(), dxfw->GetRenderQueue(), dxfw->GetBindlessHeap(), dxfw->GetResourceManager()), dxfw_(dxfw) {
        auto vs = dxfw->GetShaderLoader().CompileShader("text", ShaderType::VertexShader);
        auto ps = dxfw->GetShaderLoader().CompileShader("text", ShaderType::PixelShader);
        const D3D12_INPUT_ELEMENT_DESC iv_layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "RELPOS",   0, DXGI_FORMAT_R32G32_UINT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "CIDX",   0, DXGI_FORMAT_R32_UINT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };
        pso_desc_.InputLayout = { iv_layout, CountOf(iv_layout) };
        pso_desc_.VS = {vs.blob->GetBufferPointer(), vs.blob->GetBufferSize()};
        pso_desc_.PS = {ps.blob->GetBufferPointer(), ps.blob->GetBufferSize()};
        pso_desc_.SampleDesc.Count = GetSampleCount(dxfw_->GetInitializeParams().msaa_type);
        pso_desc_.SampleDesc.Quality = 0;
        pso_desc_.NumRenderTargets = 1;
        pso_desc_.RTVFormats[0] = dxfw_->GetInitializeParams().rt_format;
        pso_desc_.DSVFormat = dxfw_->GetInitializeParams().ds_format;
        pso_desc_.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        pso_desc_.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc_.SampleMask = UINT_MAX;
        pso_desc_.BlendState = AlphaBlendDesc;
        samplers_.Add(StaticSamplers::LINEAR_FILTER(0));
        Drawcall::BuildPipeline();
        // Load Fonts
        auto& prefix = dxfw_->GetInitializeParams().assets_dir;
        for (const auto& font_name : load_fonts) {
            if (FontExists(prefix, font_name)) {
                if (!FontTextureAndUVExists(prefix, font_name)) {
                    FontTexGenerator gen(prefix, font_name);
                    gen.GenerateFontTexAndUV(GENERATE_FONT_SIZE);
                }
                auto& ld = fonts_.emplace_back(prefix, font_name, dxfw_->GetResourceManager());
                auto texture_binding = dxfw_->GetBindlessHeap().BindTexture(ld.GetTextureName());
                auto uv_binding = dxfw_->GetBindlessHeap().BindStructuredBuffer(ld.GetUVName());
                if (!texture_binding.has_value()) {
                    LFATAL("Cannot bind texture into bindless heap while loading font {}", ld.GetFontName());
                }
                if (!uv_binding.has_value()) {
                    LFATAL("Cannot bind uv into bindless heap while loading font {}", ld.GetFontName());
                }
            }
        }
    }

    void operator()(RenderPass& rp, CachedStringView& string, const XMFLOAT4& color) {
        string.cb->GetMapping<UIPresets>()->text_color = color;
        Draw(rp, string.cb, string.vb, string.ib);
    }

    void operator()(RenderPass& rp, const std::string& font, const std::wstring& text, uint32_t draw_x, uint32_t draw_y, uint32_t size, const XMFLOAT4& color) {
        auto csv = CreateCachedStringView(font, text, draw_x, draw_y, size).value();
        operator()(rp, csv, color);
    }

    std::optional<CachedStringView> CreateCachedStringView(const std::string& font, const std::wstring& str, uint32_t draw_x, uint32_t draw_y, uint32_t size) {
        int64_t idx = -1;
        for (int i = 0; i < fonts_.size(); ++i) {
            if (fonts_[i].GetFontName() == font) idx = i;
        }
        CachedStringView csv {};
        csv.res_mgr = &dxfw_->GetResourceManager();
        csv.hash = CalcSHA256HexDigest(str);
        if (idx == -1) {
            LFATAL("Cannot found font {} while create cached string view {}", font, csv.hash);
        }
        auto& ld = fonts_[idx];
        float scale_factor = static_cast<float>(size) / ld.GetAtlasSize();
        auto result = GenerateTriangles(ld, str, draw_x, draw_y, scale_factor);
        if (!result.has_value()) return std::nullopt;
        auto vertices = result.value().first;
        auto indices = result.value().second;
        auto vertex_buffer = dxfw_->GetResourceManager().CreateVertexBuffer("str_" + csv.hash + "_vertex_buffer", vertices.data(), vertices.size());
        if (!vertex_buffer.has_value()) {
            LFATAL("Cannot create vertex buffer while create cached string view {}", csv.hash);
            return std::nullopt;
        }
        auto index_buffer = dxfw_->GetResourceManager().CreateIndexBuffer("str_" + csv.hash + "_index_buffer", indices.data(), indices.size());
        if (!index_buffer.has_value()) {
            LFATAL("Cannot create index buffer while create cached string view {}", csv.hash);
            return std::nullopt;
        }
        auto presets = dxfw_->GetResourceManager().CreateConstantBuffer<UIPresets>("str_" + csv.hash + "_presets");
        if (!presets.has_value()) {
            LFATAL("Cannot create constant buffer while create cached string view {}", csv.hash);
            return std::nullopt;
        }
        auto* presets_mapping = presets.value()->GetMapping<UIPresets>();
        presets_mapping->tex_index = dxfw_->GetBindlessHeap().QueryResourceIndex(ld.GetTextureName());
        presets_mapping->sb_index = dxfw_->GetBindlessHeap().QueryResourceIndex(ld.GetUVName());
        presets_mapping->scale_factor = scale_factor;
        presets_mapping->width = dxfw_->GetInitializeParams().width;
        presets_mapping->height = dxfw_->GetInitializeParams().height;
        csv.vb = vertex_buffer.value();
        csv.ib = index_buffer.value();
        csv.cb = presets.value();
        return csv;
    }

};
