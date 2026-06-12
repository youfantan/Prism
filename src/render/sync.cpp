#include <render/sync.h>

void Fence::GPUWait(uint64_t value, ComPtr<ID3D12CommandQueue>& queue) {
    if (value == 0) return;
    queue->Wait(fence_.Get(), value);
}

void Fence::CPUWait(uint64_t value, uint32_t wait_ms) {
    if (value == 0) return;
    if (value > fence_->GetCompletedValue()) {
        fence_->SetEventOnCompletion(value, evt_);
        WaitForSingleObject(evt_, wait_ms);
    }
}

Fence::Fence(ComPtr<ID3D12Device>& device) : value_(0) {
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    evt_ = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
}

Waitable Fence::AllocateWaitable() {
    return Waitable{ *this, ++value_ };
}

void Fence::GPUSync(Waitable& waitable, ComPtr<ID3D12CommandQueue>& queue) {
    queue->Signal(fence_.Get(), waitable.GetFenceValue());
}
