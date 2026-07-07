#pragma once

#include <base.h>
#include <render/resource.h>

namespace Prism
{
    // class RenderTarget {
    //     const dx_init_t& init_;
    //     ResourceHandle back_buffer_;
    //     ResourceHandle depth_buffer_;
    //     ResourceHandle msaa_buffer_;
    //     ResourceHandle shadow_map_;
    //     D3D12_CPU_DESCRIPTOR_HANDLE rtv_;
    //     D3D12_CPU_DESCRIPTOR_HANDLE dsv_;
    //     D3D12_CPU_DESCRIPTOR_HANDLE mrtv_;
    //     D3D12_CPU_DESCRIPTOR_HANDLE shadow_dsv_;
    // public:
    //     RenderTarget(const dx_init_t& init, ResourceHandle back_buffer, ResourceHandle depth_buffer, ResourceHandle msaa_buffer, ResourceHandle shadow_map, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, D3D12_CPU_DESCRIPTOR_HANDLE mrtv, D3D12_CPU_DESCRIPTOR_HANDLE shadow_dsv) : init_(init), back_buffer_(back_buffer), depth_buffer_(depth_buffer), msaa_buffer_(msaa_buffer), shadow_map_(shadow_map), rtv_(rtv), dsv_(dsv), mrtv_(mrtv), shadow_dsv_(shadow_dsv) {}
    //
    //     RenderTarget(const RenderTarget&) = delete;
    //     RenderTarget(RenderTarget&& rt) noexcept : init_(rt.init_), back_buffer_(std::move(rt.back_buffer_)), depth_buffer_(std::move(rt.depth_buffer_)), msaa_buffer_(std::move(rt.msaa_buffer_)), shadow_map_(std::move(rt.shadow_map_)),
    //     rtv_(rt.rtv_), dsv_(rt.dsv_), mrtv_(rt.mrtv_), shadow_dsv_(rt.shadow_dsv_) {}
    //
    //     RecordDispatcher::GPUProcess BeginShadow() {
    //         return {
    //             [&](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
    //                 shadow_map_->Transition(D3D12_RESOURCE_STATE_DEPTH_WRITE, list);
    //                 D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)init_.width, (float)init_.height, 0.0f, 1.0f };
    //                 D3D12_RECT scissor_rect = { 0, 0, static_cast<LONG>(init_.width), static_cast<LONG>(init_.height) };
    //                 list->RSSetViewports(1, &viewport);
    //                 list->RSSetScissorRects(1, &scissor_rect);
    //                 list->ClearDepthStencilView(shadow_dsv_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    //                 list->OMSetRenderTargets(0, nullptr, false, &shadow_dsv_);
    //                 shadow_map_->GetRenderWaitable().GPUWait();
    //                 shadow_map_->GetRenderWaitable().GetFenceValue() = nfv;
    //                 return nullptr;
    //             }
    //         };
    //     }
    //
    //     RecordDispatcher::GPUProcess FinishShadow() {
    //         return {
    //             [&](ComPtr<ID3D12GraphicsCommandList> list, uint64_t nfv) {
    //                 shadow_map_->Transition(D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, list);
    //                 return nullptr;
    //             }
    //         };
    //     }
    //
    //     ResourceHandle GetShadowMap() {
    //         return shadow_map_;
    //     }
    //
    //

    //};

}