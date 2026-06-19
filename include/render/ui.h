#pragma once

#include <base.h>

#include <io/font.h>
#include <render/framework.h>

template<typename Allocator>
requires std::derived_from<Allocator, DXAllocator>
class UIFramework {
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
            float x;
            float y;
        } CharPosition;
        uint32_t CharIndex;
    };

    using Index = uint32_t;

    struct CachedStringView {
        std::string hash;
        VertexBuffer* vb;
        IndexBuffer* ib;
    };
private:
    std::vector<FontTexLoader> fonts_;
    DXFramework<Allocator>* dxfw_;
    Lazy<Drawcall> text_drawcall_;

    std::optional<std::pair<std::vector<Vertex>, std::vector<Index>>> GenerateTriangles(FontTexLoader& ld, const std::wstring& str, uint32_t draw_x, uint32_t draw_y) {
        auto idx = ld.GetMappedString(str);
        if (!idx.has_value()) {
            LFATAL("Cannot generate triangles when drawing string. font: {}, string: {}", ld.GetFontName(), ConvertWstringToString(str));
            return std::nullopt;
        }
        float ndc_draw_x = static_cast<float>(draw_x) / dxfw_->GetInitializeParams().width;
        float ndc_draw_y = static_cast<float>(draw_y) / dxfw_->GetInitializeParams().height;
        std::vector<Vertex> vertices;
        std::vector<Index> indices;
        for (int i = 0; i < idx->size(); ++i) {
            Vertex a = {
                .Triangle = { -0.5f, 0.5f, 0.0f },
                .RelativeUV = { 0.0f, 0.0f },
                .CharPosition = { ndc_draw_x, ndc_draw_y } ,
                .CharIndex = idx.value()[i],
            }; // Top Left
            Vertex b = {
                .Triangle = { 0.5f, 0.5f, 0.0f },
                .RelativeUV = { 1.0f, 0.0f },
                .CharPosition = { ndc_draw_x, ndc_draw_y } ,
                .CharIndex = idx.value()[i],
            }; // Top Right
            Vertex c = {
                .Triangle = { 0.5f, -0.5f, 0.0f },
                .RelativeUV = { 1.0f, 1.0f },
                .CharPosition = { ndc_draw_x, ndc_draw_y } ,
                .CharIndex = idx.value()[i],
            }; // Bottom Right
            Vertex d = {
                .Triangle = { -0.5f, -0.5f, 0.0f },
                .RelativeUV = { 1.0f, 0.0f },
                .CharPosition = { ndc_draw_x, ndc_draw_y } ,
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
            ndc_draw_x += ld.GetCharacterMeta(i).advance / dxfw_->GetInitializeParams().width;
        }
    }
public:
    UIFramework(DXFramework<Allocator>* dxfw) : dxfw_(dxfw) {

    }

    std::optional<CachedStringView> CreateCachedStringView(const std::string& font, const std::wstring& str, uint32_t draw_x, uint32_t draw_y) {
        int64_t idx = -1;
        for (int i = 0; i < fonts_.size(); ++i) {
            if (fonts_[i].GetFontName() == font) idx = i;
        }
        CachedStringView csv {};
        csv.hash = CalcSHA256HexDigest(str);
        if (idx == -1) {
            LFATAL("Cannot found font {} while create cached string view {}", font, csv.hash);
        }
        auto& ld = fonts_[idx];
        auto result = GenerateTriangles(ld, str, draw_x, draw_y);
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
        csv.vb = vertex_buffer.value();
        csv.ib = index_buffer.value();
        return csv;
    }

};
