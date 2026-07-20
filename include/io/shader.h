#pragma once

#include <base.h>
#include <mlog.h>
#include <utils.h>

#include <format>
#include <filesystem>
#include <json.hpp>

using nlohmann::json;

namespace Prism
{
    class ShaderLoader {
private:
    std::string prefix_;
    std::string cache_dir_;

    struct ShaderSrc {
        std::string name;
        ShaderType type;
        std::string path;
        std::string profile;
        std::string entry;
    };

    struct ShaderCache {
        std::string path;
        std::string src_hash;
    };

    ComPtr<IDxcCompiler3> compiler_;
    ComPtr<IDxcUtils> utils_;
    ComPtr<IDxcLibrary> library_;
    std::unordered_map<std::string, std::string> shader_binaries_;

    ShaderType ParseType(const std::string& str) {
        if (str == "vs") {
            return ShaderType::VertexShader;
        }
        if (str == "ps") {
            return ShaderType::PixelShader;
        }
        return ShaderType::Unknown;
    }

    std::string ParseType(ShaderType type) {
        if (type == ShaderType::VertexShader) {
            return "vs";
        }
        if (type == ShaderType::PixelShader) {
            return "ps";
        }
        return "unknown";
    }
public:
    ShaderLoader(const std::string& prefix) : prefix_(prefix) {
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));
        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils_));
        DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library_));
        cache_dir_ = std::format("{}/caches", prefix_);
        std::string index_file = std::format("{}/shaders.json", prefix_);
        std::filesystem::directory_entry cache_dir(cache_dir_);
        if (!cache_dir.exists()) {
            std::filesystem::create_directory(cache_dir);
        }
        std::ifstream findex(index_file, std::ios::in | std::ios::binary);
        json index = json::parse(findex);
        std::vector<ShaderSrc> shader_srcs;
        auto& shaders = index["Shaders"];
        for (auto& shader : shaders) {
            auto& src = shader_srcs.emplace_back(shader["name"], ParseType(static_cast<std::string>(shader["type"])), shader["path"], shader["profile"], shader["entry"]);
            std::string file_src = ReadFileIntoString(prefix_ + "/" + src.path).value();
            std::string sha256 = CalcSHA256HexDigest(file_src);
            std::string out_binary_file = std::format("{}/caches/{}.{}.cache", prefix_, src.name, static_cast<std::string>(shader["type"]));
            std::string unified_key = std::format("{}.{}", static_cast<std::string>(shader["name"]), static_cast<std::string>(shader["type"]));
            if (!shader.contains("sha256")) {
                shader_binaries_[unified_key] = CompileShader(src, out_binary_file);
                shader["cache_file"] = out_binary_file;
                shader["sha256"] = sha256;
            } else if (shader["sha256"] != sha256) {
                shader_binaries_[unified_key] = CompileShader(src, shader["cache_file"]);
                shader["sha256"] = sha256;
            } else {
                LINFO("Loaded shader {}({}) by cache file {}", src.name, src.path, static_cast<std::string>(shader["cache_file"]));
                shader_binaries_[unified_key] = ReadFileIntoString(shader["cache_file"]).value();
            }
        }
        std::ofstream oindex(index_file, std::ios::out | std::ios::binary);
        oindex << index.dump();
    }

    std::string CompileShader(ShaderSrc& src, const std::string& output_path) {
        LINFO("Compiling shader {}({})", src.name, src.path);
        auto profile = ConvertStringToWstring(src.profile);
        auto entry = ConvertStringToWstring(src.entry);
        auto path = ConvertStringToWstring(prefix_ + "/" + src.path);
        std::vector args = {
            L"-T", profile.c_str(),
            L"-E", entry.c_str(),
            path.c_str()
        };
        ComPtr<IDxcBlobEncoding> file_src;
        utils_->LoadFile(path.c_str(), nullptr, &file_src);
        DxcBuffer dbuffer {};
        dbuffer.Encoding = DXC_CP_UTF8;
        dbuffer.Ptr = file_src->GetBufferPointer();
        dbuffer.Size = file_src->GetBufferSize();
        ComPtr<IDxcResult> result;
        compiler_->Compile(&dbuffer, &args[0], args.size(), nullptr, IID_PPV_ARGS(&result));
        HRESULT r;
        result->GetStatus(&r);
        if (!SUCCEEDED(r)) {
            ComPtr<IDxcBlobUtf8> err_blob;
            result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&err_blob), nullptr);
            LFATAL("Cannot compile shader {}({}), because: {}", src.name, src.path, static_cast<const char*>(err_blob->GetBufferPointer()));
            exit(EXIT_FAILURE);
        }
        ComPtr<IDxcBlob> output;
        r = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&output), nullptr);
        if (!SUCCEEDED(r)) {
            LFATAL("Cannot get the compile result of the shader {}({})", src.name, src.path);
            exit(EXIT_FAILURE);
        }
        std::string out_binary(static_cast<const char*>(output->GetBufferPointer()), output->GetBufferSize());
        WriteStringToFile(output_path, out_binary);
        return { static_cast<const char*>(output->GetBufferPointer()), output->GetBufferSize() };
    }

    std::optional<std::pair<void*, size_t>> LoadShader(const std::string& name, ShaderType type) {
        std::string unified_key = std::format("{}.{}", name, ParseType(type));
        if (!shader_binaries_.contains(unified_key)) {
            LFATAL("Cannot load shader {}(type {}): Shader not exists", name, ParseType(type));
            return std::nullopt;
        };
        auto& result = shader_binaries_[unified_key];
        return std::make_pair(reinterpret_cast<void*>(result.data()), result.size());
    }
};

}