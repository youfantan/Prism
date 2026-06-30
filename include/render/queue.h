#pragma once

#include <base.h>
#include <render/sync.h>
#include <vector>
#include <deque>

#include <io/image.h>

namespace Prism
{

class RecordDispatcher {
public:
    using GPUProcess = struct {
        std::function<void(ComPtr<ID3D12GraphicsCommandList>, uint64_t)> record;
        std::function<void()> sync;
    };
    struct Recorder {
        ComPtr<ID3D12GraphicsCommandList> list;
        ComPtr<ID3D12CommandAllocator> alloc;
        uint64_t fence_value;
    };
private:
    struct WorkerContext {
        HANDLE thread_handle;
        Fence* fence;
        std::vector<Recorder> recorders;
        std::deque<std::vector<GPUProcess>> tasks;
        ComPtr<ID3D12CommandQueue> queue;
        HANDLE mutex;
        HANDLE event;
        bool flag;
    };

    ComPtr<ID3D12CommandQueue> render_queue_;
    Fence render_fence_;
    std::vector<WorkerContext*> contexts;

    static DWORD WorkerFunc(LPVOID param) {
        auto* ctx = reinterpret_cast<WorkerContext*>(param);
        while (ctx->flag) {
            WaitForSingleObject(ctx->mutex, INFINITE);
            if (!ctx->tasks.empty()) {
                auto task = ctx->tasks.front();
                ctx->tasks.pop_front();
                ReleaseMutex(ctx->mutex);
                uint64_t min_fv = ctx->recorders[0].fence_value;
                auto* recorder = &ctx->recorders[0];
                for (auto& rec : ctx->recorders) {
                    if (rec.fence_value < min_fv) {
                        min_fv = rec.fence_value;
                        recorder = &rec;
                    }
                }
                ctx->fence->CPUWait(recorder->fence_value);
                uint64_t nfv = ctx->fence->AllocateValue(ctx->mutex);
                recorder->alloc->Reset();
                recorder->list->Reset(recorder->alloc.Get(), nullptr);
                for (auto& process : task) {
                    process.record(recorder->list, nfv);
                }
                recorder->list->Close();
                ID3D12CommandList* lists[1] = { recorder->list.Get() };
                ctx->queue->ExecuteCommandLists(1, lists);
                ctx->fence->GPUSync(nfv, ctx->queue);
                recorder->fence_value = nfv;
                for (auto& process : task) {
                    process.sync();
                }
            } else {
                ReleaseMutex(ctx->mutex);
                WaitForSingleObject(ctx->event, INFINITE);
            }
        }
        return 0;
    }
public:
    RecordDispatcher(ComPtr<ID3D12Device> device, uint64_t thread_count, uint64_t lists_per_thread) : render_fence_(device) {
        D3D12_COMMAND_QUEUE_DESC desc {
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Priority = 0,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0
        };
        device->CreateCommandQueue(&desc, IID_PPV_ARGS(&render_queue_));
        for (int i = 0; i < thread_count; ++i) {
            auto* ctx = new WorkerContext;
            ctx->fence = &render_fence_;
            ctx->queue = render_queue_;
            ctx->recorders.resize(lists_per_thread);
            ctx->mutex = CreateMutexW(nullptr, false, std::format(L"RenderDispatcher #{} Mutex", i).c_str());
            ctx->event = CreateEventW(nullptr, false, false, std::format(L"RenderDispatcher #{} Event", i).c_str());
            ctx->flag = true;
            for (int j = 0; j < lists_per_thread; ++j) {
                device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&ctx->recorders[j].alloc));
                device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, ctx->recorders[j].alloc.Get(), nullptr, IID_PPV_ARGS(&ctx->recorders[j].list));
                ctx->recorders[j].list->Close();
                ctx->recorders[j].fence_value = 0;
            }
            contexts.push_back(ctx);
            CreateThread(nullptr, 0, WorkerFunc, ctx, 0, nullptr);
        }
    }

    void PostRecordTask(std::vector<GPUProcess>&& task) {
        WorkerContext* select_ctx = contexts[0];
        uint64_t min_proc_size = contexts[0]->tasks.size();
        for (auto& ctx : contexts) {
            if (ctx->tasks.size() < min_proc_size) {
                min_proc_size = ctx->tasks.size();
                select_ctx = ctx;
            }
        }
        WaitForSingleObject(select_ctx->mutex, INFINITE);
        select_ctx->tasks.push_back(std::move(task));
        ReleaseMutex(select_ctx->mutex);
        SetEvent(select_ctx->event);
    }

    ComPtr<ID3D12CommandQueue> GetCommandQueue() {
        return render_queue_;
    }

    Fence& GetFence() {
        return render_fence_;
    }

    ~RecordDispatcher() {
        for (auto& ctx : contexts) {
            delete ctx;
        }
    }
};
}