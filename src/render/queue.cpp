#include <render/queue.h>
#include "render/resource.h"

RenderQueue::RenderQueue(ComPtr<ID3D12Device>& device, uint32_t n) : device_(device), fence_(device) {
    D3D12_COMMAND_QUEUE_DESC desc {};
    desc.NodeMask = 0;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Priority = 0;
    device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue_));
    queue_->SetName(L"Prism Render Queue");
    for (uint64_t i = 0; i < n; ++i) {
        ComPtr<ID3D12GraphicsCommandList> list;
        ComPtr<ID3D12CommandAllocator> alloc;
        device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
        device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&list));
        list->Close();
        workers_.emplace_back(list, alloc, Waitable(fence_, 0), false);
    }

}

void CopyQueue::DeferredRelease(bool final_release) {
    uint64_t complete = fence_.GetCompleteValue();
    for (auto it = upload_buffers_.begin(); it != upload_buffers_.end();) {
        if (final_release) {
            Waitable(fence_, it->GetFenceValue()).CPUWait();
            it = upload_buffers_.erase(it);
            continue;
        }
        if (complete >= it->GetFenceValue()) {
            it = upload_buffers_.erase(it);
        } else {
            ++it;
        }
    }
}

ComPtr<ID3D12GraphicsCommandList>& RenderQueue::PrepareRenderQueue(uint32_t index) {
    auto& worker = workers_[index];
    worker.waitable.CPUWait();
    worker.alloc->Reset();
    worker.list->Reset(worker.alloc.Get(), nullptr);
    return worker.list;
}

uint64_t RenderQueue::CommitRenderQueue(uint32_t index) {
    auto& worker = workers_[index];
    worker.list->Close();
    ID3D12CommandList* lists[] = { worker.list.Get() };
    queue_->ExecuteCommandLists(1, lists);
    worker.waitable = fence_.AllocateWaitable();
    fence_.GPUSync(worker.waitable, queue_);
    return worker.waitable.GetFenceValue();
}

RenderQueue::~RenderQueue() {
    for (auto& worker : workers_) {
        worker.waitable.CPUWait();
    }
}

CopyQueue::queue_worker_t& CopyQueue::AssignWorker() {
    uint64_t assigned_i = 0;
    for (uint64_t i = 0; i < workers_.size(); ++i) {
        if (workers_[i].waitable.Completed()) assigned_i = i;
    }
    return workers_[assigned_i];
}

CopyQueue::CopyQueue(ComPtr<ID3D12Device>& device, uint64_t n) : device_(device), fence_(device) {
    D3D12_COMMAND_QUEUE_DESC desc {};
    desc.NodeMask = 0;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Priority = 0;
    device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue_));
    queue_->SetName(L"Prism Copy Queue");
    for (uint64_t i = 0; i < n; ++i) {
        ComPtr<ID3D12GraphicsCommandList> list;
        ComPtr<ID3D12CommandAllocator> alloc;
        device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
        device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&list));
        list->Close();
        workers_.emplace_back(list, alloc, Waitable(fence_, 0), false);
    }
}

void CopyQueue::CopyTexture(Resource<ResourceType::Texture>& dest, UploadBuffer& src, D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint) {
    D3D12_TEXTURE_COPY_LOCATION src_loc {};
    src_loc.pResource = src.GetComPtr().Get();
    src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src_loc.PlacedFootprint = footprint;
    D3D12_TEXTURE_COPY_LOCATION dest_loc {};
    dest_loc.pResource = dest.GetComPtr().Get();
    dest_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dest_loc.SubresourceIndex = 0;
    auto& worker = AssignWorker();
    worker.waitable.CPUWait();
    worker.alloc->Reset();
    worker.list->Reset(worker.alloc.Get(), nullptr);
    worker.list->CopyTextureRegion(&dest_loc, 0, 0, 0, &src_loc, nullptr);
    worker.list->Close();
    ID3D12CommandList* lists[] = { worker.list.Get() };
    dest.GetWaitable().GPUWait(queue_);
    queue_->ExecuteCommandLists(1, lists);
    worker.waitable = fence_.AllocateWaitable();
    dest.GetWaitable() = Waitable(fence_, worker.waitable.GetFenceValue());
    fence_.GPUSync(worker.waitable, queue_);
    upload_buffers_.push_back(std::move(src));
}

CopyQueue::~CopyQueue() {
    DeferredRelease(true);
}