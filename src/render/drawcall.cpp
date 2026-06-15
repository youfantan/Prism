#include <render/drawcall.h>
#include <render/framework.h>
#include <mlog.h>

Drawcall::Drawcall(ComPtr<ID3D12Device>& device, RenderQueue& queue, BindlessHeap& heap, ResourceManager& res_mgr, const DrawcallResource& resource) : device_(device), heap_(heap), queue_(queue), res_mgr_(res_mgr), waitable_(queue.GetRenderFence(), 0) {
    D3D12_ROOT_PARAMETER rps[2] {};
    rps[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rps[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rps[0].Descriptor.ShaderRegister = 0;
    rps[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rps[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rps[1].DescriptorTable.NumDescriptorRanges = 3;
    rps[1].DescriptorTable.pDescriptorRanges = heap_.GetDescriptorRange();
    D3D12_ROOT_SIGNATURE_DESC rs_desc {};
    rs_desc.NumParameters = 2;
    rs_desc.pParameters = rps;
    rs_desc.NumStaticSamplers = resource.samplers.GetSize();
    rs_desc.pStaticSamplers = resource.samplers.GetDescs();
    rs_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> error;
    HRESULT r = D3D12SerializeRootSignature(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error);
    if (!SUCCEEDED(r)) {
        LFATAL("Cannot serialize root signature, because: {}", static_cast<const char*>(error->GetBufferPointer()));CHECKHR(r);
    }
    device_->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&sign_));
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc {};
    pso_desc.pRootSignature = sign_.Get();
    pso_desc.VS = {resource.vs_bytecode->GetBufferPointer(), resource.vs_bytecode.Get()->GetBufferSize()};
    pso_desc.PS = {resource.ps_bytecode->GetBufferPointer(), resource.ps_bytecode.Get()->GetBufferSize()};
    pso_desc.BlendState = resource.blend_desc;
    pso_desc.DepthStencilState = resource.ds_desc;
    pso_desc.RasterizerState = resource.rasterizer_desc;
    pso_desc.SampleDesc = resource.sample_desc;
    pso_desc.InputLayout = resource.iv_layout;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso_desc.SampleMask = UINT_MAX;
    device_->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_));
}

void Drawcall::operator()(RenderContext& ctx, const std::string& presets_cb_name, const std::string& vb_name, std::string& ib_name) {
    ctx.RecordRenderList([&](ComPtr<ID3D12GraphicsCommandList> list) -> RenderContext::record_t {
        auto* cb = res_mgr_.GetMap().QueryResource<ConstantBuffer>(presets_cb_name).value();
        auto* vb = res_mgr_.GetMap().QueryResource<VertexBuffer>(vb_name).value();
        auto* ib = res_mgr_.GetMap().QueryResource<IndexBuffer>(ib_name).value();
        auto vbv = res_mgr_.GetMap().QueryResourceView(vb_name, "default_vb_view").value()->data.vb_view;
        auto ibv = res_mgr_.GetMap().QueryResourceView(ib_name, "default_ib_view").value()->data.ib_view;
        list->SetPipelineState(pso_.Get());
        list->SetGraphicsRootSignature(sign_.Get());
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        list->SetGraphicsRootConstantBufferView(0, cb->GetComPtr()->GetGPUVirtualAddress());
        ID3D12DescriptorHeap* heaps[] = {heap_.GetComPtr().Get()};
        list->SetDescriptorHeaps(1, heaps);
        list->SetGraphicsRootDescriptorTable(1, heap_.GetComPtr()->GetGPUDescriptorHandleForHeapStart());
        list->IASetVertexBuffers(0, 1, &vbv);
        list->IASetIndexBuffer(&ibv);
        list->DrawIndexedInstanced(ibv.SizeInBytes / sizeof(uint32_t), 1, 0, 0, 0);
        return {
            .drawcall = this,
            .resources = { cb, vb, ib }
        };
    });
}
