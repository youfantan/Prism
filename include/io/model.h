#pragma once

#include <tiny_gltf.h>

#include <mlog.h>
#include <transform/world.h>
#include <io/texture.h>

#include <render/mesh.h>

namespace Prism
{
    class ModelLoader {
    public:
        using GLTFModel = tinygltf::Model;
        using Loader = tinygltf::TinyGLTF;
        struct Drawable {
            CompactMesh* mesh;
            XMFLOAT4X4 mesh_transform;
        };
        struct Model {
            std::string name;
            std::vector<Drawable> drawables;

            Model(const std::string& name, std::vector<Drawable>&& drawables) : name(name), drawables(std::move(drawables)) {}
            Model(const Model&) = delete;
            Model(Model&& m) noexcept : name(std::move(m.name)), drawables(std::move(m.drawables)) {}
        };

    private:
        std::string prefix_;
        Loader loader_;
        ResourceManager& rm_;
        TextureLoader& tex_loader_;
        MeshManager& mesh_mgr_;

        struct ModelInfo {
            std::string path;
            std::string type;
        };

        std::unordered_map<std::string, ModelInfo> model_infos_;
    public:
        ModelLoader(const std::string& prefix, ResourceManager& rm, TextureLoader& tex_loader, MeshManager& mesh_mgr) : prefix_(prefix), rm_(rm), tex_loader_(tex_loader), mesh_mgr_(mesh_mgr) {
            std::string index_path = prefix_ + "/" + "models.json";
            std::ifstream index_file(index_path, std::ios::in | std::ios::binary);
            json index = json::parse(index_file);
            auto& models = index["models"];
            for (auto& model : models) {
                std::string name = model["name"];
                std::string type = model["type"];
                std::string path = prefix_ + "/" + static_cast<std::string>(model["path"]);
                model_infos_[name] = ModelInfo { path, type };
            }
        }

        std::optional<std::string> GetModelPath(const std::string& name) {
            if (!model_infos_.contains(name)) {
                return std::nullopt;
            }
            return model_infos_[name].path;
        }

