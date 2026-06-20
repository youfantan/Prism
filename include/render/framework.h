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
            auto msaa_buffer = res_mgr.CreateRenderTarget(msaa_buffer_name, DXGI_FORMAT_R8G8B8A8_UNORM, init.width, init.height, init.rt_clear_color, init.msaa_type);
            if (!msaa_buffer.has_value()) {
                LFATAL("Cannot create {}", msaa_buffer_name);
            }
            msaa_rt = msaa_buffer.value();
            D3D12_RENDER_TARGET_VIEW_DESC mrtv_desc {};
            mrtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
            auto depth_buffer = res_mgr.CreateDepthBuffer(depth_buffer_name, init.width, init.height, init.msaa_type);
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
    struct record_t {
        Drawcall* drawcall;
        std::vector<Resource*> resources;
    };

    using render_callback_t = std::function<record_t(ComPtr<ID3D12GraphicsCommandList>)>;
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
    std::vector<record_t> records_;
public:
    RenderContext(Device& device, RenderQueue& rq, ResourceManager& rm, BindlessHeap& heap, const dx_init_t& init);

    void RecordRenderList(render_callback_t&& rc);
    void Render(std::function<void()>&& callback);

    ComPtr<ID3D12Device>& GetDevice() {
        return device_.GetComPtr();
    }

    FrameResource& GetCurrentFrameResource() {
        return frame_resources_[swapchain_->GetCurrentBackBufferIndex()];
    }
};

