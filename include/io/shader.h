#pragma once

#include <base.h>
#include <mlog.h>
#include <utils.h>

#include <format>

class ShaderLoader
{
public:
    using shader_in_memory_t = struct
    {
        ComPtr<ID3DBlob> blob;
        ShaderType type;
    };

private:
    std::string prefix_;

    std::string GetShaderTypeExtension(ShaderType type) {
        switch (type) {
            case ShaderType::VertexShader :
                return "vs";
            case ShaderType::PixelShader:
                return "ps";
        }
        return "unknown";
    }

    std::string GetShaderTypeTargetString(ShaderType type) {
        switch (type) {
            case ShaderType::VertexShader :
                return "vs_5_1";
            case ShaderType::PixelShader:
                return "ps_5_1";
        }
        return "unknown";
    }
public:
    ShaderLoader(std::string_view prefix) : prefix_(prefix) {

    }

    shader_in_memory_t LoadShaderIntoMemory(std::string_view shader_name, ShaderType type) {
        std::string shader_path = std::format("{}/{}.{}.hlsl", prefix_, shader_name, GetShaderTypeExtension(type));
        LDEBUG("Loading shader {}({}) into memory", shader_name, shader_path);
        std::string shader_src = ReadFileIntoString(shader_path);
        ComPtr<ID3DBlob> binary, error;
        HRESULT r = D3DCompile(shader_src.data(), shader_src.size(), nullptr, nullptr, nullptr, "main", GetShaderTypeTargetString(type).c_str(), 0, 0, &binary, &error);
        if (!SUCCEEDED(r)) {
            LFATAL("Cannot compile shader{}({}), because: {}", shader_name, shader_path, static_cast<const char*>(error->GetBufferPointer()));
        }
        return { binary, type };
    }
};
