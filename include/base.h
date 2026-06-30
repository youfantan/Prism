#pragma once

#include <concepts>
#include <dxgi.h>
#include <d3d12.h>
#include <cstdint>
#include <dxgi1_6.h>
#include <DirectXTex/DirectXTex.h>
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <DirectXMath.h>
#include <format>
#include <iostream>
#include <source_location>
#include <wrl.h>
#include <windows.h>
#include <bcrypt.h>
#include <sstream>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace Prism
{
    class Device;

enum class ResourceType {
    RenderTarget,
    DepthBuffer,
    VarStructuredObject,
    ConstStructuredObject,
    VarStructuredArray,
    ConstStructuredArray,
    Texture,
};

enum class ResourceViewType {
    RTV,
    DSV,
    SRV,
    CBV,
    UAV,
    VBV,
    IBV,
};

enum class MSAAType : uint32_t {
    NONE,
    MSAA_4X,
};

enum class ShaderType {
    VertexShader,
    PixelShader,
    Unknown,
};

using dx_init_t = struct {
    uint32_t width;
    uint32_t height;
    uint32_t buffer_count;
    uint32_t render_threads_count;
    uint32_t copy_threads_count;
    uint32_t lists_per_render_thread;
    uint32_t lists_per_copy_thread;
    uint32_t fps_limit;
    uint32_t max_texture_count;
    MSAAType msaa_type;
    DXGI_FORMAT rt_format;
    DXGI_FORMAT ds_format;
    float rt_clear_color[4];
    bool enable_vsync;
    std::string shaders_dir;
    std::string textures_dir;
    std::string assets_dir;
};

inline DXGI_SAMPLE_DESC GetSampleDesc(MSAAType type) {
    uint32_t count = 1;
    switch (type) {
        case MSAAType::NONE: count = 1;
        case MSAAType::MSAA_4X: count = 4;
    }

    return {
        .Count = count,
        .Quality = 0
    };
}

class DXAllocator {
protected:
    ComPtr<ID3D12Device> device_;
public:
    DXAllocator(ComPtr<ID3D12Device> device) : device_(device) {}
    virtual ID3D12Resource* CreateLocalResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES states, D3D12_CLEAR_VALUE* pclr) = 0;
    virtual ID3D12Resource* CreateRemoteResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES states, D3D12_CLEAR_VALUE* pclr) = 0;
    virtual void FreeResource(ID3D12Resource* resource) = 0;
    virtual ~DXAllocator() = default;
};

class Device {
    ComPtr<IDXGIFactory7> factory_;
    ComPtr<IDXGIAdapter> adapter_;
    ComPtr<ID3D12Device> device_;
public:
    Device();

    ComPtr<ID3D12Device>& GetComPtr() {
        return device_;
    }

    ComPtr<IDXGIFactory7>& GetFactory() {
        return factory_;
    }

    ComPtr<IDXGIAdapter>& GetSelectedAdapter() {
        return adapter_;
    }

    operator ComPtr<ID3D12Device>() const {
        return device_;
    }
};

class Resource;
class UploadBuffer;
class RenderContext;
class TextureHeap;
class ShaderLoader;
class TextureLoader;
class Fence;
class Waitable;

template<typename T>
requires std::same_as<T, HRESULT>
void CHECKHR(T t, const std::source_location& location = std::source_location::current()) {
    if (!SUCCEEDED(t)) {
        std::cout << std::format("Error Occurred at {}:{} while calling function {}\n", location.file_name(), location.line(), location.function_name());
    }
}

constexpr static std::string_view hex_lowercase("0123456789abcdef");
constexpr static std::string_view hex_uppercase("0123456789ABCDEF");

inline std::string ToHexLowerCase(const uint8_t* d, size_t n) {
    std::stringstream ss;
    for (size_t i = 0; i < n; ++i) {
        uint8_t k = d[i];
        ss << hex_lowercase[k / 16];
        ss << hex_lowercase[k % 16];
    }
    return ss.str();
}

template<typename Vector>
requires requires(Vector v) { v.size(); v[0]; }
inline std::string CalcSHA256HexDigest(Vector& input) {
    uint8_t digest[32] {};
    BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, (PBYTE)(&input[0]), input.size(), digest, 32);
    return ToHexLowerCase(digest, 32);
}
}