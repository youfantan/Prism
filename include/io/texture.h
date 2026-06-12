#pragma once

#include <base.h>
#include <optional>
#include <string>
#include <jpegturbo/turbojpeg.h>

class TextureLoader {
public:
    using texture_in_memory_t = struct {
        uint8_t* data;
        size_t size;
        size_t width;
        size_t height;
        size_t row_pitch;
        DXGI_FORMAT format;
    };

private:
    const std::string prefix_;
public:
    TextureLoader(const std::string& prefix);
    std::optional<texture_in_memory_t> LoadTextureIntoMemory(const std::string& tex_name);
    void FreeTextureInMemory(texture_in_memory_t& tex);
    
};