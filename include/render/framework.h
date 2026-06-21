#pragma once

#include <base.h>

#include <render/drawcall.h>
#include <render/queue.h>
#include <render/resource.h>
#include <io/shader.h>
#include <transform/camera.h>

using dx_init_t = struct {
    uint32_t width;
    uint32_t height;
    HWND hwnd;
    uint32_t buffer_count;
    uint32_t copy_workers_count;
    MSAAType msaa_type;
    DXGI_FORMAT rt_format;
    DXGI_FORMAT ds_format;
    float rt_clear_color[4];
    bool enable_vsync;
    std::string shaders_dir;
    std::string textures_dir;
    std::string assets_dir;
    uint64_t cbv_count;
    uint64_t srv_count;
    uint64_t uav_count;
};

class Device {
private:
    ComPtr<IDXGIFactory7> factory_;
    ComPtr<IDXGIAdapter> adapter_;
    ComPtr<ID3D12Device> device_;
public:
    Device();

    ComPtr<ID3D12Device>& GetComPtr() {
        return device_;
    }

    ComPtr<IDXGIFactory7>& GetFactory() {
        return factory_;
    }

    ComPtr<IDXGIAdapter>& GetSelectedAdapter() {
        return adapter_;
    }
};

struct FrameResource {
    uint32_t index;
    RenderTarget* back_buffer;
    DepthBuffer* depth_buffer;
    RenderTarget* msaa_buffer;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle;
    D3D12_CPU_DESCRIPTOR_HANDLE msaa_rtv_handle;

    FrameResource(uint32_t i, RenderTarget* bb, DepthBuffer* db, RenderTarget* mb, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, D3D12_CPU_DESCRIPTOR_HANDLE msaa_rtv) : index(i), back_buffer(bb), depth_buffer(db), msaa_buffer(mb), rtv_handle(rtv), dsv_handle(dsv), msaa_rtv_handle(msaa_rtv) {}
    FrameResource(const FrameResource&) = delete;
    FrameResource(FrameResource&& fr) noexcept : index(fr.index), back_buffer(fr.back_buffer), depth_buffer(fr.depth_buffer), msaa_buffer(fr.msaa_buffer), rtv_handle(fr.rtv_handle), dsv_handle(fr.dsv_handle), msaa_rtv_handle(fr.msaa_rtv_handle) {
        fr.index = UINT32_MAX;
        fr.back_buffer = nullptr;
        fr.depth_buffer = nullptr;
        fr.msaa_buffer = nullptr;
        fr.rtv_handle = {};
        fr.dsv_handle = {};
        fr.msaa_rtv_handle = {};
    }

    static void InitializeFrameResources(const dx_init_t& init, std::vector<FrameResource>& frs,
        ComPtr<IDXGISwapChain4>& swapchain, ResourceManager& res_mgr, ComPtr<ID3D12Device>& device,
        RTVHeap& rtv_heap, DSVHeap& dsv_heap, RTVHeap& msaa_heap) {
        RenderTarget* msaa_rt = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE msaa_rtv {};
        if (init.msaa_type != MSAAType::NONE) {
            std::string msaa_buffer_name = "MSAABuffer";
            auto msaa_buffer = res_mgr.CreateRenderTarget(msaa_buffer_name, init.rt_format, init.width, init.height, init.rt_clear_color, init.msaa_type);
            if (!msaa_buffer.has_value()) {
                LFATAL("Cannot create {}", msaa_buffer_name);
            }
            msaa_rt = msaa_buffer.value();
            D3D12_RENDER_TARGET_VIEW_DESC mrtv_desc {};
            mrtv_desc.Format = init.rt_format;
            mrtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            device->CreateRenderTargetView(msaa_buffer.value()->GetComPtr().Get(), &mrtv_desc, msaa_heap.GetCPUHandle(0));
            ResourceView mrtv {};
            mrtv.type = ResourceViewType::RTV;
            mrtv.data.handle = msaa_heap.GetCPUHandle(0);
            res_mgr.GetMap().BindResourceView(msaa_buffer_name, "rtv", mrtv);
            msaa_rtv = mrtv.data.handle;
        }
        for (int i = 0; i < init.buffer_count; ++i) {
            std::string back_buffer_name = std::format("BackBuffer #{}", i);
            std::string depth_buffer_name = std::format("DepthBuffer #{}", i);
            ComPtr<ID3D12Resource> back_buffer_resource;
            swapchain->GetBuffer(i, IID_PPV_ARGS(&back_buffer_resource));
            auto back_buffer = res_mgr.CreateRenderTarget(back_buffer_name, back_buffer_resource);
            auto depth_buffer = res_mgr.CreateDepthBuffer(depth_buffer_name, init.width, init.height, init.ds_format, init.msaa_type);
            device->CreateRenderTargetView(back_buffer.value()->GetComPtr().Get(), nullptr, rtv_heap.GetCPUHandle(i));
            ResourceView rtv {};
            rtv.type = ResourceViewType::RTV;
            rtv.data.handle = rtv_heap.GetCPUHandle(i);
            res_mgr.GetMap().BindResourceView(back_buffer_name, "rtv", rtv);
            device->CreateDepthStencilView(depth_buffer.value()->GetComPtr().Get(), nullptr, dsv_heap.GetCPUHandle(i));
            ResourceView dsv {};
            dsv.type = ResourceViewType::DSV;
            dsv.data.handle = dsv_heap.GetCPUHandle(i);
            res_mgr.GetMap().BindResourceView(depth_buffer_name, "dsv", dsv);
            frs.emplace_back(i, back_buffer.value(), depth_buffer.value(), msaa_rt, rtv.data.handle, dsv.data.handle, msaa_rtv);
        }
    }
};

