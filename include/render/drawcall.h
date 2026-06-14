#pragma once

#include <base.h>
#include <render/resource.h>

#include <vector>

class StaticSamplers {
private:
    static D3D12_STATIC_SAMPLER_DESC Default() {
        D3D12_STATIC_SAMPLER_DESC desc {};
        desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        desc.MaxLOD = D3D12_FLOAT32_MAX;
        desc.MinLOD = 0.0f;
        desc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        desc.MaxAnisotropy = 16;
        desc.MipLODBias = 0.0f;
        return desc;
    }
    std::vector<D3D12_STATIC_SAMPLER_DESC> samplers_;
public:
    StaticSamplers() = default;
    void Add(const D3D12_STATIC_SAMPLER_DESC& desc) {
        samplers_.push_back(desc);
    }

    const D3D12_STATIC_SAMPLER_DESC* GetDescs() const {
        return &samplers_[0];
    }

    uint32_t GetSize() const {
        return samplers_.size();
    }

    static D3D12_STATIC_SAMPLER_DESC LINEAR_FILTER(uint32_t reg) {
        D3D12_STATIC_SAMPLER_DESC desc = Default();
        desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        desc.ShaderRegister = reg;
        return desc;
    }

};

struct DrawcallResource {
    ComPtr<ID3DBlob> vs_bytecode;
    ComPtr<ID3DBlob> ps_bytecode;
    D3D12_RASTERIZER_DESC rasterizer_desc;
    D3D12_BLEND_DESC blend_desc;
    D3D12_DEPTH_STENCIL_DESC ds_desc;
    DXGI_SAMPLE_DESC sample_desc;
    D3D12_INPUT_LAYOUT_DESC iv_layout;
    StaticSamplers samplers;
};

class Drawcall {
    friend class RenderContext;
private:
    ComPtr<ID3D12Device> device_;
    BindlessHeap& heap_;
    RenderQueue& queue_;
    ComPtr<ID3D12PipelineState> pso_;
    ComPtr<ID3D12RootSignature> sign_;
    Waitable waitable_;
public:
    Drawcall(ComPtr<ID3D12Device>& device, RenderQueue& queue, BindlessHeap& heap, const DrawcallResource& resource);
    void operator()(RenderContext& ctx, Resource<ResourceType::ConstBuffer>& mapping, Resource<ResourceType::VertexBuffer>& vb, Resource<ResourceType::IndexBuffer>& ib);
    ~Drawcall() {
        waitable_.CPUWait();
    }

};

static constexpr D3D12_RASTERIZER_DESC DefaultRasterizerDesc = {
    .FillMode = D3D12_FILL_MODE_SOLID,
    .CullMode = D3D12_CULL_MODE_BACK,
    .FrontCounterClockwise = FALSE,
    .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
    .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
    .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
    .DepthClipEnable = TRUE,
    .MultisampleEnable = FALSE,
    .AntialiasedLineEnable = FALSE,
    .ForcedSampleCount = 0,
    .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
};

static constexpr D3D12_BLEND_DESC DefaultBlendDesc = {
    .AlphaToCoverageEnable = FALSE,
    .IndependentBlendEnable = FALSE,
    .RenderTarget = {
        FALSE,FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL,
    }
};

static constexpr D3D12_DEPTH_STENCIL_DESC DefaultDepthStencilDesc = {
    .DepthEnable = TRUE,
    .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
    .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
    .StencilEnable = FALSE,
    .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
    .StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
    .FrontFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS },
    .BackFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS }
};