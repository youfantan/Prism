#pragma once

#include <string>
#include <base.h>
#include <optional>
#include <stb_image.h>
#include <jpegturbo/turbojpeg.h>
#include <mlog.h>
#include <utils.h>

namespace Prism
{
    struct ImageFormat {
        DXGI_FORMAT DXFormat;
    };

    union PixelAccess {
        struct {
            uint8_t r;
            uint8_t g;
            uint8_t b;
        } RGB24;
        struct {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t a;
        } RGBA32;
        struct {
            uint8_t r;
        } Gray8;
    };


    struct Image {
        uint8_t* ptr;
        size_t stride;
        size_t width;
        size_t height;
        DXGI_FORMAT dx_format;
        bool managed_;

        Image(size_t width, size_t height, size_t stride, DXGI_FORMAT dx_format) : width(width), height(height), stride(stride), dx_format(dx_format), managed_(false) {
            ptr = new uint8_t[width * height * stride];
            memset(ptr, 0, width * height * stride);
        }

        Image(size_t width, size_t height, size_t stride, uint8_t* data, DXGI_FORMAT dx_format) : width(width), height(height), stride(stride), ptr(data), dx_format(dx_format), managed_(true) {

        }

        Image(const Image&) = delete;
        Image(Image&& img) noexcept : ptr(img.ptr), stride(img.stride), width(img.width), height(img.height), dx_format(img.dx_format), managed_(img.managed_) {
            img.ptr = nullptr;
            img.stride = 0;
            img.width = 0;
            img.height = 0;
        }

        PixelAccess* At(size_t x, size_t y) {
            return reinterpret_cast<PixelAccess*>(&ptr[(y * width + x) * stride]);
        }

        const PixelAccess* At(size_t x, size_t y) const {
            return reinterpret_cast<PixelAccess*>(&ptr[(y * width + x) * stride]);
        }

        void CopyRegion(const Image& img, size_t start_x, size_t start_y) const {
            for (size_t i = 0; i < img.height; ++i) {
                size_t row = i + start_y;
                memcpy(ptr + row * width + start_x, img.ptr + i * img.width, stride * img.width);
            }
        }

        ~Image() {
            if (!managed_ && ptr != nullptr) {
                delete[] ptr;
                width = 0;
                height = 0;
                stride = 0;
            }
        }
    };

    class ImageLoader {
    public:
        static Image CreateBlankImage(size_t width, size_t height, DXGI_FORMAT dx_format) {
            if (dx_format == DXGI_FORMAT_R8_UNORM) return Image{ width, height, 1, dx_format };
            if (dx_format == DXGI_FORMAT_R8G8B8A8_UNORM) return Image{ width, height, 4, dx_format };
            LFATAL("Cannot create blank image because target DXGI_FORMAT is not support");
        }

        static Image CreateImageFromPixels(size_t width, size_t height, size_t src_pitch, uint8_t* ptr, DXGI_FORMAT dx_format) {
            Image img(width, height, src_pitch / width, dx_format);
            for (size_t i = 0; i < height; ++i) {
                memcpy(img.At(0, i), ptr + i * src_pitch, width);
            }
            return img;
        }

        static std::optional<Image> LoadJPG(const std::string& path) {
            auto jpeg_content = ReadFileIntoString(path);
            if (!jpeg_content.has_value()) {
                LFATAL("Cannot open image file {} while load image", path);
                return std::nullopt;
            }
            tjhandle handle = tjInitDecompress();
            if (handle == nullptr) {
                LFATAL("Cannot initialize TurboJPEG compress library when load image {}", path);
                return std::nullopt;
            }
            int width, height, subsamp, colorspace;
            if (tjDecompressHeader3(handle, reinterpret_cast<uint8_t*>(jpeg_content.value().data()), jpeg_content.value().size(), &width, &height, &subsamp, &colorspace) != 0) {
                LFATAL("Cannot decompress header in image file {} using TurboJPEG", path);
                tjDestroy(handle);
                return std::nullopt;
            }
            int target_format, stride;
            DXGI_FORMAT dx_format;
            if (colorspace == TJCS_GRAY) {
                target_format = TJPF_GRAY;
                dx_format = DXGI_FORMAT_R8_UNORM;
                stride = 1;
            } else if (colorspace == TJCS_RGB || colorspace == TJCS_YCbCr) {
                target_format = TJPF_RGBA;
                dx_format = DXGI_FORMAT_R8G8B8A8_UNORM;
                stride = 4;
            } else {
                LFATAL("Cannot decompress jpeg file {} because its color space {} is not supported", path, static_cast<int>(colorspace));
                tjDestroy(handle);
                return std::nullopt;
            }
            Image img(width, height, stride, dx_format);
            UniqueByteBuffer pixels(reinterpret_cast<char*>(img.ptr), width * height * colorspace);
            if (tjDecompress2(handle, reinterpret_cast<uint8_t*>(jpeg_content.value().data()), jpeg_content.value().size(), reinterpret_cast<uint8_t*>(&pixels[0]), width, 0, height, target_format, TJFLAG_FASTDCT) != 0) {
                LFATAL("Cannot decompress image file {} using TurboJPEG", path);
                tjDestroy(handle);
                return std::nullopt;
            }
            tjDestroy(handle);
            return img;
        }

        static bool StoreJPG(const std::string& path, const Image& img, int quality = 100, int subsamp = TJSAMP_422) {
            tjhandle handle = tjInitCompress();
            if (handle == nullptr) {
                LFATAL("Cannot initialize TurboJPEG compress library when store image {}", path);
                return false;
            }
            uint8_t* jpeg_buf = nullptr;
            unsigned long jpeg_size = 0;
            int target_format;
            if (img.dx_format == DXGI_FORMAT_R8_UNORM) {
                target_format = TJPF_GRAY;
                subsamp = TJSAMP_GRAY;
            } else if (img.dx_format == DXGI_FORMAT_R8G8B8A8_UNORM) {
                target_format = TJPF_RGB;
            } else {
                LFATAL("Cannot compress jpeg file {} because its DXGI_FORMAT {} is not supported", path, static_cast<int>(img.dx_format));
                tjDestroy(handle);
                return false;
            }
            if (tjCompress2(handle, img.ptr, img.width, 0, img.height, target_format, &jpeg_buf, &jpeg_size, subsamp, quality, TJFLAG_FASTDCT) != 0) {
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

        static std::optional<Image> LoadPNG(const std::string& path) {
            if (stbi_is_16_bit(path.c_str())) {
                int width, height, channels;
                uint16_t* pixels = stbi_load_16(path.c_str(), &width, &height, &channels, 4);
                if (pixels == nullptr) {
                    LFATAL("Cannot load png file {} because stbi_load_16 returns nullptr");
                }
                Image img { static_cast<size_t>(width), static_cast<size_t>(height), sizeof(char) * 8, DXGI_FORMAT_R16G16B16A16_UNORM };
                memcpy(img.At(0, 0), pixels, width * height * img.stride);
                return img;
            } else {
                int width, height, channels;
                uint8_t* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
                if (pixels == nullptr) {
                    LFATAL("Cannot load png file {} because stbi_load returns nullptr");
                }
                Image img { static_cast<size_t>(width), static_cast<size_t>(height), sizeof(char) * 4, DXGI_FORMAT_R8G8B8A8_UNORM };
                memcpy(img.At(0, 0), pixels, width * height * img.stride);
                return img;
            }

        }



    };
}