class RenderContext {
    template<typename Allocator>
    requires std::derived_from<Allocator, DXAllocator>
    friend class DXFramework;
public:
private:
    Device& device_;
    const dx_init_t& init_;
    std::vector<FrameResource> frame_resources_;
    ComPtr<IDXGISwapChain4> swapchain_;
    RenderQueue& render_queue_;
    ResourceManager& res_mgr_;
    BindlessHeap& bindless_heap_;
    DSVHeap dsv_heap_;
    RTVHeap rtv_heap_;
    RTVHeap msaa_heap_;
    ComPtr<ID3D12GraphicsCommandList> record_list_;
public:
    RenderContext(Device& device, RenderQueue& rq, ResourceManager& rm, BindlessHeap& heap, const dx_init_t& init);

    void Render(std::function<void(RenderPass&)>&& callback);

    ComPtr<ID3D12Device>& GetDevice() {
        return device_.GetComPtr();
    }

    FrameResource& GetCurrentFrameResource() {
        return frame_resources_[swapchain_->GetCurrentBackBufferIndex()];
    }
};

template<typename Allocator>
requires std::derived_from<Allocator, DXAllocator>
class DXFramework {
private:
    const dx_init_t& init_;
    Device device_;
    Allocator allocator_;
    RenderQueue render_queue_;
    CopyQueue copy_queue_;
    ResourceManager res_mgr_;
    BindlessHeap heap_;
    RenderContext ctx_;
    ShaderLoader shader_loader_;
    TextureLoader texture_loader_;
public:
    template<typename... Args>
    DXFramework(const dx_init_t& init, Args&&... args) : init_(init), allocator_(device_.GetComPtr(), std::forward<Args>(args)...), copy_queue_(device_.GetComPtr(), init_.copy_workers_count), render_queue_(device_.GetComPtr(), init_.buffer_count), res_mgr_(device_.GetComPtr(), &allocator_, copy_queue_, render_queue_), heap_(device_.GetComPtr(), res_mgr_, init.srv_count, init.cbv_count, init.uav_count), ctx_(device_, render_queue_, res_mgr_, heap_, init_), shader_loader_(init.shaders_dir), texture_loader_(init.textures_dir) {
        FrameResource::InitializeFrameResources(init_, ctx_.frame_resources_, ctx_.swapchain_, res_mgr_, device_.GetComPtr(), ctx_.rtv_heap_, ctx_.dsv_heap_, ctx_.msaa_heap_);
    }

    const dx_init_t& GetInitializeParams() const {
        return init_;
    }

    Device& GetDevice() {
        return device_;
    }

    DXAllocator* GetAllocator() {
        return &allocator_;
    }

    BindlessHeap& GetBindlessHeap() {
        return heap_;
    }

    RenderContext& GetRenderContext() {
        return ctx_;
    }

    ResourceManager& GetResourceManager() {
        return res_mgr_;
    }

    CopyQueue& GetCopyQueue() {
        return copy_queue_;
    }

    RenderQueue& GetRenderQueue() {
        return render_queue_;
    }

    ShaderLoader& GetShaderLoader() {
        return shader_loader_;
    }

    TextureLoader& GetTextureLoader() {
        return texture_loader_;
    }

    class ObjectDrawcall;
};



