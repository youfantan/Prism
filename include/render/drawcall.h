#pragma once

#include <base.h>

#include <render/queue.h>

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

    class Pipeline {
    protected:
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc_ {};
        ComPtr<ID3D12PipelineState> pso_;
        ComPtr<ID3D12RootSignature> sign_;
    public:
        ComPtr<ID3D12PipelineState> GetPSO() {
            return pso_;
        }

        ComPtr<ID3D12RootSignature> GetSignature() {
            return sign_;
        }
    };

    class Drawcall {
    public:
        virtual RecordDispatcher::GPUProcess CreateRenderProcess() = 0;
        virtual ~Drawcall() = default;
    };
}
