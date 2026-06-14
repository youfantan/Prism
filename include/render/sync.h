#pragma once

#include <base.h>

class Waitable;

class Fence {
    friend class Waitable;
private:
    ComPtr<ID3D12Fence> fence_;
    uint64_t value_;
    HANDLE evt_;

    void GPUWait(uint64_t value, ComPtr<ID3D12CommandQueue>& queue);
    void CPUWait(uint64_t value, uint32_t wait_ms = INFINITE);
public:
    explicit Fence(ComPtr<ID3D12Device>& device);
    Fence(const Fence&) = delete;
    Fence(Fence&&) = delete;
    Waitable AllocateWaitable();
    void GPUSync(Waitable& waitable, ComPtr<ID3D12CommandQueue>& queue);

    uint64_t GetCompleteValue() {
        return fence_->GetCompletedValue();
    }

    ~Fence() {
        CloseHandle(evt_);
    }
};

class Waitable {
private:
    uint64_t fence_value_;
    Fence* fence_;
public:
    Waitable(Fence& fence, uint64_t fv) : fence_(&fence), fence_value_(fv) {}
    Waitable(const Waitable&) = delete;
    Waitable(Waitable&& waitable) noexcept : fence_(waitable.fence_), fence_value_(waitable.fence_value_) {
        waitable.fence_ = nullptr;
        waitable.fence_value_ = 0;
    }

    Waitable& operator=(Waitable&& waitable) noexcept {
        fence_ = waitable.fence_;
        fence_value_ = waitable.fence_value_;
        waitable.fence_value_ = 0;
        return *this;
    }

    void CPUWait(uint32_t wait_ms = INFINITE) {
        fence_->CPUWait(fence_value_, wait_ms);
    }

    void GPUWait(ComPtr<ID3D12CommandQueue>& queue) {
        fence_->GPUWait(fence_value_, queue);
    }

    uint64_t& GetFenceValue() {
        return fence_value_;
    }

    bool Completed() const {
        return fence_->GetCompleteValue() >= fence_value_;
    }
};