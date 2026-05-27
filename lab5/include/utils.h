#pragma once
#include <windows.h>
#include <string>
#include <stdexcept>

inline std::string GetLastErrorAsString() {
    DWORD code = GetLastError();
    if (NULL == code) return "No error";
    LPSTR buf = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buf), 0, nullptr
    );
    if (nullptr == buf) {
        return "Windows error code " + std::to_string(code);
    }
    std::string msg(buf);
    LocalFree(buf);
    return msg;
}

inline void ThrowLastError(const std::string& ctx) {
    throw std::runtime_error(ctx + ": " + GetLastErrorAsString());
}

// ---- RAII handle wrapper ---------------------------------------------------

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE h = nullptr) : h_(h) {}
    ~ScopedHandle() {
        if (NULL != h_ && INVALID_HANDLE_VALUE != h_) CloseHandle(h_);
    }
    HANDLE get() const { return h_; }
    bool   valid() const { return NULL != h_ && INVALID_HANDLE_VALUE != h_; }

    ScopedHandle(ScopedHandle&& o) noexcept : h_(o.h_) { o.h_ = nullptr; }
    ScopedHandle& operator=(ScopedHandle&& o) noexcept {
        if (this != &o) {
            if (valid()) CloseHandle(h_);
            h_ = o.h_; o.h_ = nullptr;
        }
        return *this;
    }
    ScopedHandle(const ScopedHandle&)            = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
private:
    HANDLE h_;
};
