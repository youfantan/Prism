#pragma once

#include <fstream>
#include <string>

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
            std::cout << "Lazy Object not constructed yet" << std::endl;
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

inline std::optional<std::wstring> ConvertStringToWstring(std::string_view src) {
    if (src.empty()) return std::nullopt;
    int len = MultiByteToWideChar(CP_ACP, 0, src.data(), -1, nullptr, 0);
    if (len == 0) return std::nullopt;
    wchar_t* dst = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (!dst) return std::nullopt;
    MultiByteToWideChar(CP_ACP, 0, src.data(), -1, dst, len);
    return dst;
}

inline std::optional<std::string> ConvertWstringToString(std::wstring_view src) {
    if (src.empty()) return std::nullopt;
    int len = WideCharToMultiByte(CP_ACP, 0, src.data(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return std::nullopt;
    char* dst = (char*)malloc(len);
    if (!dst) return std::nullopt;
    WideCharToMultiByte( CP_ACP, 0, src.data(), -1, dst, len, nullptr, nullptr);
    return dst;
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

template<typename T, size_t E>
class DynamicArray {
private:
    char* ptr_;
    size_t len_;
    size_t capacity_;

    static size_t GetMinAligned(size_t n) {
        return n % E == 0 ? n : (n / E + 1) * E;
    }
public:
    DynamicArray() {
        ptr_ = static_cast<char*>(malloc(sizeof(T) * E));
        len_ = 0;
        capacity_ = E;
    }

    DynamicArray(const DynamicArray& da) {
        ptr_ = static_cast<char*>(malloc(sizeof(T) * da.capacity_));
        len_ = da.len_;
        capacity_ = da.capacity_;
        memcpy(ptr_, da.ptr_, len_);
    }

    DynamicArray(DynamicArray&& da) noexcept : ptr_(da.ptr_), len_(da.len_), capacity_(da.capacity_) {
        da.ptr_ = nullptr;
        da.len_ = 0;
        da.capacity_ = 0;
    }

    DynamicArray& operator=(const DynamicArray& da) {
        if (&da == this) return *this;
        ptr_ = static_cast<char*>(malloc(sizeof(T) * da.capacity_));
        len_ = da.len_;
        capacity_ = da.capacity_;
        memcpy(ptr_, da.ptr_, len_);
        return *this;
    }

    DynamicArray& operator=(DynamicArray&& da) noexcept{
        ptr_ = da.ptr_;
        len_ = da.len_;
        capacity_ = da.capacity_;
        da.ptr_ = nullptr;
        da.len_ = 0;
        da.capacity_ = 0;
        return *this;
    }

    bool PushAt(size_t i, const T& t) {
        if (i > capacity_) {
            if (!Expand(GetMinAligned(i) * sizeof(T))) return false;
        }
        memcpy(&ptr_[i * sizeof(T)], &t, sizeof(T));
        return true;
    }

    bool PushBack(const T& t) {
        bool result = PushAt(len_, t);
        ++len_;
        return result;
    }

    std::optional<T*> Get(size_t i) {
        if (i >= len_) {
            return std::nullopt;
        }
        return reinterpret_cast<T*>(&ptr_[i * sizeof(T)]);
    }

    bool Expand(size_t to) {
        void* ptr = realloc(ptr_, to);
        if (ptr == nullptr) {
            return false;
        }
        ptr_ = static_cast<char*>(ptr);
        capacity_ = to / sizeof(T);
        return true;
    }

    uint64_t GetLength() {
        return len_;
    }

    ~DynamicArray() {
        if (ptr_ != nullptr) {
            free(ptr_);
            ptr_ = nullptr;
            len_ = 0;
            capacity_ = 0;
        }
    }
};

template<typename T, size_t N>
constexpr size_t CountOf(T(&)[N]) {
    return N;
}

inline size_t GetFileLength(std::string_view file_name) {
    std::ifstream input(file_name.data(), std::ios::in | std::ios::binary);
    int64_t now = input.tellg();
    input.seekg(0, std::ios::end);
    size_t len = input.tellg();
    input.seekg(now, std::ios::beg);
    return len;
}

inline std::string ReadFileIntoString(std::string_view file_name) {
    std::ifstream input(file_name.data(), std::ios::in | std::ios::binary);
    size_t len = GetFileLength(file_name);
    std::string content;
    content.resize(len);
    input.read(&content[0], len);
    return content;
}