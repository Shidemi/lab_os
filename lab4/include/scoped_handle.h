#pragma once
#include <windows.h>

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE h = nullptr) : h_(h) {}
    ~ScopedHandle() {
        if (NULL != h_ && INVALID_HANDLE_VALUE != h_) CloseHandle(h_);
    }
    HANDLE get() const { return h_; }

    ScopedHandle(ScopedHandle&& o) noexcept : h_(o.h_) { o.h_ = nullptr; }
    ScopedHandle& operator=(ScopedHandle&& o) noexcept {
        if (this != &o) { if (NULL != h_ && INVALID_HANDLE_VALUE != h_) CloseHandle(h_); h_ = o.h_; o.h_ = nullptr; }
        return *this;
    }
    ScopedHandle(const ScopedHandle&)            = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
private:
    HANDLE h_;
};
