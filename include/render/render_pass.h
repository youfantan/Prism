#pragma once

#include <base.h>
#include <render/queue.h>
#include <render/resource.h>
#include <render/render_context.h>


namespace Prism
{
    class RenderPass {
    protected:
        RenderContext& rctx_;
    public:
        RenderPass(RenderContext& rctx) : rctx_(rctx) {}
        virtual RecordDispatcher::RecordProcess GetInitProc() = 0;
        virtual RecordDispatcher::RecordProcess GetSyncProc() = 0;
        virtual void operator()(std::initializer_list<RecordDispatcher::RecordProcess> processes) {
            std::vector procs = processes;
            rctx_.GetRenderDispatcher().PostRecordTask(std::move(procs));
        }
        virtual ~RenderPass() = default;
    };

    class BackBufferRenderPass : public RenderPass {
        struct FrameResource {
            ResourceHandle back_buffer;
            ResourceHandle depth_buffer;
            ResourceHandle msaa_buffer;
            D3D12_CPU_DESCRIPTOR_HANDLE rtv;
            D3D12_CPU_DESCRIPTOR_HANDLE dsv;
            D3D12_CPU_DESCRIPTOR_HANDLE mrtv;
        };

        uint32_t buffer_count_;
        DescriptorHeap<D3D12_DESCRIPTOR_HEAP_TYPE_RTV> rtv_heap_;
        DescriptorHeap<D3D12_DESCRIPTOR_HEAP_TYPE_RTV> msaa_rtv_heap_;
        DescriptorHeap<D3D12_DESCRIPTOR_HEAP_TYPE_DSV> dsv_heap_;
        std::vector<FrameResource> frame_resources_;

    public:
        BackBufferRenderPass(RenderContext& rctx) : RenderPass(rctx), buffer_count_(rctx.GetInitializeParams().buffer_count),
        rtv_heap_(rctx.GetDevice(), buffer_count_),
        msaa_rtv_heap_(rctx.GetDevice(), 1),
        dsv_heap_(rctx.GetDevice(), buffer_count_),
        frame_resources_(buffer_count_) {
            auto& init = rctx.GetInitializeParams();
            D3D12_CLEAR_VALUE depth_clr {
                .Format = init.ds_format,
                .DepthStencil = {.Depth = 1.0f,.Stencil = 0
                }
            };
            D3D12_CLEAR_VALUE rt_clr = {
                .Format = rctx_.GetInitializeParams().rt_format,
                .Color = { init.rt_clear_color[0], rctx_.GetInitializeParams().rt_clear_color[1], rctx_.GetInitializeParams().rt_clear_color[2], rctx_.GetInitializeParams().rt_clear_color[3] }
            };

            ResourceHandle msaa_buffer = nullptr;
            D3D12_CPU_DESCRIPTOR_HANDLE mrtv {};

            if (init.msaa_type != MSAAType::NONE) {
                msaa_buffer = rctx_.GetResourceManager().CreateTexture2D("MSAA Buffer", init.width, init.height, 1, init.rt_format, D3D12_RESOURCE_STATE_COMMON, ResourceManager::NO_TEXTURE_BIND, GetSampleDesc(init.msaa_type), D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, &rt_clr);
                rctx_.GetDevice()->CreateRenderTargetView(msaa_buffer->GetD3D12Resource(), nullptr, msaa_rtv_heap_.GetCPUHandle(0));
                mrtv = msaa_rtv_heap_.GetCPUHandle(0);
            }

            for (uint32_t i = 0; i < buffer_count_; ++i) {
                std::string back_buffer_name = std::format("BackBuffer #{}", i);
                std::string depth_buffer_name = std::format("DepthBuffer #{}", i);
                std::string shadow_map_name = std::format("ShadowMap #{}", i);
                FrameResource fr {};
                ID3D12Resource* back_buffer_resource;
                rctx_.GetSwapchain()->GetBuffer(i, IID_PPV_ARGS(&back_buffer_resource));
                fr.back_buffer = rctx_.GetResourceManager().CreateAdoptedBackBuffer(back_buffer_name, back_buffer_resource, D3D12_RESOURCE_STATE_PRESENT);
                fr.depth_buffer = rctx_.GetResourceManager().CreateTexture2D(depth_buffer_name, init.width, init.height, 1, init.ds_format, D3D12_RESOURCE_STATE_DEPTH_WRITE, ResourceManager::NO_TEXTURE_BIND, GetSampleDesc(init.msaa_type), D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, &depth_clr);
                fr.msaa_buffer = msaa_buffer;
                rctx_.GetDevice()->CreateRenderTargetView(fr.back_buffer->GetD3D12Resource(), nullptr, rtv_heap_.GetCPUHandle(i));
                rctx_.GetDevice()->CreateDepthStencilView(fr.depth_buffer->GetD3D12Resource(), nullptr, dsv_heap_.GetCPUHandle(i));
                fr.mrtv = mrtv;
                fr.rtv = rtv_heap_.GetCPUHandle(i);
                fr.dsv = dsv_heap_.GetCPUHandle(i);
                frame_resources_[i] = fr;
            }
        }

