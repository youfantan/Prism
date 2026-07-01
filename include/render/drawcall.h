#pragma once

#include <base.h>

#include <render/queue.h>

#include "resource.h"

namespace Prism
{
    template<uint32_t REG, uint32_t SPACE>
    D3D12_STATIC_SAMPLER_DESC LINEAR_SAMPLER_DESC = {
        .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .MipLODBias = 0.0f,
        .MaxAnisotropy = 16,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
        .BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
        .MinLOD = 0.0f,
        .MaxLOD = D3D12_FLOAT32_MAX,
        .ShaderRegister = REG,
        .RegisterSpace = SPACE ,
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
    };

    template<uint32_t REG, uint32_t SPACE>
    D3D12_STATIC_SAMPLER_DESC LINEAR_COMPARE_SAMPLER_DESC = {
        .Filter =  D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .MipLODBias = 0.0f,
        .MaxAnisotropy = 16,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
        .BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
        .MinLOD = 0.0f,
        .MaxLOD = D3D12_FLOAT32_MAX,
        .ShaderRegister = REG,
        .RegisterSpace = SPACE ,
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
    };

    class RootSignature {
        ComPtr<ID3D12Device> device_;
        ComPtr<ID3D12RootSignature> sign_;
        std::vector<D3D12_ROOT_PARAMETER> root_parameters_;
        std::vector<D3D12_STATIC_SAMPLER_DESC> static_samplers_;
    public:
        RootSignature(ComPtr<ID3D12Device> device) : device_(device) {

        }

        RootSignature& BindConstantBuffer(uint32_t reg, uint32_t space) {
            D3D12_ROOT_PARAMETER rp {
                .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
                .Descriptor = {
                    .ShaderRegister = reg,
                    .RegisterSpace = space
                },
                .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
            };
            root_parameters_.push_back(rp);
            return *this;
        }

        RootSignature& BindStructuredBuffer(uint32_t reg, uint32_t space) {
            D3D12_ROOT_PARAMETER rp {
                .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
                .Descriptor = {
                    .ShaderRegister = reg,
                    .RegisterSpace = space
                },
                .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
            };
            root_parameters_.push_back(rp);
            return *this;
        }

        RootSignature& BindTextureHeap(TextureHeap& heap) {
            auto [p, n] = heap.GetDescriptorRanges();
            D3D12_ROOT_PARAMETER rp {
                .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                .DescriptorTable = {
                    .NumDescriptorRanges = n,
                    .pDescriptorRanges = p,
                },
                .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
            };
            root_parameters_.push_back(rp);
            return *this;
        }

        RootSignature& BindStaticSampler(const D3D12_STATIC_SAMPLER_DESC& desc) {
            static_samplers_.push_back(desc);
            return *this;
        }

        ComPtr<ID3D12RootSignature> Build(D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED) {
            ComPtr<ID3DBlob> err, rs;
            D3D12_ROOT_SIGNATURE_DESC rs_desc {
                .NumParameters = static_cast<uint32_t>(root_parameters_.size()),
                .pParameters = root_parameters_.data(),
                .NumStaticSamplers = static_cast<uint32_t>(static_samplers_.size()),
                .pStaticSamplers = static_samplers_.data(),
                .Flags = flags
            };
            HRESULT hr = D3D12SerializeRootSignature(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rs, &err);
            if (!SUCCEEDED(hr)) {
                LFATAL("Cannot serialize root signature when create object drawcall: {}", static_cast<char*>(err->GetBufferPointer()));
            }
            device_->CreateRootSignature(0, rs->GetBufferPointer(), rs->GetBufferSize(), IID_PPV_ARGS(&sign_));
            return sign_;
        }

        ID3D12RootSignature* GetD3D12Signature() {
            return sign_.Get();
        }
    };

    class Pipeline {
    protected:
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc_ {};
        ComPtr<ID3D12PipelineState> pso_;
        RootSignature rs_;
    public:
        Pipeline(ComPtr<ID3D12Device> device) : rs_(device) {

        }

        ComPtr<ID3D12PipelineState> GetPSO() {
            return pso_;
        }

        ID3D12RootSignature* GetSignature() {
            return rs_.GetD3D12Signature();
        }
    };

    class Drawcall {
    public:
        virtual RecordDispatcher::GPUProcess CreateRenderProcess() = 0;
        virtual ~Drawcall() = default;
    };
}
