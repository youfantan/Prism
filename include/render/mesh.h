#pragma once

#include <array>
#include <render/resource.h>
#include <mlog.h>

namespace Prism
{
    template<typename T>
    concept is_input_attr = requires (T& t, std::vector<D3D12_INPUT_ELEMENT_DESC>& inputs, size_t offset)
    {
        { T::Name } -> std::same_as<const std::string_view&>;
        { T::Format } -> std::same_as<const DXGI_FORMAT&>;
        { T::DataLength } -> std::same_as<const size_t&>;
        { T::Stride } -> std::same_as<const size_t&>;
        T::MakeLayout(inputs, offset);
        typename T::DataType;
    };

    template<uint32_t SLOT = 0>
    struct PositionAttr {
        constexpr static std::string_view Name = "POSITION";
        constexpr static std::string_view DXSemantic = "POSITION";
        constexpr static DXGI_FORMAT Format = DXGI_FORMAT_R32G32B32_FLOAT;
        using DataType = float;
        constexpr static size_t DataLength = 3;
        constexpr static size_t Stride = sizeof(DataType) * DataLength;
        static void MakeLayout(std::vector<D3D12_INPUT_ELEMENT_DESC>& inputs, uint32_t offset) {
            inputs.emplace_back(DXSemantic.data(), 0, Format, SLOT, offset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0);
        }

    };

    template<uint32_t SLOT = 0>
    struct TexCoord0Attr {
        constexpr static std::string_view Name = "TEXCOORD_0";
        constexpr static std::string_view DXSemantic = "TEXCOORD";
        constexpr static DXGI_FORMAT Format = DXGI_FORMAT_R32G32_FLOAT;
        using DataType = float;
        constexpr static size_t DataLength = 2;
        constexpr static size_t Stride = sizeof(DataType) * DataLength;
        static void MakeLayout(std::vector<D3D12_INPUT_ELEMENT_DESC>& inputs, uint32_t offset) {
            inputs.emplace_back(DXSemantic.data(), 0, Format, SLOT, offset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0);
        }
    };

    template<uint32_t SLOT = 0>
    struct NormalAttr {
        constexpr static std::string_view Name = "NORMAL";
        constexpr static std::string_view DXSemantic = "NORMAL";
        constexpr static DXGI_FORMAT Format = DXGI_FORMAT_R32G32B32_FLOAT;
        using DataType = float;
        constexpr static size_t DataLength = 3;
        constexpr static size_t Stride = sizeof(DataType) * DataLength;
        static void MakeLayout(std::vector<D3D12_INPUT_ELEMENT_DESC>& inputs, uint32_t offset) {
            inputs.emplace_back(DXSemantic.data(), 0, Format, SLOT, offset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0);
        }
    };

    template<uint32_t SLOT = 0>
    struct TangentAttr {
        constexpr static std::string_view Name = "TANGENT";
        constexpr static std::string_view DXSemantic = "TANGENT";
        constexpr static DXGI_FORMAT Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        using DataType = float;
        constexpr static size_t DataLength = 4;
        constexpr static size_t Stride = sizeof(DataType) * DataLength;
        static void MakeLayout(std::vector<D3D12_INPUT_ELEMENT_DESC>& inputs, uint32_t offset) {
            inputs.emplace_back(DXSemantic.data(), 0, Format, SLOT, offset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0);
        }
    };

    /*
     * POSITION 0: Color Texture
     * POSITION 1: Normal Texture
     * POSITION 2: Roughness & Metal Texture
     * POSITION 3: EmissiveTexture Texture
     * POSITION 4: Occlusion Texture
     * POSITION 5: Displacement Texture
     */
    template<uint32_t SLOT = 1>
    struct TextureIndexAttr {
        constexpr static std::string_view Name = "TEXIDX";
        constexpr static std::string_view DXSemantic = "TEXIDX";
        constexpr static DXGI_FORMAT Format = DXGI_FORMAT_R32G32B32A32_UINT;
        static void MakeLayout(std::vector<D3D12_INPUT_ELEMENT_DESC>& inputs, uint32_t offset) {
            inputs.emplace_back(DXSemantic.data(), 0, Format, SLOT, offset, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1);
            inputs.emplace_back(DXSemantic.data(), 1, Format, SLOT, offset + 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1);
        }

        using DataType = uint32_t;
        constexpr static size_t DataLength = 8;
        constexpr static size_t Stride = sizeof(DataType) * DataLength;
    };

    template<typename... Params>
    constexpr size_t Accumulate() {
        return (sizeof(Params) + ...);
    }

