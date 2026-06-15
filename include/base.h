#pragma once

#include <concepts>
#include <dxgi.h>
#include <d3d12.h>
#include <cstdint>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <format>
#include <iostream>
#include <source_location>
#include <wrl.h>
#include <windows.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class Device;

enum class ResourceType {
    RenderTarget,
    DepthBuffer,
    ConstBuffer,
    ShaderResource,
    VertexBuffer,
    IndexBuffer,
    Texture,
    UnorderedAccess,
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
};

inline uint32_t GetSampleCount(MSAAType type) {
    switch (type) {
        case MSAAType::NONE: return 1;
        case MSAAType::MSAA_4X: return 4;
        default: return 1;
    }
}

class DXAllocator {
protected:
    ComPtr<ID3D12Device> device_;
public:
    DXAllocator(ComPtr<ID3D12Device> device) : device_(device) {}
    virtual ComPtr<ID3D12Resource> CreateLocalResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES states = D3D12_RESOURCE_STATE_COMMON, D3D12_CLEAR_VALUE* pclr = nullptr) = 0;
    virtual ComPtr<ID3D12Resource> CreateRemoteResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES states = D3D12_RESOURCE_STATE_COMMON, D3D12_CLEAR_VALUE* pclr = nullptr) = 0;
    virtual void FreeResource(ComPtr<ID3D12Resource> resource) = 0;
};

class Resource;
class UploadBuffer;
class ResourceManager;
class RenderContext;
class Drawcall;

template<typename T>
requires std::same_as<T, HRESULT>
void CHECKHR(T t, const std::source_location& location = std::source_location::current()) {
    if (!SUCCEEDED(t)) {
        std::cout << std::format("Error Occurred at {}:{} while calling function {}\n", location.file_name(), location.line(), location.function_name());
    }
}