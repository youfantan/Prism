#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H
#include <json.hpp>
using json = nlohmann::json;

#include <base.h>
#include <render/resource.h>
#include <io/image.h>
#include <utils.h>

// http://www.fifi.org/doc/libttf2/docs/glyphs.htm

namespace Prism
{
    struct CharacterMeta {
    uint32_t ch;
    float tex_u0;
    float tex_v0;
    float tex_u1;
    float tex_v1;
    float width;
    float height;
    float bearing_x;
    float bearing_y;
    float advance;
};

class FontLoader;

class Font {
    friend class FontLoader;
private:
    ResourceManager& res_mgr_;
    std::unordered_map<char32_t, uint32_t> map_;
    std::vector<CharacterMeta> metas_;
    ResourceHandle meta_res;
    ResourceHandle tex_res;
    size_t atlas_;

    Font(const std::string& name, const std::string& font_tex_path, const std::string& font_uv_path, ResourceManager& res_mgr) : res_mgr_(res_mgr) {
        LDEBUG("Loading font {}({}, {})", name, font_tex_path, font_uv_path);
        auto tex_img = ImageLoader::LoadJPG<ImageFormatGray>(font_tex_path);
        if (!tex_img.has_value()) {
            LFATAL("Cannot load texture of font tex {}", font_tex_path);
        }
        tex_res = res_mgr.CreateTexture2DFromImage("Texture_Font_" + name, tex_img.value());
        std::stringstream uv_str(ReadFileIntoString(font_uv_path).value());
        size_t size;
        size_t atlas;
        uv_str >> size;
        uv_str >> atlas;
        atlas_ = atlas;
        for (size_t i = 0; i < size; ++i) {
            CharacterMeta m {};
            uv_str >> m.ch;
            uv_str >> m.width;
            uv_str >> m.height;
            uv_str >> m.tex_u0;
            uv_str >> m.tex_v0;
            uv_str >> m.tex_u1;
            uv_str >> m.tex_v1;
            uv_str >> m.bearing_x;
            uv_str >> m.bearing_y;
            uv_str >> m.advance;
            metas_.push_back(m);
            map_[m.ch] = metas_.size() - 1;
        }
        meta_res = res_mgr.CreateRemoteBuffer("Buffer_Font_" + name, metas_, D3D12_RESOURCE_STATE_COMMON);
    }
public:
    Font(const Font&) = delete;
    Font(Font&& f) noexcept : res_mgr_(f.res_mgr_), map_(std::move(f.map_)), metas_(std::move(f.metas_)), atlas_(f.atlas_), meta_res(f.meta_res), tex_res(f.tex_res) {
        f.meta_res = nullptr;
        f.tex_res = nullptr;
    }

    CharacterMeta& GetCharacterMeta(char32_t ch) {
        return metas_[map_[ch]];
    }

    std::optional<std::vector<uint32_t>> GetMappedString(const std::string& str) {
        std::u32string ustr = ConvertStringToU32String(str);
        std::vector<uint32_t> result;
        for (char32_t& ch : ustr) {
            if (!map_.contains(ch)) {
                return std::nullopt;
            }
            result.push_back(map_[ch]);
        }
        return result;
    }

    ResourceHandle GetFontMeta() {
        return meta_res;
    }

    ResourceHandle GetFontTex() {
        return tex_res;
    }

    size_t GetAtlas() {
        return atlas_;
    }

    ~Font() {
        res_mgr_.MarkAsExpired(meta_res);
        res_mgr_.MarkAsExpired(tex_res);
    }
};

class FontLoader {
    std::string prefix_;
    std::unordered_map<std::string, Font> font_map_;

