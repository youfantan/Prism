#pragma once

#include <string>
#include <base.h>
#include <optional>

#include <jpegturbo/turbojpeg.h>
#include <mlog.h>
#include <utils.h>

namespace Prism
{
    class ImageLoader;

template<typename T>
concept is_image_format = requires {
    typename T::data_type_t;
    { T::DXGIFormat } -> decay_as<DXGI_FORMAT>;
    { T::TJFormat } -> decay_as<TJPF>;
    { T::TJSampling } -> decay_as<TJSAMP>;
    { T::Stride } -> decay_as<size_t>;
};

struct ImageFormatRGBA {
    using data_type_t = struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    };

    static constexpr DXGI_FORMAT DXGIFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    static constexpr TJPF TJFormat = TJPF_RGBA;
    static constexpr TJSAMP TJSampling = TJSAMP_422;
    static constexpr size_t Stride = sizeof(data_type_t);
};

struct ImageFormatGray {
    using data_type_t = uint8_t;

    static constexpr DXGI_FORMAT DXGIFormat = DXGI_FORMAT_R8_UNORM;
    static constexpr TJPF TJFormat = TJPF_GRAY;
    static constexpr TJSAMP TJSampling = TJSAMP_GRAY;
    static constexpr size_t Stride = sizeof(data_type_t);
};

template<typename ImageFormat>
requires is_image_format<ImageFormat>
struct Image {
    friend ImageLoader;
    uint8_t* ptr;
    size_t stride;
    size_t width;
    size_t height;

    Image(const Image&) = delete;
    Image(Image&& img) noexcept : ptr(img.ptr), stride(img.stride), width(img.width), height(img.height) {
        img.ptr = nullptr;
        img.stride = 0;
        img.width = 0;
        img.height = 0;
    }

    ImageFormat::data_type_t& At(size_t x, size_t y) {
        return reinterpret_cast<ImageFormat::data_type_t&>(ptr[(y * width + x) * stride]);
    }

    const ImageFormat::data_type_t& At(size_t x, size_t y) const {
        return reinterpret_cast<ImageFormat::data_type_t&>(ptr[(y * width + x) * stride]);
    }

    bool CopyRegion(const Image& img, size_t start_x, size_t start_y) {
        if (start_x + img.width > width || start_y + img.height > height) return false;
        for (size_t i = 0; i < img.height; ++i) {
            size_t row = i + start_y;
            memcpy(ptr + row * width + start_x, img.ptr + i * img.width, stride * img.width);
        }
        return true;
    }

    ~Image() {
        if (ptr != nullptr) {
            delete[] ptr;
            width = 0;
            height = 0;
            stride = 0;
        }
    }

private:
    Image(uint8_t* ptr, size_t width, size_t height) : ptr(ptr), width(width), height(height), stride(ImageFormat::Stride) {

    }
};

class ImageLoader {
public:
    template<typename ImageFormat>
    requires is_image_format<ImageFormat>
    static Image<ImageFormat> CreateBlankImage(size_t width, size_t height) {
        using data_type_t = ImageFormat::data_type_t;
        auto ptr = new data_type_t[width * height];
        memset(ptr, 0, width * height * ImageFormat::Stride);
        Image<ImageFormat> img(ptr, width, height);
        return img;
    }

    template<typename ImageFormat>
    requires is_image_format<ImageFormat>
    static Image<ImageFormat> CreateImageFromPixels(size_t width, size_t height, size_t src_pitch, void* ptr) {
        using data_type_t = ImageFormat::data_type_t;
        auto data = new data_type_t[width * height];
        for (size_t i = 0; i < height; ++i) {
            memcpy(&data[i * width], &static_cast<char*>(ptr)[i * src_pitch], width);
        }
        Image<ImageFormat> img(data, width, height);
        return img;
    }

    template<typename ImageFormat>
    requires is_image_format<ImageFormat>
    static Image<ImageFormat> CreateImageFromPixels(size_t width, size_t height, void* ptr) {
        return CreateImageFromPixels<ImageFormat>(width, height, width, ptr);
    }

    template<typename ImageFormat>
    requires is_image_format<ImageFormat>
    static std::optional<Image<ImageFormat>> LoadJPG(const std::string& path) {
        using data_type_t = ImageFormat::data_type_t;
        FILE* jpg_file = fopen(path.c_str(), "rb");
        if (jpg_file == nullptr) {
            LFATAL("Cannot open image file {} while load image", path);
            return std::nullopt;
        }
        fseek(jpg_file, 0, SEEK_END);
        long fsize = ftell(jpg_file);
        rewind(jpg_file);
        auto* jpeg_buf = new uint8_t[fsize];
        fread(jpeg_buf, 1, fsize, jpg_file);
        fclose(jpg_file);
        tjhandle handle = tjInitDecompress();
        if (handle == nullptr) {
            LFATAL("Cannot initialize TurboJPEG compress library when load image {}", path);
            delete[] jpeg_buf;
            return std::nullopt;
        }
        int width, height, subsamp, colorspace;
        if (tjDecompressHeader3(handle, jpeg_buf, fsize, &width, &height, &subsamp, &colorspace) != 0) {
            LFATAL("Cannot decompress header in image file {} using TurboJPEG", path);
            tjDestroy(handle);
            delete[] jpeg_buf;
            return std::nullopt;
        }
        auto ptr = new data_type_t[width * height * ImageFormat::Stride];
        Image<ImageFormat> img(reinterpret_cast<uint8_t*>(ptr),width, height);
        if (tjDecompress2(handle, jpeg_buf, fsize, img.ptr, width, 0, height, ImageFormat::TJFormat, TJFLAG_FASTDCT) != 0) {
            LFATAL("Cannot decompress image file {} using TurboJPEG", path);
            tjDestroy(handle);
            delete[] jpeg_buf;
            delete[] img.ptr;
            return std::nullopt;
        }
        tjDestroy(handle);
        delete[] jpeg_buf;
        return img;
    }

    template<typename ImageFormat>
    requires is_image_format<ImageFormat>
    static bool StoreJPG(const std::string& path, const Image<ImageFormat>& img, int quality = 100) {
        tjhandle handle = tjInitCompress();
        if (handle == nullptr) {
            LFATAL("Cannot initialize TurboJPEG compress library when store image {}", path);
            return false;
        }
        unsigned char *jpeg_buf = nullptr;
        unsigned long jpeg_size = 0;
        if (tjCompress2(handle, img.ptr, img.width, 0, img.height, ImageFormat::TJFormat, &jpeg_buf, &jpeg_size, ImageFormat::TJSampling, quality, TJFLAG_FASTDCT) != 0) {
            LFATAL("Cannot compress image file {} using TurboJPEG", path);
            tjDestroy(handle);
            return false;
        }
        FILE *outfile = fopen(path.c_str(), "wb");
        if (outfile == nullptr) {
            LFATAL("Cannot open image file {} while store image", path);
            tjFree(jpeg_buf);
            tjDestroy(handle);
            return false;
        }
        fwrite(jpeg_buf, 1, jpeg_size, outfile);
        fclose(outfile);
        tjFree(jpeg_buf);
        tjDestroy(handle);
        return true;
    }


};
}