        RecordDispatcher::RecordProcess GetInitProc() override {
            return {
                [&, frame_resource = frame_resources_[rctx_.GetCurrentIndex()]](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                    auto& init = rctx_.GetInitializeParams();
                    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(init.width), static_cast<float>(init.height), 0.0f, 1.0f };
                    D3D12_RECT scissor_rect = { 0, 0, static_cast<int>(init.width), static_cast<int>(init.height) };
                    list->RSSetViewports(1, &viewport);
                    list->RSSetScissorRects(1, &scissor_rect);
                    list->ClearDepthStencilView(frame_resource.dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
                    if (init.msaa_type == MSAAType::NONE) {
                        frame_resource.back_buffer->Transition(D3D12_RESOURCE_STATE_RENDER_TARGET, list);
                        list->OMSetRenderTargets(1, &frame_resource.rtv, false, &frame_resource.dsv);
                        list->ClearRenderTargetView(frame_resource.rtv, init.rt_clear_color, 0, nullptr);
                    } else {
                        frame_resource.msaa_buffer->Transition(D3D12_RESOURCE_STATE_RENDER_TARGET, list);
                        list->OMSetRenderTargets(1, &frame_resource.mrtv, false, &frame_resource.dsv);
                        list->ClearRenderTargetView(frame_resource.mrtv, init.rt_clear_color, 0, nullptr);
                    }
                    return nullptr;
                }
            };
        }

        RecordDispatcher::RecordProcess GetSyncProc() override {
            return {
                [&, frame_resource = frame_resources_[rctx_.GetCurrentIndex()]](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                    auto& init = rctx_.GetInitializeParams();
                    if (init.msaa_type != MSAAType::NONE) {
                        frame_resource.msaa_buffer->Transition(D3D12_RESOURCE_STATE_RESOLVE_SOURCE, list);
                        frame_resource.back_buffer->Transition(D3D12_RESOURCE_STATE_RESOLVE_DEST, list);
                        list->ResolveSubresource(frame_resource.back_buffer->GetD3D12Resource(), 0, frame_resource.msaa_buffer->GetD3D12Resource(), 0, init.rt_format);
                    }
                    frame_resource.back_buffer->Transition(D3D12_RESOURCE_STATE_PRESENT, list);
                    return nullptr;
                }, [&](void*) {
                    rctx_.Present();
                }
            };
        }

        FrameResource& GetFrameResource(uint32_t index) {
            return frame_resources_[index];
        }
    };



    class ShadowRenderPass : public RenderPass {
        struct FrameResource {
            ResourceHandle shadow_map;
            D3D12_CPU_DESCRIPTOR_HANDLE sm_dsv;
        };

        std::vector<FrameResource> frame_resources_;
        DescriptorHeap<D3D12_DESCRIPTOR_HEAP_TYPE_DSV> sm_heap_;
    public:
        ShadowRenderPass(RenderContext& rctx) : RenderPass(rctx), frame_resources_(rctx.GetInitializeParams().buffer_count),
        sm_heap_(rctx.GetDevice(), rctx.GetInitializeParams().buffer_count) {
            auto& init = rctx_.GetInitializeParams();
            D3D12_CLEAR_VALUE depth_clr {
                .Format = init.ds_format,
                .DepthStencil = {.Depth = 1.0f,.Stencil = 0
                }
            };
            for (uint32_t i = 0; i < rctx_.GetInitializeParams().buffer_count; ++i) {
                std::string shadow_map_name = std::format("ShadowMap #{}", i);
                ResourceManager::TextureBindSettings sm_bind_settings = {
                    .bind_to_heap = true,
                    .bind_format = DXGI_FORMAT_R32_FLOAT
                };
                FrameResource fr {};
                fr.shadow_map = rctx_.GetResourceManager().CreateTexture2D(shadow_map_name, init.width, init.height, 1, DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_STATE_DEPTH_WRITE, sm_bind_settings, { 1, 0 }, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, &depth_clr);
                rctx_.GetDevice()->CreateDepthStencilView(fr.shadow_map->GetD3D12Resource(), nullptr, sm_heap_.GetCPUHandle(i));
                fr.sm_dsv = sm_heap_.GetCPUHandle(i);
                frame_resources_[i] = fr;
            }
        }

        RecordDispatcher::RecordProcess GetInitProc() override {
            return {
                [&, frame_resource = frame_resources_[rctx_.GetCurrentIndex()]](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                    auto& init = rctx_.GetInitializeParams();
                    frame_resource.shadow_map->Transition(D3D12_RESOURCE_STATE_DEPTH_WRITE, list);
                    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(init.width), static_cast<float>(init.height), 0.0f, 1.0f };
                    D3D12_RECT scissor_rect = { 0, 0, static_cast<int>(init.width), static_cast<int>(init.height) };
                    list->RSSetViewports(1, &viewport);
                    list->RSSetScissorRects(1, &scissor_rect);
                    list->ClearDepthStencilView(frame_resource.sm_dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
                    list->OMSetRenderTargets(0, nullptr, false, &frame_resource.sm_dsv);
                    frame_resource.shadow_map->GetRenderWaitable().GPUWait();
                    frame_resource.shadow_map->GetRenderWaitable().GetFenceValue() = nfv;
                    return nullptr;
                }
            };
        }

        RecordDispatcher::RecordProcess GetSyncProc() override {
            return {
                [&, frame_resource = frame_resources_[rctx_.GetCurrentIndex()]](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
                    frame_resource.shadow_map->Transition(D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, list);
                    return nullptr;
                }
            };
        }

        FrameResource& GetFrameResource(uint32_t index) {
            return frame_resources_[index];
        }
    };
}
