#pragma once

// Header-only Win32 ownership and validation helpers.
//
// This file is inlined by tools/bundle.py, so it must stay self-contained and
// must never include another repository header.

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <utility>

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

// Reads bytes from this process through the documented API instead of
// dereferencing a pointer that may not be mapped.
inline bool TryReadSelf(const void* address, std::size_t size, void* buffer) {
    if (address == nullptr || buffer == nullptr || size == 0) {
        return false;
    }
    SIZE_T read = 0;
    const BOOL result = ReadProcessMemory(GetCurrentProcess(), address, buffer,
                                          size, &read);
    return result != FALSE && read == size;
}

// True when the whole range is committed and readable in this process.
inline bool IsReadableRange(const void* address, std::size_t size) {
    if (address == nullptr || size == 0) {
        return false;
    }
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(address);
    if (size > UINTPTR_MAX - start) {
        return false;
    }
    const std::uintptr_t end = start + size;
    std::uintptr_t current = start;
    while (current < end) {
        MEMORY_BASIC_INFORMATION info = {};
        if (VirtualQuery(reinterpret_cast<const void*>(current), &info,
                         sizeof(info)) != sizeof(info)) {
            return false;
        }
        if (info.State != MEM_COMMIT) {
            return false;
        }
        if ((info.Protect & PAGE_GUARD) != 0 ||
            (info.Protect & PAGE_NOACCESS) != 0) {
            return false;
        }
        const std::uintptr_t regionStart =
            reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        if (info.RegionSize > UINTPTR_MAX - regionStart) {
            return false;
        }
        const std::uintptr_t regionEnd = regionStart + info.RegionSize;
        if (regionEnd <= current) {
            return false;
        }
        current = regionEnd;
    }
    return true;
}

// Resolves the entry point of a loaded module after validating the PE
// headers. Returns false for any truncated or inconsistent image.
inline bool FindModuleEntryPoint(HMODULE module, void** entryPoint) {
    if (entryPoint == nullptr) {
        return false;
    }
    *entryPoint = nullptr;
    if (module == nullptr) {
        return false;
    }

    const BYTE* const base = reinterpret_cast<const BYTE*>(module);

    IMAGE_DOS_HEADER dosHeader = {};
    if (!TryReadSelf(base, sizeof(dosHeader), &dosHeader)) {
        return false;
    }
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE || dosHeader.e_lfanew <= 0) {
        return false;
    }

    const std::uintptr_t baseValue = reinterpret_cast<std::uintptr_t>(base);
    const std::uintptr_t ntOffset = static_cast<std::uintptr_t>(
        static_cast<DWORD>(dosHeader.e_lfanew));
    if (ntOffset > UINTPTR_MAX - baseValue) {
        return false;
    }
    const std::uintptr_t ntAddress = baseValue + ntOffset;
    const BYTE* const ntBase = reinterpret_cast<const BYTE*>(ntAddress);

    DWORD signature = 0;
    if (!TryReadSelf(ntBase, sizeof(signature), &signature)) {
        return false;
    }
    if (signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    constexpr std::uintptr_t kOptionalHeaderOffset =
        offsetof(IMAGE_NT_HEADERS32, OptionalHeader);
    if (kOptionalHeaderOffset > UINTPTR_MAX - ntAddress) {
        return false;
    }
    const std::uintptr_t optionalAddress = ntAddress + kOptionalHeaderOffset;
    const BYTE* const optionalBase =
        reinterpret_cast<const BYTE*>(optionalAddress);
    WORD optionalMagic = 0;
    if (!TryReadSelf(optionalBase, sizeof(optionalMagic), &optionalMagic)) {
        return false;
    }

    DWORD sizeOfImage = 0;
    DWORD addressOfEntryPoint = 0;
    if (optionalMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        IMAGE_NT_HEADERS64 headers = {};
        if (!TryReadSelf(ntBase, sizeof(headers), &headers)) {
            return false;
        }
        sizeOfImage = headers.OptionalHeader.SizeOfImage;
        addressOfEntryPoint = headers.OptionalHeader.AddressOfEntryPoint;
    } else if (optionalMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        IMAGE_NT_HEADERS32 headers = {};
        if (!TryReadSelf(ntBase, sizeof(headers), &headers)) {
            return false;
        }
        sizeOfImage = headers.OptionalHeader.SizeOfImage;
        addressOfEntryPoint = headers.OptionalHeader.AddressOfEntryPoint;
    } else {
        return false;
    }

    if (addressOfEntryPoint == 0 || addressOfEntryPoint >= sizeOfImage) {
        return false;
    }

    if (addressOfEntryPoint > UINTPTR_MAX - baseValue) {
        return false;
    }
    const BYTE* const entry =
        reinterpret_cast<const BYTE*>(baseValue + addressOfEntryPoint);
    if (!IsReadableRange(entry, 1)) {
        return false;
    }
    *entryPoint = const_cast<BYTE*>(entry);
    return true;
}

}  // namespace yoko::win32