    template<typename... Attrs>
    requires (is_input_attr<Attrs> && ...)
    struct InputAttrs {
        constexpr static size_t Stride() {
            size_t total = 0;
            auto make = [&]<typename Attr>() -> void {
                total += Attr::Stride;
            };
            (make.template operator()<Attrs>(), ...);
            return total;
        }

        static std::vector<D3D12_INPUT_ELEMENT_DESC> MakeInputElements() {
            std::vector<D3D12_INPUT_ELEMENT_DESC> elems;
            size_t off = 0;
            auto make = [&]<typename Attr>() -> void {
                Attr::MakeLayout(elems, off);
                off += Attr::Stride;
            };
            (make.template operator()<Attrs>(), ...);
            return elems;
        }

        constexpr static std::array<std::string_view, sizeof...(Attrs)> GetSemanticNames() {
            std::array<std::string_view, sizeof...(Attrs)> result;
            size_t i = 0;
            auto get = [&]<typename Attr>() -> void {
                result[i] = Attr::Name;
                ++i;
            };
            (get.template operator()<Attrs>(), ...);
            return result;
        }

        constexpr static size_t GetSemanticOffset(std::string_view semantic_name) {
            size_t off = 0;
            size_t result = UINT64_MAX;
            auto find = [&]<typename Attr>() -> void {
                if (Attr::Name == semantic_name) {
                    result = off;
                }
                off += Attr::Stride;
            };
            (find.template operator()<Attrs>(), ...);
            return result;
        }

        constexpr static size_t GetSemanticSize(std::string_view semantic_name) {
            size_t result = UINT64_MAX;
            auto find = [&]<typename Attr>() -> void {
                if (Attr::Name == semantic_name) {
                    result = Attr::Stride;
                }
            };
            (find.template operator()<Attrs>(), ...);
            return result;
        }

        uint8_t Storage[Stride()];
    };

    template<typename Attrs>
    struct Input {
        std::vector<Attrs> attrs;

        Input(const Input&) = delete;
        Input(Input&& input) noexcept : attrs(std::move(input.attrs)) {}
    };

    struct CompactMesh {
        struct Primitive {
            D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view {};
            D3D12_INDEX_BUFFER_VIEW index_buffer_view {};
            D3D12_VERTEX_BUFFER_VIEW instance_buffer_view {};
        };
        std::string mesh_name;
        std::vector<Primitive> primitives;
        ResourceHandle vertex_buffer {};
        ResourceHandle index_buffer {};
        ResourceHandle instance_buffer {};
        ResourceManager& mgr;
        bool NoInstance;

        CompactMesh(const std::string& name, ResourceManager& rm, bool NoInstance) : mesh_name(name), mgr(rm), NoInstance(NoInstance) {

        }
        CompactMesh(CompactMesh&) = delete;
        CompactMesh(CompactMesh&& cm) noexcept : mgr(cm.mgr), primitives(std::move(cm.primitives)), vertex_buffer(cm.vertex_buffer), index_buffer(cm.index_buffer), instance_buffer(cm.instance_buffer), NoInstance(cm.NoInstance) {
            cm.vertex_buffer = nullptr;
            cm.index_buffer = nullptr;
            cm.instance_buffer = nullptr;
        }

        /*
         * At any times, a mesh could have the status by following:
         *      A. Copy process in progressing. (R/W)
         *      B. Render process in progressing. (Readonly)
         * So we only need GPU to wait for copy, and set render waitable to avoid release when inuse.
         *
         */
        void RenderSync(uint64_t fence_value) const {
            vertex_buffer->GetCopyWaitable().GPUWait();
            index_buffer->GetCopyWaitable().GPUWait();
            vertex_buffer->GetRenderWaitable().GetFenceValue() = fence_value;
            index_buffer->GetRenderWaitable().GetFenceValue() = fence_value;
            if (instance_buffer != nullptr) {
                instance_buffer->GetCopyWaitable().GPUWait();
                instance_buffer->GetRenderWaitable().GetFenceValue() = fence_value;
            }
        }

        void DrawMesh(ComPtr<ID3D12GraphicsCommandList> list) {
            for (auto & primitive : primitives) {
                D3D12_VERTEX_BUFFER_VIEW primitive_vbvs[2];
                primitive_vbvs[0] = primitive.vertex_buffer_view;
                if (!NoInstance) {
                    primitive_vbvs[1] = primitive.instance_buffer_view;
                }
                list->IASetVertexBuffers(0, NoInstance ? 1 : 2, primitive_vbvs);
                list->IASetIndexBuffer(&primitive.index_buffer_view);
                list->DrawIndexedInstanced(primitive.index_buffer_view.SizeInBytes / (BitsPerPixel(primitive.index_buffer_view.Format) / 8), NoInstance ? 1 : primitive_vbvs[1].SizeInBytes / primitive_vbvs[1].StrideInBytes, 0, 0, 0);
            }
        }

