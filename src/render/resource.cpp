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

BindlessHeap::BindlessHeap(ComPtr<ID3D12Device>& device, ResourceManager& res_mgr, uint32_t srv_scope, uint32_t cbv_scope, uint32_t uav_scope) : DescriptorHeap(device, srv_scope + cbv_scope + uav_scope, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE), device_(device), res_mgr_(res_mgr), srv_scope_(srv_scope), cbv_scope_(cbv_scope), uav_scope_(uav_scope), resident_(size_), ranges_{} {
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

std::optional<std::string> BindlessHeap::BindTexture(const std::string& name) {
    int32_t index = AssignIndex(Type::SRV);
    if (index == -1) return std::nullopt;
    D3D12_SHADER_RESOURCE_VIEW_DESC desc {};
    auto query = res_mgr_.GetMap().QueryResource<Texture>(name);
    if (!query.has_value()) return std::nullopt;
    desc.Format = query.value()->GetComPtr()->GetDesc().Format;
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipLevels = 1;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUHandle(index);
    device_->CreateShaderResourceView(query.value()->GetComPtr().Get(), &desc, handle);
    heap_mapping_[name] = index;
    ResourceView rv {};
    rv.type = ResourceViewType::SRV;
    rv.data.handle = handle;
    std::string default_view_name("bindless_srv");
    res_mgr_.GetMap().BindResourceView(name, default_view_name, rv);
    return default_view_name;
}

std::optional<std::string> BindlessHeap::BindStructuredBuffer(const std::string& name) {
    int32_t index = AssignIndex(Type::SRV);
    if (index == -1) return std::nullopt;
    D3D12_SHADER_RESOURCE_VIEW_DESC desc {};
    auto query = res_mgr_.GetMap().QueryResource<StructuredBuffer>(name);
    if (!query.has_value()) return std::nullopt;
    desc.Format = query.value()->GetComPtr()->GetDesc().Format;
    desc.Buffer.StructureByteStride = query.value()->GetStructureSize();
    desc.Buffer.FirstElement = 0;
    desc.Buffer.NumElements = query.value()->GetComPtr()->GetDesc().Width / desc.Buffer.StructureByteStride;
    desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUHandle(index);
    device_->CreateShaderResourceView(query.value()->GetComPtr().Get(), &desc, handle);
    heap_mapping_[name] = index;
    ResourceView rv {};
    rv.type = ResourceViewType::SRV;
    rv.data.handle = handle;
    std::string default_view_name("bindless_srv");
    res_mgr_.GetMap().BindResourceView(name, default_view_name, rv);
    return default_view_name;
}

std::optional<std::string> BindlessHeap::BindConstantBuffer(const std::string& name) {
    int32_t index = AssignIndex(Type::CBV);
    if (index == -1) return std::nullopt;
    auto query = res_mgr_.GetMap().QueryResource<ConstantBuffer>(name);
    if (!query.has_value()) return std::nullopt;
    D3D12_CONSTANT_BUFFER_VIEW_DESC desc {};
    desc.BufferLocation = query.value()->GetComPtr()->GetGPUVirtualAddress();
    desc.SizeInBytes = query.value()->GetComPtr()->GetDesc().Width;
    D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUHandle(index);
    device_->CreateConstantBufferView(&desc, handle);
    ResourceView rv {};
    rv.type = ResourceViewType::CBV;
    rv.data.handle = handle;
    std::string default_view_name("bindless_cbv");
    res_mgr_.GetMap().BindResourceView(name, default_view_name, rv);
    heap_mapping_[name] = index;
    return default_view_name;
}

int32_t BindlessHeap::QueryResourceIndex(const std::string& name) {
    if (heap_mapping_.contains(name)) {
        return heap_mapping_[name];
    }
    LFATAL("Cannot found request index of resource {} in bindless heap", name);
    return -1;
}

bool BindlessHeap::Unbind(const std::string& name) {
    if (heap_mapping_.contains(name)) {
        resident_[heap_mapping_[name]] = false;
        heap_mapping_.erase(name);
        return true;
    }
    return false;
}
