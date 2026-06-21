#pragma once

#include <base.h>
#include <render/resource.h>

#include <vector>

#include "framework.h"

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

static constexpr D3D12_BLEND_DESC AlphaBlendDesc = {
    .AlphaToCoverageEnable = FALSE,
    .IndependentBlendEnable = FALSE,
    .RenderTarget = {
        D3D12_RENDER_TARGET_BLEND_DESC {
            .BlendEnable = true,
            .LogicOpEnable = false,
            .SrcBlend = D3D12_BLEND_SRC_ALPHA,
            .DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
            .BlendOp = D3D12_BLEND_OP_ADD,
            .SrcBlendAlpha = D3D12_BLEND_ONE,
            .DestBlendAlpha = D3D12_BLEND_ZERO,
            .BlendOpAlpha = D3D12_BLEND_OP_ADD,
            .LogicOp = D3D12_LOGIC_OP_NOOP,
            .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
        },
    },
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

class Drawcall {
    friend class RenderContext;
public:
    struct DrawcallRecord {
        Drawcall* drawcall;
        std::vector<Resource*> resources;
    };
protected:
    ComPtr<ID3D12Device> device_;
    BindlessHeap& heap_;
    RenderQueue& queue_;
    ResourceManager& res_mgr_;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc_;
    ComPtr<ID3D12PipelineState> pso_;
    ComPtr<ID3D12RootSignature> sign_;
    Waitable waitable_;
    std::vector<D3D12_ROOT_PARAMETER> rparams_;
    StaticSamplers samplers_;

    virtual bool BuildPipeline() {
        D3D12_ROOT_SIGNATURE_DESC rs_desc {};
        rs_desc.NumParameters = rparams_.size();
        rs_desc.pParameters = rparams_.data();
        rs_desc.NumStaticSamplers = samplers_.GetSize();
        rs_desc.pStaticSamplers = samplers_.GetDescs();
        rs_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
            | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
        ComPtr<ID3DBlob> serialized;
        ComPtr<ID3DBlob> error;
        HRESULT r = D3D12SerializeRootSignature(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error);
        if (!SUCCEEDED(r)) {
            LFATAL("Cannot serialize root signature, because: {}", static_cast<const char*>(error->GetBufferPointer()));CHECKHR(r);
            return false;
        }
        device_->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&sign_));
        pso_desc_.pRootSignature = sign_.Get();
        device_->CreateGraphicsPipelineState(&pso_desc_, IID_PPV_ARGS(&pso_));
        return true;
    }

    virtual void Draw(RenderPass& rp, ConstantBuffer* cb, VertexBuffer* vb, IndexBuffer* ib) {
        auto vbv = res_mgr_.GetMap().QueryResourceView(vb->GetName(), "default_vb_view").value()->data.vb_view;
        auto ibv = res_mgr_.GetMap().QueryResourceView(ib->GetName(), "default_ib_view").value()->data.ib_view;
        rp.command_list->SetPipelineState(pso_.Get());
        rp.command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D12DescriptorHeap* heaps[] = {heap_.GetComPtr().Get()};
        rp.command_list->SetDescriptorHeaps(1, heaps);
        rp.command_list->SetGraphicsRootSignature(sign_.Get());
        rp.command_list->SetGraphicsRootConstantBufferView(0, cb->GetComPtr()->GetGPUVirtualAddress());
        rp.command_list->SetGraphicsRootDescriptorTable(1, heap_.GetComPtr()->GetGPUDescriptorHandleForHeapStart());
        rp.command_list->IASetVertexBuffers(0, 1, &vbv);
        rp.command_list->IASetIndexBuffer(&ibv);
        rp.command_list->DrawIndexedInstanced(ibv.SizeInBytes / sizeof(uint32_t), 1, 0, 0, 0);
        rp.drawcalls.push_back(this);
        rp.referenced_resources.push_back(cb);
        rp.referenced_resources.push_back(vb);
        rp.referenced_resources.push_back(ib);
    }
public:
    Drawcall(ComPtr<ID3D12Device>& device, RenderQueue& queue, BindlessHeap& heap, ResourceManager& res_mgr) : device_(device), queue_(queue), heap_(heap), res_mgr_(res_mgr), waitable_(queue.GetRenderFence(), 0), pso_desc_() {
        rparams_.resize(2);
        pso_desc_.BlendState = DefaultBlendDesc;
        pso_desc_.DepthStencilState = DefaultDepthStencilDesc;
        pso_desc_.RasterizerState = DefaultRasterizerDesc;
        rparams_[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rparams_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rparams_[0].Descriptor.ShaderRegister = 0;
        rparams_[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rparams_[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rparams_[1].DescriptorTable.NumDescriptorRanges = 3;
        rparams_[1].DescriptorTable.pDescriptorRanges = heap_.GetDescriptorRange();
    }

    virtual ~Drawcall() {
        waitable_.CPUWait();
    }

};