        ~CompactMesh() {
            if (vertex_buffer != nullptr) mgr.MarkAsExpired(vertex_buffer);
            if (index_buffer != nullptr) mgr.MarkAsExpired(index_buffer);
            if (instance_buffer != nullptr) mgr.MarkAsExpired(instance_buffer);
        }
    };

    template<typename VertexAttrs, typename IndexType, typename InstanceAttrs>
    struct Mesh {
        constexpr static size_t VertexStride = VertexAttrs::Stride();
        constexpr static size_t IndexStride = sizeof(IndexType);
        constexpr static size_t InstanceStride = InstanceAttrs::Stride();
        constexpr static size_t NoInstance = InstanceStride == 0;
        struct Primitive {
            std::vector<uint8_t> vertices;
            std::vector<uint8_t> indices;
            std::vector<uint8_t> instances;

            Primitive() = default;
            Primitive(const Primitive&) = delete;
            Primitive(Primitive&& prim) noexcept : vertices(std::move(prim.vertices)), indices(std::move(prim.indices)), instances(std::move(prim.instances)) {}
        };
        std::vector<Primitive> primitives;
        std::string mesh_name;
        ResourceManager& mgr;

        Mesh(const std::string& name, size_t n, ResourceManager& rm) : mesh_name(name), primitives(n), mgr(rm) {}
        Mesh(const Mesh&) = delete;
        Mesh(Mesh&& cm) noexcept : primitives(std::move(cm.primitives)), mesh_name(std::move(cm.mesh_name)), mgr(cm.mgr) {

        }

        template<typename V, typename I>
        requires (sizeof(V) == VertexStride && sizeof(I) == InstanceStride)
        static CompactMesh CreateMeshFromStructuredData(const std::string& name, const std::vector<V>& vertices, const std::vector<IndexType>& indices, const std::vector<I>& instances, ResourceManager& mgr) {
            Mesh mesh(name, 1, mgr);
            mesh.primitives[0].vertices.resize(sizeof(V) * vertices.size());
            mesh.primitives[0].indices.resize(sizeof(IndexType) * indices.size());
            mesh.primitives[0].instances.resize(sizeof(I) * instances.size());
            memcpy(mesh.primitives[0].vertices.data(), vertices.data(), sizeof(V) * vertices.size());
            memcpy(mesh.primitives[0].indices.data(), indices.data(), sizeof(IndexType) * indices.size());
            memcpy(mesh.primitives[0].instances.data(), instances.data(), sizeof(I) * instances.size());
            return mesh.UploadToGPU();
        }

        template<typename V>
        requires (sizeof(V) == VertexStride && InstanceStride == 0)
        static CompactMesh CreateMeshFromStructuredData(const std::string& name, const std::vector<V>& vertices, const std::vector<IndexType>& indices, ResourceManager& mgr) {
            Mesh mesh(name, 1, mgr);
            mesh.primitives[0].vertices.resize(sizeof(V) * vertices.size());
            mesh.primitives[0].indices.resize(sizeof(IndexType) * indices.size());
            memcpy(mesh.primitives[0].vertices.data(), vertices.data(), sizeof(V) * vertices.size());
            memcpy(mesh.primitives[0].indices.data(), indices.data(), sizeof(IndexType) * indices.size());
            return mesh.UploadToGPU();
        }

