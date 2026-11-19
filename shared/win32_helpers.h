#pragma once

// Header-only Win32 ownership and validation helpers.
//
// This file is inlined by tools/bundle.py, so it must stay self-contained and
// must never include another repository header.

#include <windows.h>

namespace yoko::win32 {

inline constexpr float kMinimumDpiScale = 0.5f;
inline constexpr float kMaximumDpiScale = 4.0f;
inline constexpr UINT kDefaultDpi = 96;

// Owns a kernel handle and closes it exactly once.
class UniqueHandle {
public:
    UniqueHandle() = default;

    explicit UniqueHandle(HANDLE handle) : handle_(handle) {}

    ~UniqueHandle() { reset(); }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.release()) {}

    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    HANDLE get() const { return handle_; }

    bool valid() const {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }

    HANDLE release() {
        const HANDLE handle = handle_;
        handle_ = nullptr;
        return handle;
    }

    void reset(HANDLE handle = nullptr) {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
        handle_ = handle;
    }

private:
    HANDLE handle_ = nullptr;
};

// Owns a GDI object and deletes it exactly once.
class UniqueGdiObject {
public:
    UniqueGdiObject() = default;

    explicit UniqueGdiObject(HGDIOBJ object) : object_(object) {}

    ~UniqueGdiObject() { reset(); }

    UniqueGdiObject(const UniqueGdiObject&) = delete;
    UniqueGdiObject& operator=(const UniqueGdiObject&) = delete;

    UniqueGdiObject(UniqueGdiObject&& other) noexcept
        : object_(other.release()) {}

    UniqueGdiObject& operator=(UniqueGdiObject&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    HGDIOBJ get() const { return object_; }

    bool valid() const { return object_ != nullptr; }

    HGDIOBJ release() {
        const HGDIOBJ object = object_;
        object_ = nullptr;
        return object;
    }

    void reset(HGDIOBJ object = nullptr) {
        if (object_ != nullptr) {
            DeleteObject(object_);
        }
        object_ = object;
    }

private:
    HGDIOBJ object_ = nullptr;
};

// A CURSORINFO that is zero-initialized with the required cbSize.
struct UniqueCursorInfo {
    CURSORINFO value{};

    UniqueCursorInfo() { value.cbSize = sizeof(value); }

    bool query() { return GetCursorInfo(&value) != FALSE; }

    bool visible() const {
        return (value.flags & CURSOR_SHOWING) != 0 && value.hCursor != nullptr;
    }
};

// A window handle is only usable while the window still exists.
inline bool IsValidWindow(HWND window) {
    return window != nullptr && IsWindow(window) != FALSE;
}

// Converts a monitor DPI to a clamped scale factor. 96 DPI maps to 1.0.
inline float DpiScaleForDpi(UINT dpi) {
    const UINT effectiveDpi = dpi == 0 ? kDefaultDpi : dpi;
    const float scale = static_cast<float>(effectiveDpi) /
                        static_cast<float>(kDefaultDpi);
    if (scale < kMinimumDpiScale) {
        return kMinimumDpiScale;
    }
    if (scale > kMaximumDpiScale) {
        return kMaximumDpiScale;
    }
    return scale;
}

}  // namespace yoko::win32
