#pragma once

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <tiny_gltf.h>
#include <mlog.h>

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
};
