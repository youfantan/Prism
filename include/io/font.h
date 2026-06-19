#include <ft2build.h>
#include FT_FREETYPE_H

#include <base.h>
#include <render/resource.h>
#include <io/image.h>
#include <utils.h>


struct character_meta_t {
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

class FontTexGenerator {
private:
    std::string font_name_;
    std::string font_path_;
    std::string map_path_;
    std::string generate_tex_path_;
    std::string generate_uv_path_;
    std::wstring ch_map_;
    FT_Library ft_lib_;
    FT_Face face_;
public:
    FontTexGenerator(const std::string& prefix, const std::string& font_name) : ft_lib_(nullptr), face_(nullptr), font_name_(font_name) {
        font_path_ = prefix + "/" + font_name + ".ttf";
        map_path_ = prefix + "/chmap.txt";
        generate_tex_path_ = std::format("{}/{}_tex.jpg", prefix, font_name);
        generate_uv_path_ = std::format("{}/{}_uv.txt", prefix, font_name);
        if (FT_Init_FreeType(&ft_lib_) != FT_Err_Ok) {
            LFATAL("Cannot initialize FreeType library while loading font {}", font_name_);
        }
        if (FT_New_Face(ft_lib_, font_path_.c_str(), 0, &face_) != FT_Err_Ok) {
            LFATAL("Cannot create font face while loading font {}", font_name_);
        }
        auto chs = ReadFileIntoString(map_path_);
        if (!chs.has_value()) {
            LFATAL("Cannot read character map file {} while loading font {}", map_path_, font_name_);
        }
        ch_map_ = ConvertStringToWstring(chs.value());
    }

    bool GenerateFontTexAndUV(size_t atlas_size) {
        LINFO("Start to generate texture and uv of the font {}", font_name_);
        size_t cell_size = atlas_size + 32;
        if (FT_Set_Pixel_Sizes(face_, 0, atlas_size) != FT_Err_Ok) {
            LFATAL("Cannot set pixel size to {} while loading font {}", font_name_);
            return false;
        }
        auto GetMinSquare = [](uint64_t N) {
            auto s = static_cast<size_t>(std::sqrt(N));
            while (s * s <= N) ++s;
            return s;
        };
        uint64_t cell_cols = GetMinSquare(ch_map_.size());
        uint64_t cell_rows = GetMinSquare(ch_map_.size());
        auto bitmap = ImageLoader::CreateBlankImage<ImageFormatGray>(cell_cols * cell_size, cell_rows * cell_size);
        std::stringstream atlas_output;
        atlas_output << ch_map_.size() << std::endl;
        for (uint64_t i = 0; i < ch_map_.size(); ++i) {
            uint64_t cell_x = i % cell_cols;
            uint64_t cell_y = i / cell_rows;
            if (FT_Load_Char(face_, ch_map_[i], FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING) != FT_Err_Ok) {
                LFATAL("Cannot load character {} while loading font {}", static_cast<uint32_t>(ch_map_[i]), font_name_);
                return false;
            }
            if (FT_Render_Glyph(face_->glyph, FT_RENDER_MODE_NORMAL) != FT_Err_Ok) {
                LFATAL("Cannot render character {}", static_cast<uint32_t>(ch_map_[i]));
                return false;
            }
            LINFO("Generating character {}", static_cast<uint32_t>(ch_map_[i]));
            FT_Bitmap& glyph = face_->glyph->bitmap;
            auto glyph_bitmap = ImageLoader::CreateImageFromPixels<ImageFormatGray>(glyph.width, glyph.rows, glyph.pitch, glyph.buffer);
            uint64_t start_x = cell_x * cell_size + (cell_size - glyph.width) / 2;
            uint64_t start_y = cell_y * cell_size + (cell_size - glyph.rows) / 2;
            bitmap.CopyRegion(glyph_bitmap, start_x, start_y);
            character_meta_t meta {};
            meta.ch = ch_map_[i];
            meta.width = glyph.width;
            meta.height = glyph.rows;
            meta.tex_u0 = static_cast<float>(start_x) / bitmap.width;
            meta.tex_u1 = static_cast<float>(start_x + glyph.width) / bitmap.width;
            meta.tex_v0 = static_cast<float>(start_y) / bitmap.height;
            meta.tex_v1 = static_cast<float>(start_y + glyph.rows) / bitmap.height;
            meta.bearing_x = (float)face_->glyph->bitmap_left;
            meta.bearing_y = (float)face_->glyph->bitmap_top;
            meta.advance  = (float)(face_->glyph->advance.x >> 6);
            atlas_output << std::format("{} {} {} {} {} {} {} {} {} {}", static_cast<uint32_t>(meta.ch), meta.width, meta.height, meta.tex_u0, meta.tex_v0, meta.tex_u1, meta.tex_v1, meta.bearing_x, meta.bearing_y, meta.advance) << std::endl;
        }
        ImageLoader::StoreJPG(generate_tex_path_, bitmap);
        WriteStringToFile(generate_uv_path_, atlas_output.str());
        return true;
    }
};

class FontTexLoader {
private:
    std::string prefix_;
    std::string font_name_;
    std::string generate_tex_path_;
    std::string generate_uv_path_;
    ResourceManager& res_mgr_;
    Texture* tex_;
    std::vector<character_meta_t> uvs_;
    std::unordered_map<wchar_t, uint32_t> map_;