template<typename Allocator>
requires std::derived_from<Allocator, DXAllocator>
class DXFramework<Allocator>::ObjectDrawcall : Drawcall {
public:
    struct Vertex {
        struct {
            float X;
            float Y;
            float Z;
        } Position;
        struct {
            float U;
            float V;
        } Tex;
        struct {
            float X;
            float Y;
            float Z;
        } Normal;
    };

    using Index = uint32_t;

    struct ObjectPresets {
        XMFLOAT4X4 world;
        uint32_t texture_index;
        uint32_t scene_index;
    };

    struct Scene {
        XMFLOAT4X4 vp;
        XMFLOAT4 camera_position;
        uint32_t dotlight_count;
        float _padding0[3];
        XMFLOAT4 dotlight_positions[16];
        XMFLOAT4 dotlight_colors[16];
    };

private:
    std::string object_name_;
    DXFramework* dxfw_;
    object_params_t op_ {};
    ConstantBuffer* presets_;
    VertexBuffer* vb_;
    IndexBuffer* ib_;
    std::string presets_name_;
    std::string vertex_buffer_name_;
    std::string index_buffer_name_;
    uint32_t tex_index_;
    uint32_t scene_index_;
public:
    ObjectDrawcall(const std::string& object_name, const std::vector<Vertex>& vertices, const std::vector<Index>& indices, DXFramework* dxfw, uint32_t tex_index, uint32_t scene_index)
    : Drawcall(dxfw->GetDevice().GetComPtr(), dxfw->GetRenderQueue(), dxfw->GetBindlessHeap(), dxfw->GetResourceManager()), object_name_(object_name), dxfw_(dxfw), tex_index_(tex_index), scene_index_(scene_index) {
        auto vs = dxfw->GetShaderLoader().CompileShader("object", ShaderType::VertexShader);
        auto ps = dxfw->GetShaderLoader().CompileShader("object", ShaderType::PixelShader);
        const D3D12_INPUT_ELEMENT_DESC iv_layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };
        pso_desc_.InputLayout = { iv_layout, CountOf(iv_layout) };
        pso_desc_.VS = {vs.blob->GetBufferPointer(), vs.blob->GetBufferSize()};
        pso_desc_.PS = {ps.blob->GetBufferPointer(), ps.blob->GetBufferSize()};
        pso_desc_.SampleDesc.Count = GetSampleCount(dxfw_->GetInitializeParams().msaa_type);
        pso_desc_.SampleDesc.Quality = 0;
        pso_desc_.NumRenderTargets = 1;
        pso_desc_.RTVFormats[0] = dxfw_->GetInitializeParams().rt_format;
        pso_desc_.DSVFormat = dxfw_->GetInitializeParams().ds_format;
        pso_desc_.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        pso_desc_.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc_.SampleMask = UINT_MAX;
        samplers_.Add(StaticSamplers::LINEAR_FILTER(0));
        Drawcall::BuildPipeline();

        presets_name_ = std::format("{}_presets", object_name_);
        vertex_buffer_name_ = std::format("{}_vertex_buffer", object_name_);
        index_buffer_name_ = std::format("{}_index_buffer", object_name_);
        auto presets_create = dxfw_->GetResourceManager().CreateConstantBuffer<ObjectPresets>(presets_name_);
        auto vertex_buffer_create = dxfw_->GetResourceManager().CreateVertexBuffer(vertex_buffer_name_, &vertices[0], vertices.size());
        auto index_buffer_create = dxfw_->GetResourceManager().CreateIndexBuffer(index_buffer_name_, &indices[0], indices.size());
        if (!presets_create.has_value()) {
            LFATAL("Cannot create Object Presets constant buffer while make object drawcall {}", object_name_);
        }
        if (!vertex_buffer_create.has_value()) {
            LFATAL("Cannot create vertex buffer while make object drawcall {}", object_name_);
        }
        if (!index_buffer_create.has_value()) {
            LFATAL("Cannot create index buffer while make object drawcall {}", object_name_);
        }
        vb_ = vertex_buffer_create.value();
        ib_ = index_buffer_create.value();
        presets_ = presets_create.value();
        presets_->GetMapping<ObjectPresets>()->texture_index = tex_index_;
        presets_->GetMapping<ObjectPresets>()->scene_index = scene_index_;
    }

    ObjectPresets* GetObjectPresets() {
        return presets_->GetMapping<ObjectPresets>();
    }

    void operator()(RenderPass& rp, float x, float y, float z, float size) {
        op_.position = { x, y, z };
        op_.size = { size, size, size };
        MakeWorld(op_, presets_->GetMapping<ObjectPresets>()->world);
        Draw(rp, presets_, vb_, ib_);
    }
};