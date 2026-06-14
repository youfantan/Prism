#include <render/resource.h>

int32_t BindlessHeap::AssignIndex(Type type) {
    uint32_t beg = 0;
    if (type == Type::CBV) beg = srv_scope_;
    if (type == Type::UAV) beg = srv_scope_ + cbv_scope_;
    for (int i = beg; i < resident_.size(); ++i) {
        if (!resident_[i]) {
            resident_[i] = true;
            return i;
        }
    }
    return -1;
}

BindlessHeap::BindlessHeap(ComPtr<ID3D12Device>& device, uint32_t srv_scope, uint32_t cbv_scope, uint32_t uav_scope) : DescriptorHeap(device, srv_scope + cbv_scope + uav_scope, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE), device_(device), srv_scope_(srv_scope), cbv_scope_(cbv_scope), uav_scope_(uav_scope), resident_(size_), ranges_{} {
    ranges_[0].BaseShaderRegister = 0; // SRV register from t0
    ranges_[0].NumDescriptors = srv_scope_;
    ranges_[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges_[0].OffsetInDescriptorsFromTableStart = 0;
    ranges_[1].BaseShaderRegister = 1; // CBV register from b1
    ranges_[1].NumDescriptors = cbv_scope_;
    ranges_[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges_[1].OffsetInDescriptorsFromTableStart = srv_scope_;
    ranges_[2].BaseShaderRegister = 0; // UAV register from u0
    ranges_[2].NumDescriptors = uav_scope_;
    ranges_[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges_[2].OffsetInDescriptorsFromTableStart = srv_scope_ + cbv_scope_;
}

std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> BindlessHeap::BindShaderResource(std::string_view name, Resource<ResourceType::Texture>& res) {
    int32_t index = AssignIndex(Type::SRV);
    if (index == -1) return std::nullopt;
    D3D12_SHADER_RESOURCE_VIEW_DESC desc {};
    desc.Format = res.GetComPtr()->GetDesc().Format;
    if (res.GetComPtr()->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    } else if (res.GetComPtr()->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;
    } else {
        desc.ViewDimension = D3D12_SRV_DIMENSION_UNKNOWN;
    }
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUHandle(index);
    device_->CreateShaderResourceView(res.GetComPtr().Get(), &desc, handle);
    res.GetView() = handle;
    heap_mapping_[name] = index;
#ifndef NDEBUG
    res.GetComPtr()->SetName(ConvertStringToWstring(name).value().data());
#endif
    return handle;
}

std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> BindlessHeap::BindConstantBuffer(std::string_view name, Resource<ResourceType::ConstBuffer>& res) {
    int32_t index = AssignIndex(Type::CBV);
    if (index == -1) return std::nullopt;
    D3D12_CONSTANT_BUFFER_VIEW_DESC desc {};
    desc.BufferLocation = res.GetComPtr()->GetGPUVirtualAddress();
    desc.SizeInBytes = res.GetComPtr()->GetDesc().Width;
    D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUHandle(index);
    device_->CreateConstantBufferView(&desc, handle);
    res.GetView() = handle;
    heap_mapping_[name] = index;
#ifndef NDEBUG
    res.GetComPtr()->SetName(ConvertStringToWstring(name).value().data());
#endif
    return handle;
}

int32_t BindlessHeap::QueryResourceIndex(std::string_view name) {
    if (heap_mapping_.contains(name)) {
        return heap_mapping_[name];
    }
    return -1;
}

bool BindlessHeap::Unbind(std::string_view name) {
    if (heap_mapping_.contains(name)) {
        resident_[heap_mapping_[name]] = false;
        heap_mapping_.erase(name);
        return true;
    }
    return false;
}
