#pragma once

#include <string>
#include <base.h>
#include <optional>

#include <jpegturbo/turbojpeg.h>
#include <mlog.h>
#include <utils.h>

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
    Image() : ptr(nullptr), width(0), height(0), stride(0) {}
};

class ImageLoader {
public:
    template<typename ImageFormat>
    requires is_image_format<ImageFormat>
    static Image<ImageFormat> CreateBlankImage(size_t width, size_t height) {
        using data_type_t = ImageFormat::data_type_t;
        Image<ImageFormat> img {};
        img.ptr = new data_type_t[width * height];
        memset(img.ptr, 0, width * height * ImageFormat::Stride);
        img.width = width;
        img.height = height;
        img.stride = ImageFormat::Stride;
        return img;
    }

    template<typename ImageFormat>
    requires is_image_format<ImageFormat>
    static Image<ImageFormat> CreateImageFromPixels(size_t width, size_t height, size_t src_pitch, void* ptr) {
        using data_type_t = ImageFormat::data_type_t;
        Image<ImageFormat> img;
        img.ptr = new data_type_t[width * height];
        for (size_t i = 0; i < height; ++i) {
            memcpy(&img.ptr[i * width], &static_cast<char*>(ptr)[i * src_pitch], width);
        }
        img.width = width;
        img.height = height;
        img.stride = ImageFormat::Stride;
        return img;
    }

    template<typename ImageFormat>
    requires is_image_format<ImageFormat>
    static Image<ImageFormat> CreateImageFromPixels(size_t width, size_t height, void* ptr) {
        return CreateImageFromPixels<ImageFormat>(width, height, width, ptr);
    }

    template<typename ImageFormat>
    requires is_image_format<ImageFormat>
    static std::optional<Image<ImageFormat>> LoadJPG(const std::string& name) {
        using data_type_t = ImageFormat::data_type_t;
        FILE* jpg_file = fopen(name.c_str(), "rb");
        if (jpg_file == nullptr) {
            LFATAL("Cannot open image file {} while load image", name);
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
            LFATAL("Cannot initialize TurboJPEG compress library when load image {}", name);
            delete[] jpeg_buf;
            return std::nullopt;
        }
        int width, height, subsamp, colorspace;
        if (tjDecompressHeader3(handle, jpeg_buf, fsize, &width, &height, &subsamp, &colorspace) != 0) {
            LFATAL("Cannot decompress header in image file {} using TurboJPEG", name);
            tjDestroy(handle);
            delete[] jpeg_buf;
            return std::nullopt;
        }
        Image<ImageFormat> img {};
        img.width = width;
        img.height = height;
        img.stride = ImageFormat::Stride;
        img.ptr = new data_type_t[img.width * img.height * img.stride];
        if (tjDecompress2(handle, jpeg_buf, fsize, img.ptr, width, 0, height, ImageFormat::TJFormat, TJFLAG_FASTDCT) != 0) {
            LFATAL("Cannot decompress image file {} using TurboJPEG", name);
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
    static bool StoreJPG(const std::string& name, const Image<ImageFormat>& img, int quality = 100) {
        tjhandle handle = tjInitCompress();
        if (handle == nullptr) {
            LFATAL("Cannot initialize TurboJPEG compress library when store image {}", name);
            return false;
        }
        unsigned char *jpeg_buf = nullptr;
        unsigned long jpeg_size = 0;
        if (tjCompress2(handle, img.ptr, img.width, 0, img.height, ImageFormat::TJFormat, &jpeg_buf, &jpeg_size, ImageFormat::TJSampling, quality, TJFLAG_FASTDCT) != 0) {
            LFATAL("Cannot compress image file {} using TurboJPEG", name);
            tjDestroy(handle);
            return false;
        }
        FILE *outfile = fopen(name.c_str(), "wb");
        if (outfile == nullptr) {
            LFATAL("Cannot open image file {} while store image", name);
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