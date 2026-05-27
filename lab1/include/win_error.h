#pragma once

#include <windows.h>
#include <string>
#include <stdexcept>

// Retrieves the last Windows API error as a human-readable string
inline std::string GetLastErrorAsString() {
    DWORD errorCode = GetLastError();
    if (NULL == errorCode) {
        return "No error";
    }

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&messageBuffer),
        0,
        nullptr
    );

    if (0 == size || nullptr == messageBuffer) {
        return "Windows error code " + std::to_string(errorCode);
    }

    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
}

// Throws a runtime_error with the Windows error message appended
inline void ThrowLastError(const std::string& context) {
    throw std::runtime_error(context + ": " + GetLastErrorAsString());
}