        CompactMesh UploadToGPU() {
            CompactMesh cm(mesh_name, mgr, NoInstance);
            cm.primitives.resize(primitives.size());
            size_t vertex_buffer_length = 0;
            size_t index_buffer_length = 0;
            size_t instance_buffer_length = 0;
            for (auto& primitive : primitives) {
                vertex_buffer_length += primitive.vertices.size();
                index_buffer_length += primitive.indices.size();
                instance_buffer_length += primitive.instances.size();
            }
            ResourceHandle upload_vb = mgr.CreateLocalBuffer(std::format("Mesh_{}_UploadBuffer_Vertex", mesh_name), 1, vertex_buffer_length, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE, nullptr, true);
            ResourceHandle upload_ib = mgr.CreateLocalBuffer(std::format("Mesh_{}_UploadBuffer_Index", mesh_name), 1, index_buffer_length, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE, nullptr, true);
            size_t off_vb = 0;
            size_t off_ib = 0;
            uint8_t* vb_mapping;
            uint8_t* ib_mapping;
            upload_vb->GetD3D12Resource()->Map(0, nullptr, reinterpret_cast<void**>(&vb_mapping));
            upload_ib->GetD3D12Resource()->Map(0, nullptr, reinterpret_cast<void**>(&ib_mapping));
            for (auto& p : primitives) {
                memcpy(vb_mapping + off_vb, p.vertices.data(), p.vertices.size());
                memcpy(ib_mapping + off_ib, p.indices.data(), p.indices.size());
                off_vb += p.vertices.size();
                off_ib += p.indices.size();
            }
            upload_vb->GetD3D12Resource()->Unmap(0, nullptr);
            upload_ib->GetD3D12Resource()->Unmap(0, nullptr);
            cm.vertex_buffer = mgr.CreateRemoteBuffer(std::format("VertexBuffer_{}", mesh_name), 1, vertex_buffer_length, D3D12_RESOURCE_STATE_COMMON);
            cm.index_buffer = mgr.CreateRemoteBuffer(std::format("IndexBuffer_{}", mesh_name), 1, index_buffer_length, D3D12_RESOURCE_STATE_COMMON);
            Resource::CopyToRemoteBuffer(mgr.GetCopyDispatcher(), upload_vb, cm.vertex_buffer);
            Resource::CopyToRemoteBuffer(mgr.GetCopyDispatcher(), upload_ib, cm.index_buffer);
            if constexpr (!NoInstance) {
                ResourceHandle upload_is = mgr.CreateLocalBuffer(std::format("Upload_InstanceBuffer_{}", mesh_name), 1, instance_buffer_length, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE, nullptr, true);
                cm.instance_buffer = mgr.CreateRemoteBuffer(std::format("InstanceBuffer_{}", mesh_name), 1, instance_buffer_length, D3D12_RESOURCE_STATE_COMMON);
                size_t off_is = 0;
                uint8_t* is_mapping;
                upload_is->GetD3D12Resource()->Map(0, nullptr, reinterpret_cast<void**>(&is_mapping));
                for (auto& p : primitives) {
                    memcpy(is_mapping + off_is, p.instances.data(), p.instances.size());
                    off_is += p.instances.size();
                }
                upload_is->GetD3D12Resource()->Unmap(0, nullptr);
                Resource::CopyToRemoteBuffer(mgr.GetCopyDispatcher(), upload_is, cm.instance_buffer);
            }
            size_t vb_bytes_off = 0;
            size_t ib_bytes_off = 0;
            size_t is_bytes_off = 0;
            for (size_t i = 0; i < primitives.size(); ++i) {
                cm.primitives[i].vertex_buffer_view.BufferLocation = cm.vertex_buffer->GetD3D12Resource()->GetGPUVirtualAddress() + vb_bytes_off;
                cm.primitives[i].vertex_buffer_view.SizeInBytes = primitives[i].vertices.size();
                cm.primitives[i].vertex_buffer_view.StrideInBytes = VertexStride;
                cm.primitives[i].index_buffer_view.BufferLocation = cm.index_buffer->GetD3D12Resource()->GetGPUVirtualAddress() + ib_bytes_off;
                cm.primitives[i].index_buffer_view.SizeInBytes = primitives[i].indices.size();
                cm.primitives[i].index_buffer_view.Format = IndexStride == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
                vb_bytes_off += primitives[i].vertices.size();
                ib_bytes_off += primitives[i].indices.size();
                if constexpr (!NoInstance) {
                    cm.primitives[i].instance_buffer_view.BufferLocation = cm.instance_buffer->GetD3D12Resource()->GetGPUVirtualAddress() + is_bytes_off;
                    cm.primitives[i].instance_buffer_view.SizeInBytes = primitives[i].instances.size();
                    cm.primitives[i].instance_buffer_view.StrideInBytes = InstanceStride;
                    is_bytes_off += primitives[i].instances.size();
                }
            }
            return cm;
        }
    };

    class MeshManager {
        std::unordered_map<std::string, CompactMesh> meshes_;
    public:
        bool CreateMesh(CompactMesh&& mesh) {
            auto[it, r] = meshes_.try_emplace(mesh.mesh_name, std::move(mesh));
            if (!r) {
                LFATAL("Cannot create mesh {}: mesh already exists", mesh.mesh_name);
            }
            return r;
        }

        std::optional<CompactMesh*> GetMesh(const std::string& mesh_name) {
            if (!meshes_.contains(mesh_name)) {
                LFATAL("Cannot get mesh {}: mesh not exists", mesh_name);
                return std::nullopt;
            }
            return &meshes_.at(mesh_name);
        }

    };
}