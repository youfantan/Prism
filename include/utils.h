#pragma once

#include <cuchar>
#include <DirectXMath.h>
#include <fstream>
#include <string>
#include <functional>
#include <windows.h>
#include <mlog.h>
#include <thread>

namespace Prism
{
    template<size_t ALIGNMENT, typename T>
T AlignV(T value) {
    return (value % ALIGNMENT == 0) ? value : (value / ALIGNMENT + 1) * ALIGNMENT;
}

/* Package of C++ class that enabled lazy initializing */
template<typename T>
class Lazy {
private:
    alignas(T) unsigned char object_[sizeof(T)]{};
    bool initialized_{};
public:
    Lazy() = default;

    Lazy(Lazy&) = delete;
    Lazy(Lazy&& obj) noexcept : object_(obj.object_), initialized_(obj.initialized_) {
        obj.object_ = nullptr;
        obj.initialized_ = false;
    }

    /* When initialize params are ready, call Construct() to initialize the class */
    template<typename... Args>
    void Construct(Args&& ...args) {
        if (!initialized_) {
            new(object_) T(std::forward<Args>(args)...);
            initialized_ = true;
        }
    }

    /* After called Construct(), call Get() to get the reference of the class */
    T& Get() {
#ifndef NDEBUG
        if (!initialized_) {
            LFATAL("Cannot execute Get() of a Lazy object, the Lazy object not constructed yet");
            exit(EXIT_FAILURE);
        }
#endif
        return *reinterpret_cast<T*>(object_);
    }

    /* Indicates whether called Construct() */
    bool Constructed() {
        return initialized_;
    }

    /* Lazy objects has the same life time like common classes */
    ~Lazy() {
        if (initialized_) reinterpret_cast<T*>(object_)->~T();
    }
};


/* Package of CPU Buffer with Move constructor only */
class UniqueByteBuffer {
private:
    uint64_t size_;
    char* ptr_;
    bool rw_; // true -> read, false -> write
public:
    /* Construct Buffer with given size and set to Write Only mode */
    UniqueByteBuffer(size_t size) : size_(size), rw_(true) {
        ptr_ = static_cast<char *>(malloc(size_));
    }
    /* Construct Buffer with give pointer and size and set to Read Only mode */
    UniqueByteBuffer(char* ptr, size_t size) : ptr_(ptr), size_(size), rw_(false) {}
    UniqueByteBuffer(UniqueByteBuffer&) = delete;
    UniqueByteBuffer(UniqueByteBuffer&& buf) noexcept : size_(buf.size_), ptr_(buf.ptr_), rw_(buf.rw_) {
        buf.size_ = 0;
        buf.ptr_ = nullptr;
    }
    /* Array-like access */
    char& operator[](uint64_t off) {
        return ptr_[off];
    }

    uint64_t size() {
        return size_;
    }