    void LoadTexAndUV() {
        auto tex_img = ImageLoader::LoadJPG<ImageFormatGray>(generate_tex_path_);
        if (!tex_img.has_value()) {
            LFATAL("Cannot load texture of font {}", font_name_);
        }
        auto tex = res_mgr_.CreateTextureFromImage(std::format("font_{}_tex", font_name_), tex_img.value());
        if (!tex.has_value()) {
            LFATAL("Cannot create texture of font {}", font_name_);
        }
        tex_ = tex.value();
        std::stringstream uv_str(ReadFileIntoString(generate_uv_path_).value());
        size_t size;
        uv_str >> size;
        for (size_t i = 0; i < size; ++i) {
            character_meta_t m {};
            uv_str >> m.ch;
            uv_str >> m.width;
            uv_str >> m.height;
            uv_str >> m.tex_u0;
            uv_str >> m.tex_u1;
            uv_str >> m.tex_v0;
            uv_str >> m.tex_v1;
            uv_str >> m.bearing_x;
            uv_str >> m.bearing_y;
            uv_str >> m.advance;
            uvs_.push_back(m);
            map_[m.ch] = uvs_.size() - 1;
        }
        auto uv = res_mgr_.CreateStructuredBuffer(std::format("font_{}_uv", font_name_), uvs_);
        if (!uv.has_value()) {
            LFATAL("Cannot create uv array of font {}", font_name_);
        }
    }
public:
    FontTexLoader(const std::string& prefix, const std::string& font_name, ResourceManager& res_mgr) : prefix_(prefix), font_name_(font_name), res_mgr_(res_mgr), tex_(nullptr) {
        generate_tex_path_ = std::format("{}/{}_tex.jpg", prefix, font_name);
        generate_uv_path_ = std::format("{}/{}_uv.txt", prefix, font_name);
        if (!FileExists(generate_tex_path_)) {
            LFATAL("Cannot found font texture file {} while loading font {}", generate_tex_path_, font_name_);
        }
        if (!FileExists(generate_uv_path_)) {
            LFATAL("Cannot found font uv file {} while loading font {}", generate_uv_path_, font_name_);
        }
        LoadTexAndUV();
    }

    std::optional<std::vector<uint32_t>> GetMappedString(const std::wstring& string) {
        std::vector<uint32_t> mapped(string.size());
        for (int i = 0; i < string.size(); ++i) {
            if (map_.contains(string[i])) {
                mapped[i] = map_[string[i]];
            } else {
                return std::nullopt;
            }
        }
        return mapped;
    }

    character_meta_t& GetCharacterMeta(uint32_t index) {
        return uvs_[index];
    }

    const std::string& GetFontName() const {
        return font_name_;
    }
};

inline bool FontTextureAndUVExists(const std::string& prefix, const std::string& font_name) {
    std::string generate_tex_path_ = std::format("{}/{}_tex.jpg", prefix, font_name);
    std::string generate_uv_path_ = std::format("{}/{}_uv.txt", prefix, font_name);
    return FileExists(generate_uv_path_) && FileExists(generate_tex_path_);
}