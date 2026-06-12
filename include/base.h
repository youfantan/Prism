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
    RenderTarget, // RTV
    DepthBuffer, // DSV
    ConstBuffer, // CBV
    ShaderResource, // SRV
    VertexBuffer, // VBV
    IndexBuffer, // IBV
    Texture, // SRV
    UnorderedAccess, // UAV
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

template<ResourceType RT>
class Resource;

class UploadBuffer;

template<typename A>
concept is_allocator = requires (A a, D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES state, D3D12_CLEAR_VALUE* optclr)
{
    { a.CreateLocalResource(desc, state, optclr) } -> std::same_as<ComPtr<ID3D12Resource>>;
    { a.CreateLocalResource(desc) } -> std::same_as<ComPtr<ID3D12Resource>>;
    { a.CreateRemoteResource(desc, state, optclr) } -> std::same_as<ComPtr<ID3D12Resource>>;
    { a.CreateRemoteResource(desc) } -> std::same_as<ComPtr<ID3D12Resource>>;

};

template<typename Allocator>
requires is_allocator<Allocator>
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