    void GenFontTexAndUV(const std::string& font_path, const std::string& chmap_path, const std::string& uv_path, const std::string& tex_path, uint32_t atlas_size) {
        FT_Library ft_lib_;
        FT_Face face_;
        if (FT_Init_FreeType(&ft_lib_) != FT_Err_Ok) {
            LFATAL("Cannot initialize FreeType library while loading font {}", font_path);
        }
        if (FT_New_Face(ft_lib_, font_path.c_str(), 0, &face_) != FT_Err_Ok) {
            LFATAL("Cannot create font face while loading font {}", font_path);
        }
        LINFO("Start to generate texture and uv of the font {}", font_path);
        uint32_t cell_size = atlas_size + 32;
        if (FT_Set_Pixel_Sizes(face_, 0, atlas_size) != FT_Err_Ok) {
            LFATAL("Cannot set pixel size to {} while loading font {}", font_path);
        }
        auto GetMinSquare = [](uint64_t N) {
            auto s = static_cast<size_t>(std::sqrt(N));
            while (s * s <= N) ++s;
            return s;
        };
        std::string chmap_str = ReadFileIntoString(chmap_path).value();
        std::u32string chmap = ConvertStringToU32String(chmap_str);
        uint64_t cell_cols = GetMinSquare(chmap.size());
        uint64_t cell_rows = GetMinSquare(chmap.size());
        auto bitmap = ImageLoader::CreateBlankImage<ImageFormatGray>(cell_cols * cell_size, cell_rows * cell_size);
        std::stringstream atlas_output;
        atlas_output << chmap.size() << " " << atlas_size << std::endl;
        for (uint64_t i = 0; i < chmap.size(); ++i) {
            uint64_t cell_x = i % cell_cols;
            uint64_t cell_y = i / cell_rows;
            if (FT_Load_Char(face_, chmap[i], FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING) != FT_Err_Ok) {
                LFATAL("Cannot load character {} while loading font {}", static_cast<uint32_t>(chmap[i]), font_path);
            }
            if (FT_Render_Glyph(face_->glyph, FT_RENDER_MODE_NORMAL) != FT_Err_Ok) {
                LFATAL("Cannot render character {}", static_cast<uint32_t>(chmap[i]));
            }
            LINFO("Generating character {}", static_cast<uint32_t>(chmap[i]));
            FT_Bitmap& glyph = face_->glyph->bitmap;
            auto glyph_bitmap = ImageLoader::CreateImageFromPixels<ImageFormatGray>(glyph.width, glyph.rows, glyph.pitch, glyph.buffer);
            uint64_t start_x = cell_x * cell_size + (cell_size - glyph.width) / 2;
            uint64_t start_y = cell_y * cell_size + (cell_size - glyph.rows) / 2;
            bitmap.CopyRegion(glyph_bitmap, start_x, start_y);
            CharacterMeta meta {};
            meta.ch = chmap[i];
            meta.width = glyph.width;
            meta.height = glyph.rows;
            meta.tex_u0 = static_cast<float>(start_x) / bitmap.width;
            meta.tex_v0 = static_cast<float>(start_y) / bitmap.height;
            meta.tex_u1 = static_cast<float>(start_x + glyph.width) / bitmap.width;
            meta.tex_v1 = static_cast<float>(start_y + glyph.rows) / bitmap.height;
            meta.bearing_x = (float)face_->glyph->bitmap_left;
            meta.bearing_y = (float)face_->glyph->bitmap_top;
            meta.advance  = (float)(face_->glyph->advance.x >> 6);
            atlas_output << std::format("{} {} {} {} {} {} {} {} {} {}", static_cast<uint32_t>(meta.ch), meta.width, meta.height, meta.tex_u0, meta.tex_v0, meta.tex_u1, meta.tex_v1, meta.bearing_x, meta.bearing_y, meta.advance) << std::endl;
        }
        ImageLoader::StoreJPG(tex_path, bitmap);
        WriteStringToFile(uv_path, atlas_output.str());
    }

public:
    FontLoader(const std::string& prefix, ResourceManager& rm) : prefix_(prefix) {
        std::string json_path = prefix_ + "/fonts.json";
        std::ifstream ffonts(json_path, std::ios::in | std::ios::binary);
        if (!ffonts.good()) {
            LFATAL("Cannot found {}", prefix_ + "/fonts.json");
            exit(EXIT_FAILURE);
        }
        json fonts_json = json::parse(ffonts);
        auto& fonts = fonts_json["Fonts"];
        for (auto& font : fonts) {
            std::string name = font["name"];
            std::string path = prefix_ + "/" + static_cast<std::string>(font["path"]);
            std::string character_map = prefix_ + "/" + static_cast<std::string>(font["character_map"]);
            if (!font.contains("gen_tex_path") || !font.contains("gen_meta_path")) {
                std::string gen_tex_path = std::format("{}/{}.tex.jpg", prefix_, name);
                std::string gen_meta_path = std::format("{}/{}.meta.txt", prefix_, name);
                GenFontTexAndUV(path, character_map, gen_meta_path, gen_tex_path, 128);
                font["gen_tex_path"] = gen_tex_path;
                font["gen_meta_path"] = gen_meta_path;
            }
            std::string gen_tex_path = font["gen_tex_path"];
            std::string gen_meta_path = font["gen_meta_path"];
            Font f(name, gen_tex_path, gen_meta_path, rm);
            font_map_.try_emplace(name, std::move(f));
        }
        std::ofstream ofonts(json_path, std::ios::out | std::ios::binary);
        ofonts << fonts_json.dump();
    }

    std::optional<Font*> GetFont(const std::string& name) {
        if (!font_map_.contains(name)) {
            LFATAL("Cannot found font {}", name);
            return std::nullopt;
        }
        return &font_map_.at(name);
    }
};

inline bool FontExists(const std::string& prefix, const std::string& font_name) {
    std::string font_path = std::format("{}/{}.ttf", prefix, font_name);
    return FileExists(font_path);
}

inline bool FontTextureAndUVExists(const std::string& prefix, const std::string& font_name) {
    std::string generate_tex_path_ = std::format("{}/{}_tex.jpg", prefix, font_name);
    std::string generate_uv_path_ = std::format("{}/{}_uv.txt", prefix, font_name);
    return FileExists(generate_uv_path_) && FileExists(generate_tex_path_);
}
}