    /* Auto release */
    ~UniqueByteBuffer() {
        if (!rw_ && ptr_ != nullptr) {
            free(ptr_);
        }
    }
};

inline std::wstring ConvertStringToWstring(const std::string& src) {
    if (src.empty()) return {};
    int len = MultiByteToWideChar(CP_ACP, 0, src.data(), -1, nullptr, 0);
    if (len == 0) {
        LFATAL("Cannot convert string to wstring, Win32 API MultiByteToWideChar() returns length == 0");
        return {};
    }
    wchar_t* dst = (wchar_t*)malloc(len * sizeof(wchar_t));
    MultiByteToWideChar(CP_ACP, 0, src.data(), -1, dst, len);
    return dst;
}
inline std::string ConvertWstringToString(const std::wstring& src) {
    if (src.empty()) {};
    int len = WideCharToMultiByte(CP_ACP, 0, src.data(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) {
        LFATAL("Cannot convert wstring to string, Win32 API WideCharToMultiByte() returns length == 0");
        return {};
    }
    char* dst = (char*)malloc(len);
    WideCharToMultiByte( CP_ACP, 0, src.data(), -1, dst, len, nullptr, nullptr);
    return dst;
}

inline std::u32string ConvertStringToU32String(const std::string& src) {
    std::u32string result;
    std::mbstate_t state{};
    char32_t c32;
    const char* ptr = src.data();
    const char* end = ptr + src.size();
    while (size_t rc = std::mbrtoc32(&c32, ptr, end - ptr, &state)) {
        if (rc == static_cast<size_t>(-1) || rc == static_cast<size_t>(-3)) break;
        if (rc == static_cast<size_t>(-2)) break;
        result.push_back(c32);
        ptr += rc;
    }
    return result;
}
inline std::string ConvertU32StringToString(const std::u32string& src) {
    std::string result;
    std::mbstate_t state{};
    char buf[8];
    for (char32_t cp : src) {
        size_t rc = std::c32rtomb(buf, cp, &state);
        if (rc == static_cast<size_t>(-1)) break;
        result.append(buf, rc);
    }
    return result;
}

/* Performance Counter, used to track FPS, CPU usage, GPU usage etc. */
class PerformanceCounter {
private:
    LARGE_INTEGER freq_;
    LARGE_INTEGER prev_;
    float fps_;
public:
    PerformanceCounter() : fps_(0) {
        QueryPerformanceFrequency(&freq_);
        QueryPerformanceCounter(&prev_);
    }

    float DeltaMs() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        float delta_ms = static_cast<float>(now.QuadPart - prev_.QuadPart) / static_cast<float>(freq_.QuadPart) * 1000;
        fps_ = 1.0f / (delta_ms / 1000);
        prev_ = now;
        return delta_ms;
    }

    int QueryFPS() {
        return fps_;
    }
};

class MetronomeTimer {
private:
    uint32_t interval_;
    std::function<void(MetronomeTimer& mt)> callback_;
    bool flag_;
public:
    MetronomeTimer(uint32_t interval, const std::function<void(MetronomeTimer&)>& callback) : interval_(interval), callback_(callback), flag_(false) {

    }

    void Start() {
        flag_ = true;
        while (flag_) {
            auto next = std::chrono::system_clock::now() + std::chrono::milliseconds(interval_);
            callback_(*this);
            std::this_thread::sleep_until(next);
        }
    }

    void Stop() {
        flag_ = false;
    }

    void ChangeInterval(uint32_t new_interval) {
        interval_ = new_interval;
    }

};

template<typename T, size_t N>
constexpr size_t CountOf(T(&)[N]) {
    return N;
}

inline bool FileExists(const std::string& file_name) {
    std::ifstream in(file_name);
    return in.good();

}

inline size_t GetFileLength(const std::string& file_name) {
    std::ifstream input(file_name.data(), std::ios::in | std::ios::binary);
    int64_t now = input.tellg();
    input.seekg(0, std::ios::end);
    size_t len = input.tellg();
    input.seekg(now, std::ios::beg);
    return len;
}

inline std::optional<std::string> ReadFileIntoString(const std::string& file_name) {
    std::ifstream input(file_name.data(), std::ios::in | std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    size_t len = GetFileLength(file_name);
    std::string content;
    content.resize(len);
    input.read(&content[0], len);
    return content;
}

inline void WriteStringToFile(const std::string& file_name, const std::string& str) {
    std::ofstream output(file_name, std::ios::out | std::ios::binary);
    output.write(str.c_str(), str.size());
}

inline uint32_t RGBA32(const DirectX::XMFLOAT4& xf4) {
    uint8_t r = xf4.x * 255;
    uint8_t g = xf4.y * 255;
    uint8_t b = xf4.z * 255;
    uint8_t a = xf4.w * 255;
    return r | (g << 8) | (b << 16) | (a << 24);
}

template<typename T, typename U>
concept decay_as = std::same_as<std::decay_t<T>, U>;
}
