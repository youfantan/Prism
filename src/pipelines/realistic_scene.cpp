#include <piplines/realistic_scene.h>

Prism::LightSrcDrawcall::LightSrcPipeline::LightSrcPipeline(PrismApp* app) : Pipeline(app->GetDevice().GetComPtr()), app_(app) {
    std::vector<D3D12_INPUT_ELEMENT_DESC> layout;
    for (auto& l : VertexAttrs::MakeInputElements()) {
        layout.push_back(l);
    }
    for (auto& l : InstanceAttrs::MakeInputElements()) {
        layout.push_back(l);
    }
    rs_
    .BindTextureHeap(app_->GetResourceManager().GetTextureHeap())
    .BindConstantBuffer(0, 0)
    .BindConstantBuffer(1, 0)
    .BindStaticSampler(LINEAR_SAMPLER_DESC<0, 0>)
    .Build();
    pso_desc_.pRootSignature = rs_.GetD3D12Signature();
    pso_desc_.BlendState = {
        .AlphaToCoverageEnable = FALSE,
        .IndependentBlendEnable = FALSE,
        .RenderTarget = {
            D3D12_RENDER_TARGET_BLEND_DESC{
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
    pso_desc_.RasterizerState = {
        .FillMode = D3D12_FILL_MODE_SOLID,
        .CullMode = D3D12_CULL_MODE_BACK,
        .FrontCounterClockwise = false,
        .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
        .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
        .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
        .DepthClipEnable = true,
        .MultisampleEnable = app_->GetInitializeParams().msaa_type != MSAAType::NONE,
        .AntialiasedLineEnable = false,
        .ForcedSampleCount = 0,
        .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
    };
    pso_desc_.DepthStencilState = {
        .DepthEnable = true,
        .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
        .StencilEnable = false,
        .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
        .StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
        .FrontFace = {
            D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS
        },
        .BackFace = {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS}
    };
    pso_desc_.SampleDesc = GetSampleDesc(app_->GetInitializeParams().msaa_type);
    auto [vs_ptr, vs_len] = app_->GetShaderLoader().LoadShader("light", ShaderType::VertexShader).value();
    auto [ps_ptr, ps_len] = app_->GetShaderLoader().LoadShader("light", ShaderType::PixelShader).value();
    pso_desc_.VS = {vs_ptr, vs_len};
    pso_desc_.PS = {ps_ptr, ps_len};
    pso_desc_.NumRenderTargets = 1;
    pso_desc_.RTVFormats[0] = app_->GetInitializeParams().rt_format;
    pso_desc_.DSVFormat = app_->GetInitializeParams().ds_format;
    pso_desc_.NodeMask = 0;
    pso_desc_.InputLayout = {layout.data(), static_cast<uint32_t>(layout.size())};
    pso_desc_.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    pso_desc_.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc_.SampleMask = UINT_MAX;
    app_->GetDevice().GetComPtr()->CreateGraphicsPipelineState(&pso_desc_, IID_PPV_ARGS(&pso_));
}

Prism::ObjectDrawcall::ObjectPipeline::ObjectPipeline(PrismApp* app) : Pipeline(app->GetDevice().GetComPtr()), app_(app) {
    std::vector<D3D12_INPUT_ELEMENT_DESC> layout;
    for (auto& l : VertexAttrs::MakeInputElements()) {
        layout.push_back(l);
    }
    for (auto& l : InstanceAttrs::MakeInputElements()) {
        layout.push_back(l);
    }
    rs_
    .BindTextureHeap(app->GetResourceManager().GetTextureHeap())
    .BindConstantBuffer(0, 0)
    .BindConstantBuffer(1, 0)
    .BindStaticSampler(LINEAR_SAMPLER_DESC<0, 0>)
    .BindStaticSampler(LINEAR_COMPARE_SAMPLER_DESC<1, 0>)
    .Build();
    pso_desc_.pRootSignature = rs_.GetD3D12Signature();
    pso_desc_.BlendState = {
        .AlphaToCoverageEnable = FALSE,
        .IndependentBlendEnable = FALSE,
        .RenderTarget = {
            D3D12_RENDER_TARGET_BLEND_DESC{
                .BlendEnable = true, .LogicOpEnable = false, .SrcBlend = D3D12_BLEND_SRC_ALPHA,
                .DestBlend = D3D12_BLEND_INV_SRC_ALPHA, .BlendOp = D3D12_BLEND_OP_ADD, .SrcBlendAlpha = D3D12_BLEND_ONE,
                .DestBlendAlpha = D3D12_BLEND_ZERO, .BlendOpAlpha = D3D12_BLEND_OP_ADD, .LogicOp = D3D12_LOGIC_OP_NOOP,
                .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
            },
        },
    };
    pso_desc_.RasterizerState = {
        .FillMode = D3D12_FILL_MODE_SOLID,
        .CullMode = D3D12_CULL_MODE_BACK,
        .FrontCounterClockwise = false,
        .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
        .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
        .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
        .DepthClipEnable = true,
        .MultisampleEnable = app_->GetInitializeParams().msaa_type != MSAAType::NONE,
        .AntialiasedLineEnable = false,
        .ForcedSampleCount = 0,
        .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
    };
    pso_desc_.DepthStencilState = {
        .DepthEnable = true,
        .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
        .StencilEnable = false,
        .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
        .StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
        .FrontFace = {
            D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS
        },
        .BackFace = {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS}
    };
    pso_desc_.SampleDesc = GetSampleDesc(app_->GetInitializeParams().msaa_type);
    auto [vs_ptr, vs_len] = app_->GetShaderLoader().LoadShader("object", ShaderType::VertexShader).value();
    auto [ps_ptr, ps_len] = app_->GetShaderLoader().LoadShader("object", ShaderType::PixelShader).value();
    pso_desc_.VS = {vs_ptr, vs_len};
    pso_desc_.PS = {ps_ptr, ps_len};
    pso_desc_.NumRenderTargets = 1;
    pso_desc_.RTVFormats[0] = app_->GetInitializeParams().rt_format;
    pso_desc_.DSVFormat = app_->GetInitializeParams().ds_format;
    pso_desc_.NodeMask = 0;
    pso_desc_.InputLayout = {layout.data(), static_cast<uint32_t>(layout.size())};
    pso_desc_.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    pso_desc_.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc_.SampleMask = UINT_MAX;
    app_->GetDevice().GetComPtr()->CreateGraphicsPipelineState(&pso_desc_, IID_PPV_ARGS(&pso_));
}

Prism::ObjectDrawcall::PBRObjectPipeline::PBRObjectPipeline(PrismApp* app) : Pipeline(app->GetDevice().GetComPtr()), app_(app) {
    std::vector<D3D12_INPUT_ELEMENT_DESC> layout;
    for (auto& l : VertexAttrs::MakeInputElements()) {
        layout.push_back(l);
    }
    for (auto& l : InstanceAttrs::MakeInputElements()) {
        layout.push_back(l);
    }
    rs_
    .BindTextureHeap(app->GetResourceManager().GetTextureHeap())
    .BindConstantBuffer(0, 0)
    .BindConstantBuffer(1, 0)
    .BindStaticSampler(LINEAR_SAMPLER_DESC<0, 0>)
    .BindStaticSampler(LINEAR_COMPARE_SAMPLER_DESC<1, 0>)
    .Build();
    pso_desc_.pRootSignature = rs_.GetD3D12Signature();
    pso_desc_.BlendState = {
        .AlphaToCoverageEnable = FALSE,
        .IndependentBlendEnable = FALSE,
        .RenderTarget = {
            D3D12_RENDER_TARGET_BLEND_DESC{
                .BlendEnable = true, .LogicOpEnable = false, .SrcBlend = D3D12_BLEND_SRC_ALPHA,
                .DestBlend = D3D12_BLEND_INV_SRC_ALPHA, .BlendOp = D3D12_BLEND_OP_ADD, .SrcBlendAlpha = D3D12_BLEND_ONE,
                .DestBlendAlpha = D3D12_BLEND_ZERO, .BlendOpAlpha = D3D12_BLEND_OP_ADD, .LogicOp = D3D12_LOGIC_OP_NOOP,
                .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
            },
        },
    };
    pso_desc_.RasterizerState = {
        .FillMode = D3D12_FILL_MODE_SOLID,
        .CullMode = D3D12_CULL_MODE_NONE,
        .FrontCounterClockwise = false,
        .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
        .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
        .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
        .DepthClipEnable = true,
        .MultisampleEnable = app_->GetInitializeParams().msaa_type != MSAAType::NONE,
        .AntialiasedLineEnable = false,
        .ForcedSampleCount = 0,
        .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
    };
    pso_desc_.DepthStencilState = {
        .DepthEnable = true,
        .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
        .StencilEnable = false,
        .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
        .StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
        .FrontFace = {
            D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS
        },
        .BackFace = {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS}
    };
    pso_desc_.SampleDesc = GetSampleDesc(app_->GetInitializeParams().msaa_type);
    auto [vs_ptr, vs_len] = app_->GetShaderLoader().LoadShader("object", ShaderType::VertexShader).value();
    auto [ps_ptr, ps_len] = app_->GetShaderLoader().LoadShader("pbr_object", ShaderType::PixelShader).value();
    pso_desc_.VS = {vs_ptr, vs_len};
    pso_desc_.PS = {ps_ptr, ps_len};
    pso_desc_.NumRenderTargets = 1;
    pso_desc_.RTVFormats[0] = app_->GetInitializeParams().rt_format;
    pso_desc_.DSVFormat = app_->GetInitializeParams().ds_format;
    pso_desc_.NodeMask = 0;
    pso_desc_.InputLayout = {layout.data(), static_cast<uint32_t>(layout.size())};
    pso_desc_.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    pso_desc_.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc_.SampleMask = UINT_MAX;
    app_->GetDevice().GetComPtr()->CreateGraphicsPipelineState(&pso_desc_, IID_PPV_ARGS(&pso_));
    pso_->SetName(L"PBR Object Pipeline");
}

Prism::ObjectDrawcall::ShadowPipeline::ShadowPipeline(PrismApp* app) : Pipeline(app->GetDevice().GetComPtr()), app_(app) {
    std::vector<D3D12_INPUT_ELEMENT_DESC> layout;
    for (auto& l : VertexAttrs::MakeInputElements()) {
        layout.push_back(l);
    }
    for (auto& l : InstanceAttrs::MakeInputElements()) {
        layout.push_back(l);
    }
    rs_
    .BindConstantBuffer(0, 0)
    .BindConstantBuffer(1, 0)
    .Build(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    pso_desc_.pRootSignature = rs_.GetD3D12Signature();
    pso_desc_.BlendState = {
        .AlphaToCoverageEnable = FALSE,
        .IndependentBlendEnable = FALSE,
        .RenderTarget = {
            D3D12_RENDER_TARGET_BLEND_DESC{
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
    pso_desc_.RasterizerState = {
        .FillMode = D3D12_FILL_MODE_SOLID,
        .CullMode = D3D12_CULL_MODE_BACK,
        .FrontCounterClockwise = false,
        .DepthBias = 1000,
        .DepthBiasClamp = 0.0f,
        .SlopeScaledDepthBias = 1.0f,
        .DepthClipEnable = true,
        .MultisampleEnable = false,
        .AntialiasedLineEnable = false,
        .ForcedSampleCount = 0,
        .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
    };
    pso_desc_.DepthStencilState = {
        .DepthEnable = true,
        .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
        .StencilEnable = false,
        .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
        .StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
        .FrontFace = {
            D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS
        },
        .BackFace = {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS}
    };
    pso_desc_.SampleDesc = {1, 0};
    auto [vs_ptr, vs_len] = app_->GetShaderLoader().LoadShader("shadow", ShaderType::VertexShader).value();
    pso_desc_.VS = {vs_ptr, vs_len};
    pso_desc_.NumRenderTargets = 0;
    pso_desc_.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso_desc_.NodeMask = 0;
    pso_desc_.InputLayout = {layout.data(), static_cast<uint32_t>(layout.size())};
    pso_desc_.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    pso_desc_.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc_.SampleMask = UINT_MAX;
    app_->GetDevice().GetComPtr()->CreateGraphicsPipelineState(&pso_desc_, IID_PPV_ARGS(&pso_));
}
