#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <limits>
#include <stdexcept>
#include <string>

namespace winText {
inline int length(size_t size) {
    if (size > static_cast<size_t>((std::numeric_limits<int>::max)()))
        throw std::length_error("Text is too long for Windows conversion.");
    return static_cast<int>(size);
}

// Flags are explicit so callers can retain replacement of malformed input
// where that was already part of their display behaviour.
inline std::wstring toWide(const std::string& text, DWORD flags = MB_ERR_INVALID_CHARS) {
    if (text.empty()) return {};
    const int size = length(text.size());
    const int count = MultiByteToWideChar(CP_UTF8, flags, text.data(), size, nullptr, 0);
    if (!count) throw std::runtime_error("Cannot convert UTF-8 text for display.");
    std::wstring result(static_cast<size_t>(count), L'\0');
    if (MultiByteToWideChar(CP_UTF8, flags, text.data(), size, result.data(), count) != count)
        throw std::runtime_error("Cannot convert UTF-8 text for display.");
    return result;
}

inline std::string toUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = length(text.size());
    const int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), size, nullptr, 0, nullptr, nullptr);
    if (!count) throw std::runtime_error("Cannot convert Windows text to UTF-8.");
    std::string result(static_cast<size_t>(count), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, text.data(), size, result.data(), count, nullptr, nullptr) != count)
        throw std::runtime_error("Cannot convert Windows text to UTF-8.");
    return result;
}
}
