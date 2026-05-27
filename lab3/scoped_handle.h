#pragma once

#include <windows.h>

// RAII wrapper for HANDLE; calls CloseHandle on destruction.
// Non-copyable; ensures handles are released even on exceptions.
class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE h = nullptr) : handle_(h) {}

    ~ScopedHandle() {
        if (NULL != handle_ && INVALID_HANDLE_VALUE != handle_) {
            CloseHandle(handle_);
        }
    }

    HANDLE get() const { return handle_; }

    // Allow move semantics so we can store ScopedHandles in vectors
    ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            if (NULL != handle_ && INVALID_HANDLE_VALUE != handle_) {
                CloseHandle(handle_);
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ScopedHandle(const ScopedHandle&)            = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

private:
    HANDLE handle_;
};
