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
    std::string_view shaders_dir;
    std::string_view textures_dir;
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
    Resource<ResourceType::RenderTarget> back_buffer;
    Resource<ResourceType::DepthBuffer> depth_buffer;
    Resource<ResourceType::RenderTarget> msaa_buffer;

    FrameResource(uint32_t i, Resource<ResourceType::RenderTarget>& bb, Resource<ResourceType::DepthBuffer>& db, Resource<ResourceType::RenderTarget>& mb) : index(i), back_buffer(std::move(bb)), depth_buffer(std::move(db)), msaa_buffer(std::move(mb)) {}
    FrameResource(const FrameResource&) = delete;
    FrameResource(FrameResource&& fr) noexcept : index(fr.index), back_buffer(std::move(fr.back_buffer)), depth_buffer(std::move(fr.depth_buffer)), msaa_buffer(std::move(fr.msaa_buffer)) {
        fr.index = UINT32_MAX;
    }

    template<typename Allocator>
    static void InitializeFrameResources(const dx_init_t& init, std::vector<FrameResource>& frs,
        ComPtr<IDXGISwapChain4>& swapchain, ResourceManager<Allocator>& res_mgr, ComPtr<ID3D12Device>& device,
        RTVHeap& rtv_heap, DSVHeap& dsv_heap, RTVHeap& msaa_heap) {
        for (int i = 0; i < init.buffer_count; ++i) {
            ComPtr<ID3D12Resource> back_buffer_resource;
            swapchain->GetBuffer(i, IID_PPV_ARGS(&back_buffer_resource));
            auto back_buffer = res_mgr.CreateFromExistsPtr<ResourceType::RenderTarget>(back_buffer_resource, rtv_heap.GetCPUHandle(i), D3D12_RESOURCE_STATE_PRESENT);
            auto depth_buffer = res_mgr.CreateDepthBuffer(init.width, init.height, init.msaa_type, dsv_heap.GetCPUHandle(i));
            device->CreateRenderTargetView(back_buffer.GetComPtr().Get(), nullptr, rtv_heap.GetCPUHandle(i));
            device->CreateDepthStencilView(depth_buffer.GetComPtr().Get(), nullptr, dsv_heap.GetCPUHandle(i));
            if (init.msaa_type != MSAAType::NONE) {
                auto msaa_buffer = res_mgr.CreateRenderTarget(DXGI_FORMAT_R8G8B8A8_UNORM, init.width, init.height, init.msaa_type, msaa_heap.GetCPUHandle(i));
                D3D12_RENDER_TARGET_VIEW_DESC mrtv_desc {};
                mrtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                mrtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
                device->CreateRenderTargetView(msaa_buffer.GetComPtr().Get(), &mrtv_desc, msaa_heap.GetCPUHandle(i));
            } else {
                auto msaa_buffer = res_mgr.CreateFromExistsPtr<ResourceType::RenderTarget>(nullptr);
                frs.emplace_back(i, back_buffer, depth_buffer, msaa_buffer);
            }
        }
    }
};

class RenderContext {
    template<typename Allocator>
    requires is_allocator<Allocator>
    friend class DXFramework;
public:
    using render_callback_t = std::function<void()>;
    using resource_draw_record_t = std::function<void(ComPtr<ID3D12GraphicsCommandList>&)>;
    using resource_sync_record_t = std::function<void(Fence&, uint64_t)>;
private:
    Device& device_;
    const dx_init_t& init_;
    std::vector<FrameResource> frame_resources_;
    ComPtr<IDXGISwapChain4> swapchain_;
    RenderQueue& render_queue_;
    DSVHeap dsv_heap_;
    RTVHeap rtv_heap_;
    RTVHeap msaa_heap_;

    struct record_t {
        Drawcall* drawcall;
        resource_draw_record_t draw;
        resource_sync_record_t sync;
    };
    std::vector<record_t> records_;
public:
    RenderContext(Device& device, RenderQueue& rq, const dx_init_t& init);
    void Render(render_callback_t&& rcb);

    void AddRenderRecord(Drawcall* drawcall, resource_draw_record_t&& draw, resource_sync_record_t&& sync) {
        records_.emplace_back(drawcall, std::move(draw), std::move(sync));
    }

    ComPtr<ID3D12Device>& GetDevice() {
        return device_.GetComPtr();
    }

    FrameResource& GetCurrentFrameResource() {
        return frame_resources_[swapchain_->GetCurrentBackBufferIndex()];
    }
};

template<typename Allocator>
requires is_allocator<Allocator>
class ObjectDrawcall;

