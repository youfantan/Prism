#pragma once

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <tiny_gltf.h>

#include "mlog.h"

class GLTFModelLoader {
public:
    using Model = tinygltf::Model;
    using Loader = tinygltf::TinyGLTF;
private:
    std::string prefix_;
    Loader loader_;
public:
    GLTFModelLoader(const std::string& prefix) : prefix_(prefix) {

    }

    void LoadGLB(const std::string& model_name) {
        Model model;
        std::string err, warn;
        loader_.LoadBinaryFromFile(&model, &err, &warn, prefix_ + "/" + model_name);
        if (!warn.empty()) {
            LERROR("Warning occurred when loading glb model {}: {}", model_name, warn);
        }
        if (!err.empty()) {
            LFATAL("Error occurred when loading glb model {}: {}", model_name, err);
        }
        for (size_t m = 0; m < model.meshes.size(); m++) {
        auto& mesh = model.meshes[m];
        std::cout << "Mesh[" << m << "]: " << mesh.name
                  << " (" << mesh.primitives.size() << " primitives)" << std::endl;

        for (size_t p = 0; p < mesh.primitives.size(); p++) {
            auto& prim = mesh.primitives[p];
            std::cout << "  Primitive[" << p << "] mode=" << prim.mode << std::endl;

            for (auto& [attrName, accessorIdx] : prim.attributes) {
                auto& acc = model.accessors[accessorIdx];
                auto& view = model.bufferViews[acc.bufferView];
                auto& buf  = model.buffers[view.buffer];
                const unsigned char* data = buf.data.data() + view.byteOffset + acc.byteOffset;

                std::cout << "    attr: " << attrName
                          << "  count=" << acc.count
                          << "  type=" << acc.type
                          << "  componentType=" << acc.componentType
                          << "  byteOffset=" << acc.byteOffset << std::endl;

                if (attrName == "POSITION" && acc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                    auto* floats = (const float*)data;
                    for (int i = 0; i < std::min(3, (int)acc.count); i++)
                        printf("      [%d] (%.3f, %.3f, %.3f)\n",
                               i, floats[i*3], floats[i*3+1], floats[i*3+2]);
                }
            }

            if (prim.indices >= 0) {
                auto& idxAcc = model.accessors[prim.indices];
                auto& idxView = model.bufferViews[idxAcc.bufferView];
                auto& idxBuf  = model.buffers[idxView.buffer];
                const unsigned char* idxData = idxBuf.data.data() + idxView.byteOffset + idxAcc.byteOffset;

                std::cout << "    indices: count=" << idxAcc.count
                          << "  componentType=" << idxAcc.componentType << std::endl;
            }

            if (prim.material >= 0) {
                auto& mat = model.materials[prim.material];
                std::cout << "    material: " << mat.name << std::endl;

                auto& pbr = mat.pbrMetallicRoughness;
                if (pbr.baseColorTexture.index >= 0)
                    std::cout << "      baseColorTexture: "
                              << model.textures[pbr.baseColorTexture.index].source << std::endl;
                auto& ext = mat.normalTexture;
                if (ext.index >= 0)
                    std::cout << "      normalTexture: "
                              << model.textures[ext.index].source << std::endl;
            }
        }
    }

    }
};
