#pragma once

#include <base.h>
#include <render/resource.h>
#include <io/image.h>
#include <json.hpp>

using nlohmann::json;

namespace Prism
{
    enum class TextureType {
        IMAGE,
        PBR,
    };

    inline std::string GetTextureTypeString(TextureType tt) {
        switch (tt) {
            case TextureType::IMAGE: {
                return "IMAGE";
            }
            case TextureType::PBR: {
                return "PBR";
            }
            default: {
                return "Unknown";
            }
        }
    }

    struct Texture {
        std::string name;
        TextureType type;
        union {
            struct {
                ResourceHandle tex;
            } Image;
            struct {
                ResourceHandle tex;
                ResourceHandle normal_tex;
                ResourceHandle rough_tex;
                ResourceHandle disp_tex;
            } PBR;
        } Resources;
    };

    class TextureLoader {
        std::string prefix_;
        ResourceManager& rm_;
        std::unordered_map<std::string, Texture> texture_map_;

        enum class TextureFormat {
            UNKNOWN,
            JPG,
            PNG,
            DDS,
        };

        ResourceHandle UploadTexture(const std::string name, const std::string path) {
            TextureFormat fmt {};
            if (path.ends_with(".jpg")) {
                fmt = TextureFormat::JPG;
            } else if (path.ends_with(".png")) {
                fmt = TextureFormat::PNG;
            } else if (path.ends_with(".dds")) {
                fmt = TextureFormat::DDS;
            }
            if (fmt == TextureFormat::JPG) {
                Image img = ImageLoader::LoadJPG(path).value();
                ResourceHandle tex = rm_.CreateTexture2DFromImage(name, img);
                return tex;
            }
            if (fmt == TextureFormat::PNG) {
                Image img = ImageLoader::LoadPNG(path).value();
                ResourceHandle tex = rm_.CreateTexture2DFromImage(name, img);
                return tex;
            }
            LFATAL("Unsupported texture format {} of texture {}", static_cast<int>(fmt), name);
            return nullptr;
        }

        void LoadStaticTextures() {
            std::string index_path = prefix_ + "/" + "textures.json";
            std::ifstream index_file(index_path, std::ios::in | std::ios::binary);
            json index = json::parse(index_file);
            auto& textures = index["Textures"];
            for (auto& texture : textures) {
                Texture ntex {};
                std::string type = texture["type"];
                auto& files = texture["files"];
                ntex.name = texture["name"];
                if (texture_map_.contains(ntex.name)) {
                    LFATAL("Cannot load texture {}: texture already exists", ntex.name);
                }
                if (type == "img") {
                    ntex.type = TextureType::IMAGE;
                    for (auto& file : files) {
                        std::string ftype = file["type"];
                        std::string path = prefix_ + "/" + static_cast<std::string>(file["path"]);
                        if (ftype == "texture") {
                            ntex.Resources.Image.tex = UploadTexture("Texture_{}", path);
                        }
                    }
                } else if (type == "pbr") {
                    ntex.type = TextureType::PBR;
                    for (auto& file : files) {
                        std::string ftype = file["type"];
                        std::string path = prefix_ + "/" + static_cast<std::string>(file["path"]);
                        if (ftype == "texture") {
                            ntex.Resources.PBR.tex = UploadTexture("PBRTexture_{}", path);
                        } else if (ftype == "normal_texture") {
                            ntex.Resources.PBR.normal_tex = UploadTexture("PBRTexture_Normal_{}", path);
                        } else if (ftype == "roughness_texture") {
                            ntex.Resources.PBR.rough_tex = UploadTexture("PBRTexture_Roughness_{}", path);
                        } else if (ftype == "displacement_texture") {
                            ntex.Resources.PBR.disp_tex = UploadTexture("PBRTexture_Displacement_{}", path);
                        } else {
                            LFATAL("Unrecognized PBR texture type {}", ftype);
                        }
                    }
                } else {
                    LFATAL("Cannot parse textures.json: Unrecognized texture type {}", type);
                }
                texture_map_[ntex.name] = ntex;
            }
        }

    public:
        TextureLoader(std::string prefix, ResourceManager& rm) : prefix_(prefix), rm_(rm) {
            LoadStaticTextures();
        }

        void LoadModelTextures(const std::string& model_name, const tinygltf::Model& model, const tinygltf::Material& material) {
            auto& pbr = material.pbrMetallicRoughness;
            Texture tex {};
            tex.name = std::format("{}_{}_Material", model_name, material.name);
            if (texture_map_.contains(tex.name)) {
                LFATAL("Cannot load model {} texture: resource with same name {} already exists", model_name, tex.name);
            }
            if (pbr.baseColorTexture.index < 0) {
                LFATAL("Cannot load model material {}: at least basic color texture is required in the model", tex.name);
            } else {
                std::string name = std::format("{}_{}_ColorTexture", model_name, material.name);
                auto& img = model.images[model.textures[pbr.baseColorTexture.index].source];
                tex.Resources.Image.tex = rm_.CreateTexture2DFromGLTFImage(name, img);
            }
            if (pbr.metallicRoughnessTexture.index > 0 && material.normalTexture.index > 0) {
                tex.type = TextureType::PBR;
                auto& normal = model.images[model.textures[material.normalTexture.index].source];
                auto& rough = model.images[model.textures[pbr.metallicRoughnessTexture.index].source];
                std::string normal_name = std::format("{}_{}_NormalTexture", model_name, material.name);
                std::string rough_name = std::format("{}_{}_RoughnessTexture", model_name, material.name);
                tex.Resources.PBR.normal_tex = rm_.CreateTexture2DFromGLTFImage(normal_name, normal);
                tex.Resources.PBR.rough_tex = rm_.CreateTexture2DFromGLTFImage(rough_name, rough);
                LINFO("Loaded texture of model material, {}:{} -> {}, type: {}", model_name, material.name, tex.name, "PBR");
            } else {
                tex.type = TextureType::IMAGE;
                LINFO("Loaded texture of model material, {}:{} -> {}, type: {}", model_name, material.name, tex.name, "IMAGE");
            }
            texture_map_[tex.name] = tex;
        }

        std::optional<Texture*> Get(const std::string& name) {
            if (!texture_map_.contains(name)) {
                LFATAL("Cannot get texture {}: texture not exists", name);
                return std::nullopt;
            }
            return &texture_map_[name];
        }
    };
}