        template<typename VertexAttrs, typename IndexType, typename InstanceAttrs>
        requires (InstanceAttrs::GetSemanticOffset("TEXIDX") != UINT64_MAX)
        Model LoadGLB(const std::string& model_name) {
            using Mesh = Mesh<VertexAttrs, IndexType, InstanceAttrs>;
            using Primitive = Mesh::Primitive;
            auto q = GetModelPath(model_name);
            if (!q) {
                LFATAL("Cannot load GLB model {}, model not exists", model_name);
            }
            std::string model_path = q.value();
            GLTFModel model;
            std::string err, warn;
            loader_.LoadBinaryFromFile(&model, &err, &warn, model_path);
            if (!warn.empty()) {
                LERROR("Warning while loading GLB model {}: {}", model_path, warn);
            }
            if (!err.empty()) {
                LFATAL("Cannot load GLB model {} because {}", model_path, err);
            }
            for (size_t i = 0; i < model.materials.size(); ++i) {
                tex_loader_.LoadModelTextures(model_name, model, model.materials[i], i);
            }
            for (size_t i = 0; i < model.meshes.size(); ++i) {
                auto& mesh = model.meshes[i];
                Mesh mesh_create(std::format("{}_Mesh #{}", model_name, i), mesh.primitives.size(), rm_);
                for (size_t p = 0; p < mesh.primitives.size(); ++p) {
                    auto& primitive = mesh.primitives[p];
                    if (primitive.attributes.empty()) continue;
                    Primitive& destination = mesh_create.primitives[p];
                    auto semantic_names = VertexAttrs::GetSemanticNames();
                    size_t validated_attr_elem_count = model.accessors[primitive.attributes.begin()->second].count;
                    for (auto& attr : primitive.attributes) {
                        if (model.accessors[attr.second].count != validated_attr_elem_count) {
                            LFATAL("Attributes elements count in given model {} mesh {} primitive #{} is invalid. required: {}, this: {}", model_name, mesh.name, p, validated_attr_elem_count, model.accessors[attr.second].count);
                        }
                    }
                    destination.vertices.resize(validated_attr_elem_count * VertexAttrs::Stride());
                    for (std::string_view& cname : semantic_names) {
                        std::string name(cname.data());
                        if (!primitive.attributes.contains(name)) {
                            if (name.starts_with("TEXCOORD")) {
                                LWARN("Attribute {} not exists in given model {} mesh {} primitive #{}, fill with zero", name, model_name, mesh.name, p);
                                continue;
                            }
                            LFATAL("Attribute {} not exists in given model {} mesh {} primitive #{}", name, model_name, mesh.name, p);
                        }
                        size_t attr_off = VertexAttrs::GetSemanticOffset(name);
                        size_t attr_size = VertexAttrs::GetSemanticSize(name);
                        auto& attr_access = model.accessors[primitive.attributes[name]];
                        auto& attr_buffer_view = model.bufferViews[attr_access.bufferView];
                        auto& attr_buffer = model.buffers[attr_buffer_view.buffer];
                        size_t actual_stride = attr_buffer_view.byteStride != 0 ? attr_buffer_view.byteStride : tinygltf::GetComponentSizeInBytes(attr_access.componentType) * tinygltf::GetNumComponentsInType(attr_access.type);
                        if (attr_size != actual_stride) {
                            LFATAL("Attribute stride {} in given model {} mesh {} primitive #{} not equals to request. required: {}, this: {}", name, model_name, mesh.name, p, attr_size, attr_buffer_view.byteStride);
                        }
                        size_t attr_byte_offset = attr_buffer_view.byteOffset + attr_access.byteOffset;
                        for (size_t e = 0; e < validated_attr_elem_count; ++e) {
                            memcpy(&destination.vertices[e * VertexAttrs::Stride() + attr_off], &attr_buffer.data[attr_byte_offset + e * actual_stride], attr_size);
                        }
                    }
                    auto& acc_indices = model.accessors[primitive.indices];
                    auto& indices_buffer_view = model.bufferViews[acc_indices.bufferView];
                    auto& indices_buffer = model.buffers[indices_buffer_view.buffer];
                    size_t indices_byte_offset = indices_buffer_view.byteOffset + acc_indices.byteOffset;
                    destination.indices.resize(acc_indices.count * sizeof(IndexType));
                    if (indices_buffer.data.empty()) {
                        LFATAL("Indices buffer is empty in given model {} mesh {} primitive #{}", model_name, mesh.name, p);
                    }
                    if (acc_indices.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        if (sizeof(IndexType) < sizeof(uint32_t)) {
                            LFATAL("Index type sized {} is not big enough to contains uint32_t index in given model {} mesh {} primitive #{}", sizeof(IndexType), model_name, mesh.name, p);
                        }
                        for (size_t d = 0; d < acc_indices.count; ++d) {
                            *reinterpret_cast<IndexType*>(&destination.indices[d * sizeof(IndexType)]) = *reinterpret_cast<const uint32_t*>(&indices_buffer.data[indices_byte_offset + d * sizeof(uint32_t)]);
                        }
                    } else if (acc_indices.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        if (sizeof(IndexType) < sizeof(uint16_t)) {
                            LFATAL("Index type sized {} is not big enough to contains uint16_t index in given model {} mesh {} primitive #{}", sizeof(IndexType), model_name, mesh.name, p);
                        }
                        for (size_t d = 0; d < acc_indices.count; ++d) {
                            *reinterpret_cast<IndexType*>(&destination.indices[d * sizeof(IndexType)]) = *reinterpret_cast<const uint16_t*>(&indices_buffer.data[indices_byte_offset + d * sizeof(uint16_t)]);
                        }
                    }
                    if (primitive.material < 0) LFATAL("Cannot load GLB model {}: no material found", model_path);
                    std::string texture_name = std::format("{}_Material #{}", model_name, primitive.material);
                    destination.instances.resize(InstanceAttrs::Stride());
                    Texture* tex = tex_loader_.Get(texture_name).value();
                    size_t texidx_offset = InstanceAttrs::GetSemanticOffset("TEXIDX");
                    std::string tex_info;
                    if (tex->type == TextureType::IMAGE) {
                        uint32_t sentinel = UINT32_MAX;
                        memcpy(&destination.instances[0] + texidx_offset, &tex->Resources.Image.tex->GetResourceMeta().Tex2D.bind_index, sizeof(uint32_t));
                        memcpy(&destination.instances[0] + texidx_offset + sizeof(uint32_t), &sentinel, sizeof(uint32_t));
                        memcpy(&destination.instances[0] + texidx_offset + sizeof(uint32_t) * 2, &sentinel, sizeof(uint32_t));
                        tex_info = std::format("type Image, referenced texture index: {}", tex->Resources.Image.tex->GetResourceMeta().Tex2D.bind_index);
                    } else if (tex->type == TextureType::PBR) {
                        memcpy(&destination.instances[0] + texidx_offset, &tex->Resources.PBR.tex->GetResourceMeta().Tex2D.bind_index, sizeof(uint32_t));
                        memcpy(&destination.instances[0] + texidx_offset + sizeof(uint32_t) * 1, &tex->Resources.PBR.normal_tex->GetResourceMeta().Tex2D.bind_index, sizeof(uint32_t));
                        memcpy(&destination.instances[0] + texidx_offset + sizeof(uint32_t) * 2, &tex->Resources.PBR.rough_tex->GetResourceMeta().Tex2D.bind_index, sizeof(uint32_t));
                        tex_info = std::format("type PBR, referenced texture index: color {}, normal {}, rough {}", tex->Resources.PBR.tex->GetResourceMeta().Tex2D.bind_index, tex->Resources.PBR.normal_tex->GetResourceMeta().Tex2D.bind_index, tex->Resources.PBR.rough_tex->GetResourceMeta().Tex2D.bind_index);
                    }
                    std::string model_info (R"(
Loaded primitive #{} of mesh '{}' in model '{}'({}), detailed,
Texture:
    {}
Indices:
    count {}, type {}
Attributes:)");
                    for (std::string_view& cname : semantic_names) {
                        auto& attr_access = model.accessors[primitive.attributes[cname.data()]];
                        model_info.append(std::format("\n    attr '{}' count {}, type {}, stride {}", cname.data(), attr_access.count, attr_access.componentType, model.bufferViews[attr_access.bufferView].byteStride));
                    }
                    LDEBUG(model_info.data(), p, mesh.name, model_name, model_path, tex_info, acc_indices.count, acc_indices.componentType);
                }
                mesh_mgr_.CreateMesh(mesh_create.UploadToGPU());
            }

            struct TransformNode {
                XMMATRIX mat;
                tinygltf::Node* gltf_node;
            };
            std::vector<TransformNode> nodes;
            std::map<uint32_t, std::vector<XMFLOAT4X4>> instances;

            auto MakeTransformMatrix = [](const tinygltf::Node* n) {
                if (!n->matrix.empty()) {
                    XMFLOAT4X4 m;
                    for (int i = 0; i < 16; ++i) {
                        reinterpret_cast<float*>(&m)[i] = static_cast<float>(n->matrix[i]);
                    }
                    return XMLoadFloat4x4(&m);
                } else {
                    TranslationTransform translation {};
                    XYZWRotationTransform rotation {};
                    ScalingTransform scaling {};
                    if (!n->translation.empty()) {
                        translation.x = static_cast<float>(n->translation[0]);
                        translation.y = static_cast<float>(n->translation[1]);
                        translation.z = static_cast<float>(n->translation[2]);
                    }
                    if (!n->rotation.empty()) {
                        rotation.x = static_cast<float>(n->rotation[0]);
                        rotation.y = static_cast<float>(n->rotation[1]);
                        rotation.z = static_cast<float>(n->rotation[2]);
                        rotation.w = static_cast<float>(n->rotation[3]);
                    }
                    if (!n->scale.empty()) {
                        scaling.x = static_cast<float>(n->scale[0]);
                        scaling.y = static_cast<float>(n->scale[1]);
                        scaling.z = static_cast<float>(n->scale[2]);
                    }
                    return MakeWorldMatrix(scaling, rotation, translation);
                }
            };

            if (model.defaultScene >= 0) {
                for (auto& n : model.scenes[model.defaultScene].nodes) {
                    nodes.emplace_back(MakeUnitMatrix(), &model.nodes[n]);
                }
            } else {
                LFATAL("Cannot load GLB model {}: no default scene found", model_path);
            }
            while (!nodes.empty()) {
                TransformNode node = std::move(nodes.back());
                nodes.pop_back();
                XMMATRIX mat = MakeTransformMatrix(node.gltf_node);
                node.mat = mat * node.mat;
                tinygltf::Node* gltf_node = node.gltf_node;
                if (gltf_node->mesh >= 0) {
                    auto& instance = instances[gltf_node->mesh].emplace_back();
                    XMStoreFloat4x4(&instance, node.mat);
                }
                for (auto& n : gltf_node->children) {
                    nodes.emplace_back(node.mat, &model.nodes[n]);
                }
            }
            std::vector<Drawable> drawables;
            for (auto& m : instances) {
                CompactMesh* mesh = mesh_mgr_.GetMesh(std::format("{}_Mesh #{}", model_name, m.first)).value();
                for (auto& xm : m.second) {
                    drawables.emplace_back(mesh, xm);
                }
            }
            return { model_name, std::move(drawables) };
        }
    };

}
