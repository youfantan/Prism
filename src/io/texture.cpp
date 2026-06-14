#include <io/texture.h>
#include <mlog.h>

#include <jpegturbo/turbojpeg.h>

TextureLoader::TextureLoader(std::string_view prefix) : prefix_(prefix) {}

std::optional<TextureLoader::texture_in_memory_t> TextureLoader::LoadTextureIntoMemory(const std::string& tex_name)
{
    std::string tex_file = prefix_ + "/" + tex_name + ".jpg";
    FILE* jpg_file = fopen(tex_file.c_str(), "rb");
    if (jpg_file == nullptr) return std::nullopt;
    fseek(jpg_file, 0, SEEK_END);
    long fsize = ftell(jpg_file);
    rewind(jpg_file);
    auto* jpeg_buf = new uint8_t[fsize];
    fread(jpeg_buf, 1, fsize, jpg_file);
    fclose(jpg_file);
    tjhandle handle = tjInitDecompress();
    if (handle == nullptr) {
        LFATAL("Cannot initialize TurboJPEG compress library when load texture {}", tex_name);
        delete[] jpeg_buf;
        return std::nullopt;
    }
    int width, height, subsamp, colorspace;
    if (tjDecompressHeader3(handle, jpeg_buf, fsize, &width, &height, &subsamp, &colorspace) != 0) {
        LFATAL("Cannot decompress header in texture file {} using TurboJPEG", tex_name);
        tjDestroy(handle);
        delete[] jpeg_buf;
        return std::nullopt;
    }
    texture_in_memory_t tex {};
    tex.width = width;
    tex.height = height;
    tex.row_pitch = width * 4;
    tex.size = tex.row_pitch * height;
    tex.data = new uint8_t[tex.size];
    tex.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    if (tjDecompress2(handle, jpeg_buf, fsize, tex.data, width, 0, height, TJPF_RGBA, TJFLAG_FASTDCT) != 0) {
        LFATAL("Cannot decompress texture file {} using TurboJPEG", tex_name);
        tjDestroy(handle);
        delete[] jpeg_buf;
        delete[] tex.data;
        return std::nullopt;
    }
    tjDestroy(handle);
    delete[] jpeg_buf;
    return tex;
}

void TextureLoader::FreeTextureInMemory(texture_in_memory_t& tex)
{
    delete[] tex.data;
    tex.data = nullptr;
    tex.width = 0;
    tex.height = 0;
    tex.size = 0;
}