template<typename Allocator = DXDefaultAllocator>
requires is_allocator<Allocator>
class DXFramework {
private:
    const dx_init_t& init_;
    Device device_;
    Fence fence_;
    RenderQueue render_queue_;
    CopyQueue copy_queue_;
    Allocator allocator_;
    ResourceManager<Allocator> res_mgr_;
    RenderContext ctx_;
    BindlessHeap heap_;
    ShaderLoader shader_loader_;
    TextureLoader texture_loader_;
public:
    DXFramework(const dx_init_t& init, const Allocator::init_t& allocator_init = {}) : init_(init), fence_(device_.GetComPtr()), copy_queue_(device_.GetComPtr(), init_.copy_workers_count), render_queue_(device_.GetComPtr(), init_.buffer_count), allocator_(device_.GetComPtr(), allocator_init), res_mgr_(device_.GetComPtr(), allocator_, copy_queue_), ctx_(device_, render_queue_, init_), heap_(device_.GetComPtr(), init.srv_count, init.cbv_count, init.uav_count), shader_loader_(init.shaders_dir), texture_loader_(init.textures_dir) {
        FrameResource::InitializeFrameResources<Allocator>(init_, ctx_.frame_resources_, ctx_.swapchain_, res_mgr_, device_.GetComPtr(), ctx_.rtv_heap_, ctx_.dsv_heap_, ctx_.msaa_heap_);
    }

    const dx_init_t& GetInitializeParams() const {
        return init_;
    }

    BindlessHeap& GetBindlessHeap() {
        return heap_;
    }

    RenderContext& GetRenderContext() {
        return ctx_;
    }

    ResourceManager<Allocator>& GetResourceManager() {
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

    using ObjectDrawcall = ObjectDrawcall<Allocator>;
};

template<typename Allocator>
requires is_allocator<Allocator>
class ObjectDrawcall {
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
    std::vector<Vertex> vertices_;
    std::vector<Index> indices_;
    DXFramework<Allocator>& dxfw_;
    object_params_t op_ {};
    Lazy<Drawcall> drawcall_;
    Lazy<Resource<ResourceType::ConstBuffer>> presets_;
    Lazy<Resource<ResourceType::VertexBuffer>> vertex_buffer_;
    Lazy<Resource<ResourceType::IndexBuffer>> index_buffer_;
public:
    ObjectDrawcall(std::vector<Vertex>&& vertices, std::vector<Index>&& indices, DXFramework<Allocator>& dxfw, std::string_view tex_name)
    : vertices_(std::move(vertices)), indices_(std::move(indices)), dxfw_(dxfw) {
        auto vs = dxfw.GetShaderLoader().LoadShaderIntoMemory("object", ShaderType::VertexShader);
        auto ps = dxfw.GetShaderLoader().LoadShaderIntoMemory("object", ShaderType::PixelShader);
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
            .sample_desc = {1, 0},
            .iv_layout = {iv_layout, 3},
            .samplers = ssamplers
        };
        drawcall_.Construct(dxfw_.GetRenderContext().GetDevice(), dxfw_.GetRenderContext(), dxfw_.GetBindlessHeap(), drawres);
        presets_.Construct(dxfw_.GetResourceManager().CreateConstantBuffer<ObjectPresets>());
        presets_.Get().GetMapping<ObjectPresets>()->texture_index = dxfw_.GetBindlessHeap().QueryResourceIndex(tex_name);
        vertex_buffer_.Construct(dxfw_.GetResourceManager().CreateVertexBuffer(&vertices_[0], vertices_.size()));
        index_buffer_.Construct(dxfw_.GetResourceManager().CreateIndexBuffer(&indices[0], indices_.size()));
    }

    ObjectDrawcall(Vertex* vertices, size_t vertices_count, Index* indices, size_t indices_count, DXFramework<Allocator>& dxfw, std::string_view tex_name)
    : vertices_(vertices_count), indices_(indices_count), dxfw_(dxfw) {
        memcpy(&vertices_[0], vertices, sizeof(Vertex) * vertices_count);
        memcpy(&indices_[0], indices, sizeof(Index) * indices_count);
        auto vs = dxfw.GetShaderLoader().LoadShaderIntoMemory("object", ShaderType::VertexShader);
        auto ps = dxfw.GetShaderLoader().LoadShaderIntoMemory("object", ShaderType::PixelShader);
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
            .sample_desc = {1, 0},
            .iv_layout = {iv_layout, 3},
            .samplers = ssamplers
        };
        drawcall_.Construct(dxfw_.GetRenderContext().GetDevice(), dxfw_.GetRenderQueue(), dxfw_.GetBindlessHeap(), drawres);
        presets_.Construct(dxfw_.GetResourceManager().CreateConstantBuffer<ObjectPresets>());
        presets_.Get().GetMapping<ObjectPresets>()->texture_index = dxfw_.GetBindlessHeap().QueryResourceIndex(tex_name);
        vertex_buffer_.Construct(dxfw_.GetResourceManager().CreateVertexBuffer(&vertices_[0], vertices_.size()));
        index_buffer_.Construct(dxfw_.GetResourceManager().CreateIndexBuffer(&indices[0], indices_.size()));
        vertex_buffer_.Get().GetComPtr()->SetName(L"VB");
        index_buffer_.Get().GetComPtr()->SetName(L"IB");
        presets_.Get().GetComPtr()->SetName(L"PRESETS");
    }

    ObjectPresets* GetObjectPresets() {
        return presets_;
    }

    void operator()(float x, float y, float z, float size) {
        op_.position = { x, y, z };
        op_.size = { size, size, size };
        MakeWorld(op_, presets_.Get().GetMapping<ObjectPresets>()->world);
        drawcall_.Get()(dxfw_.GetRenderContext(), presets_.Get(), vertex_buffer_.Get(), index_buffer_.Get());
    }

    ~ObjectDrawcall() {

    }
};