class ObjectDrawcall;

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
class DXFramework<Allocator>::ObjectDrawcall {
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
    std::vector<Vertex> vertices_;
    std::vector<Index> indices_;
    DXFramework* dxfw_;
    object_params_t op_ {};
    Lazy<Drawcall> drawcall_;
    ConstantBuffer* presets_;
    std::string presets_name_;
    std::string vertex_buffer_name_;
    std::string index_buffer_name_;
public:
    ObjectDrawcall(const std::string& object_name, std::vector<Vertex>&& vertices, std::vector<Index>&& indices, DXFramework* dxfw, const std::string& tex_name)
    : object_name_(object_name), vertices_(std::move(vertices)), indices_(std::move(indices)), dxfw_(dxfw) {
        auto vs = dxfw->GetShaderLoader().CompileShader("object", ShaderType::VertexShader);
        auto ps = dxfw->GetShaderLoader().CompileShader("object", ShaderType::PixelShader);
        const D3D12_INPUT_ELEMENT_DESC iv_layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };
        StaticSamplers ssamplers;
        ssamplers.Add(StaticSamplers::LINEAR_FILTER(0));
        DrawcallResource drawres = {
            .vs_bytecode = vs.blob,
            .ps_bytecode = ps.blob,
            .rasterizer_desc = DefaultRasterizerDesc,
            .blend_desc = DefaultBlendDesc,
            .ds_desc = DefaultDepthStencilDesc,
            .sample_desc = {GetSampleCount(dxfw_->GetInitializeParams().msaa_type), 0},
            .iv_layout = {iv_layout, 3},
            .samplers = ssamplers
        };
        presets_name_ = std::format("{}_presets", object_name_);
        vertex_buffer_name_ = std::format("{}_vertex_buffer", object_name_);
        index_buffer_name_ = std::format("{}_index_buffer", object_name_);
        drawcall_.Construct(dxfw_->GetRenderContext().GetDevice(), dxfw_->GetRenderContext(), dxfw_->GetBindlessHeap(), drawres);
        auto presets_create = dxfw_->GetResourceManager().CreateConstantBuffer<ObjectPresets>(presets_name_);
        auto vertex_buffer_create = dxfw_->GetResourceManager().CreateVertexBuffer(vertex_buffer_name_, &vertices_[0], vertices_.size());
        auto index_buffer_create = dxfw_->GetResourceManager().CreateIndexBuffer(index_buffer_name_, &indices[0], indices_.size());
        if (!presets_create.has_value()) {
            LFATAL("Cannot create Object Presets constant buffer while make object drawcall {}", object_name_);
        }
        if (!vertex_buffer_create.has_value()) {
            LFATAL("Cannot create vertex buffer while make object drawcall {}", object_name_);
        }
        if (!index_buffer_create.has_value()) {
            LFATAL("Cannot create index buffer while make object drawcall {}", object_name_);
        }
        presets_ = presets_create.value();
        presets_->GetMapping<ObjectPresets>()->texture_index = dxfw_->GetBindlessHeap().QueryResourceIndex(tex_name);
    }

    ObjectDrawcall(const std::string& object_name, Vertex* vertices, size_t vertices_count, Index* indices, size_t indices_count, DXFramework* dxfw, const std::string& tex_name)
    : object_name_(object_name), vertices_(vertices_count), indices_(indices_count), dxfw_(dxfw) {
        memcpy(&vertices_[0], vertices, sizeof(Vertex) * vertices_count);
        memcpy(&indices_[0], indices, sizeof(Index) * indices_count);
        auto vs = dxfw->GetShaderLoader().CompileShader("object", ShaderType::VertexShader);
        auto ps = dxfw->GetShaderLoader().CompileShader("object", ShaderType::PixelShader);
        const D3D12_INPUT_ELEMENT_DESC iv_layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };
        StaticSamplers ssamplers;
        ssamplers.Add(StaticSamplers::LINEAR_FILTER(0));
        DrawcallResource drawres = {
            .vs_bytecode = vs.blob,
            .ps_bytecode = ps.blob,
            .rasterizer_desc = DefaultRasterizerDesc,
            .blend_desc = DefaultBlendDesc,
            .ds_desc = DefaultDepthStencilDesc,
            .sample_desc = {GetSampleCount(dxfw_->GetInitializeParams().msaa_type), 0},
            .iv_layout = {iv_layout, 3},
            .samplers = ssamplers
        };
        presets_name_ = std::format("{}_presets", object_name_);
        vertex_buffer_name_ = std::format("{}_vertex_buffer", object_name_);
        index_buffer_name_ = std::format("{}_index_buffer", object_name_);
        drawcall_.Construct(dxfw_->GetRenderContext().GetDevice(), dxfw_->GetRenderQueue(), dxfw_->GetBindlessHeap(), dxfw_->GetResourceManager(), drawres);
        auto presets_create = dxfw_->GetResourceManager().CreateConstantBuffer<ObjectPresets>(std::format("{}_presets", object_name_));
        auto vertex_buffer_create = dxfw_->GetResourceManager().CreateVertexBuffer(std::format("{}_vertex_buffer", object_name_), &vertices_[0], vertices_.size());
        auto index_buffer_create = dxfw_->GetResourceManager().CreateIndexBuffer(std::format("{}_index_buffer", object_name_), &indices[0], indices_.size());
        if (!presets_create.has_value()) {
            LFATAL("Cannot create Object Presets constant buffer while make object drawcall {}", object_name_);
        }
        if (!vertex_buffer_create.has_value()) {
            LFATAL("Cannot create vertex buffer while make object drawcall {}", object_name_);
        }
        if (!index_buffer_create.has_value()) {
            LFATAL("Cannot create index buffer while make object drawcall {}", object_name_);
        }
        presets_ = presets_create.value();
        presets_->GetMapping<ObjectPresets>()->texture_index = dxfw_->GetBindlessHeap().QueryResourceIndex(tex_name);
        auto* presets = GetObjectPresets();
    }
    ObjectPresets* GetObjectPresets() {
        return presets_->GetMapping<ObjectPresets>();
    }

    void operator()(float x, float y, float z, float size) {
        op_.position = { x, y, z };
        op_.size = { size, size, size };
        MakeWorld(op_, presets_->GetMapping<ObjectPresets>()->world);
        drawcall_.Get()(dxfw_->GetRenderContext(), presets_name_, vertex_buffer_name_, index_buffer_name_);
    }

    ~ObjectDrawcall() {

    }
};