#pragma once

// PE entry-point lookup for mods that use the pasted tool-mod launcher.
//
// This file is inlined by tools/bundle.py, so it must stay self-contained and
// must never include another repository header.

#include <windows.h>

#include <cstddef>
#include <cstdint>

namespace yoko::win32 {

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
