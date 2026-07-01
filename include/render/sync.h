#pragma once

#include <base.h>

namespace Prism
{
    class Fence {
        friend class Waitable;
    private:
        ComPtr<ID3D12Fence> fence_;
        uint64_t value_ {};
        HANDLE evt_;
    public:
        explicit Fence(ComPtr<ID3D12Device>& device) {
            device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
            evt_ = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        }

        Fence(const Fence&) = delete;
        Fence(Fence&&) = delete;
        uint64_t AllocateValue(HANDLE mutex) {
            WaitForSingleObject(mutex, INFINITE);
            ++value_;
            ReleaseMutex(mutex);
            return value_;
        }

        void GPUSync(uint64_t value, ComPtr<ID3D12CommandQueue>& queue) {
            queue->Signal(fence_.Get(), value);
        }

        void GPUWait(uint64_t value, ComPtr<ID3D12CommandQueue>& queue) {
            if (value == 0) return;
            queue->Wait(fence_.Get(), value);
        }

        void CPUWait(uint64_t value, uint32_t wait_ms = INFINITE) {
            if (value == 0) return;
            if (value > fence_->GetCompletedValue()) {
                fence_->SetEventOnCompletion(value, evt_);
                WaitForSingleObject(evt_, wait_ms);
            }
        }
        uint64_t GetCompleteValue() {
            return fence_->GetCompletedValue();
        }

        void ReleaseSignal() {
            SetEvent(evt_);
        }

        ~Fence() {
            CloseHandle(evt_);
        }
    };

    class Waitable {
        Fence* fence_;
        ComPtr<ID3D12CommandQueue> queue_;
        uint64_t value_;

    public:
        Waitable(Fence& fence, ComPtr<ID3D12CommandQueue> queue) : fence_(&fence), queue_(queue), value_(0) {

        }
        Waitable(const Waitable&) = delete;
        Waitable(Waitable&& w) noexcept : fence_(w.fence_), queue_(std::move(w.queue_)), value_(w.value_) {
            w.fence_ = nullptr;
            w.value_ = 0;
        }

        uint64_t& GetFenceValue() {
            return value_;
        }

        void CPUWait() {
            fence_->CPUWait(value_);
        }

        bool Completed() const {
            return fence_->GetCompleteValue() >= value_;
        }

        void GPUWait() {
            fence_->GPUWait(value_, queue_);
        }
    };

    template<size_t N>
    class WaitableSet {
        Waitable waitables_[N];

    public:
        template<typename... Waitable>
        WaitableSet(Waitable&&... w) : waitables_{ Waitable(std::move(w))... } {}
        WaitableSet(const WaitableSet&) = delete;
        template<size_t... I>
        WaitableSet(WaitableSet&& ws, std::index_sequence<I...>) noexcept : waitables_{ std::move(ws.waitables_[I])... } {}
        WaitableSet(WaitableSet&& ws) noexcept : WaitableSet(std::move(ws), std::make_index_sequence<N>()) {}

        template<size_t I>
        Waitable& Get() {
            if constexpr (I >= N) {
                static_assert(false, "out of bounds");
            }
            return waitables_[I];
        }

        ~WaitableSet() {
            for (auto& w : waitables_) {
                w.CPUWait();
            }
        }
    };
}