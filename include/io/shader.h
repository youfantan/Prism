#pragma once

#include <base.h>
#include <mlog.h>
#include <utils.h>

#include <format>

class ShaderLoader
{
public:
    using shader_in_memory_t = struct {
        ComPtr<IDxcBlob> blob;
        ShaderType type;
    };

private:
    std::string prefix_;
    ComPtr<IDxcCompiler3> compiler_;
    ComPtr<IDxcUtils> utils_;

    std::string GetShaderTypeExtension(ShaderType type) {
        switch (type) {
            case ShaderType::VertexShader :
                return "vs";
            case ShaderType::PixelShader:
                return "ps";
        }
        return "unknown";
    }

    std::wstring GetShaderTypeTargetString(ShaderType type) {
        switch (type) {
            case ShaderType::VertexShader :
                return L"vs_6_6";
            case ShaderType::PixelShader:
                return L"ps_6_6";
        }
        return L"unknown";
    }
public:
    ShaderLoader(std::string_view prefix) : prefix_(prefix) {
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));
        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils_));

    }
/*
 *  Old D3DCompiler
    shader_in_memory_t CompileShader(std::string_view shader_name, ShaderType type) {
        std::string shader_path = std::format("{}/{}.{}.hlsl", prefix_, shader_name, GetShaderTypeExtension(type));
        LDEBUG("Loading shader {}({}) into memory", shader_name, shader_path);
        std::string shader_src = ReadFileIntoString(shader_path).value();
        ComPtr<ID3DBlob> binary, error;
        HRESULT r = D3DCompile(shader_src.data(), shader_src.size(), nullptr, nullptr, nullptr, "main", GetShaderTypeTargetString(type).c_str(), 0, 0, &binary, &error);
        if (!SUCCEEDED(r)) {
            LFATAL("Cannot compile shader{}({}), because: {}", shader_name, shader_path, static_cast<const char*>(error->GetBufferPointer()));
        }
        return { binary, type };
    }
*/
    shader_in_memory_t CompileShader(std::string_view shader_name, ShaderType type) {
        std::string shader_path = std::format("{}/{}.{}.hlsl", prefix_, shader_name, GetShaderTypeExtension(type));
        LDEBUG("Compiling shader {}({})", shader_name, shader_path);
        std::wstring wshader_path = ConvertStringToWstring(shader_path);
        std::wstring profile = GetShaderTypeTargetString(type);
        std::vector args = {
            L"-T", profile.c_str(),
            L"-E", L"main",
            wshader_path.c_str()
        };
        ComPtr<IDxcBlobEncoding> src;
        utils_->LoadFile(ConvertStringToWstring(shader_path).c_str(), nullptr, &src);
        DxcBuffer dbuffer {};
        dbuffer.Encoding = DXC_CP_UTF8;
        dbuffer.Ptr = src->GetBufferPointer();
        dbuffer.Size = src->GetBufferSize();
        ComPtr<IDxcResult> result;
        compiler_->Compile(&dbuffer, &args[0], args.size(), nullptr, IID_PPV_ARGS(&result));
        HRESULT r;
        result->GetStatus(&r);
        if (!SUCCEEDED(r)) {
            ComPtr<IDxcBlobUtf8> err_blob;
            result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&err_blob), nullptr);
            LFATAL("Cannot compile shader{}({}), because: {}", shader_name, shader_path, static_cast<const char*>(err_blob->GetBufferPointer()));
            exit(EXIT_FAILURE);
        }
        ComPtr<IDxcBlob> output;
        r = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&output), nullptr);
        if (!SUCCEEDED(r)) {
            LFATAL("Cannot get the compile result of the shader{}({})", shader_name, shader_path);
            exit(EXIT_FAILURE);
        }
        return { output, type };
    }
};
