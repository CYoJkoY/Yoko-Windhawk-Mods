#pragma once

// Header-only pattern scanner for loaded modules.
//
// The scanner reads through ReadProcessMemory, so it works for both the
// current process and another process, and it can never fault on an address
// that went away. Every section walk is bounded by the PE header sizes and by
// an explicit byte budget. This file is inlined by tools/bundle.py and must
// stay self-contained.

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace yoko::detail {

struct PatternByte {
    std::uint8_t value;
    bool wildcard;
};

inline int HexDigitValue(wchar_t character) {
    if (character >= L'0' && character <= L'9') {
        return static_cast<int>(character - L'0');
    }
    if (character >= L'a' && character <= L'f') {
        return static_cast<int>(character - L'a') + 10;
    }
    if (character >= L'A' && character <= L'F') {
        return static_cast<int>(character - L'A') + 10;
    }
    return -1;
}

inline bool IsPatternSpace(wchar_t character) {
    return character == L' ' || character == L'\t' || character == L'\n' ||
           character == L'\r';
}

// Parses an IDA-style pattern such as L"48 8B ?? 05". Both "?" and "??" are
// accepted for a wildcard byte. Returns false for empty or malformed input.
inline bool ParsePattern(std::wstring_view text,
                         std::vector<PatternByte>& pattern) {
    pattern.clear();
    std::size_t index = 0;
    while (index < text.size()) {
        const wchar_t character = text[index];
        if (IsPatternSpace(character)) {
            ++index;
            continue;
        }
        if (character == L'?') {
            pattern.push_back({0, true});
            ++index;
            if (index < text.size() && text[index] == L'?') {
                ++index;
            }
            continue;
        }
        const int high = HexDigitValue(character);
        if (high < 0) {
            pattern.clear();
            return false;
        }
        ++index;
        if (index >= text.size()) {
            pattern.clear();
            return false;
        }
        const int low = HexDigitValue(text[index]);
        if (low < 0) {
            pattern.clear();
            return false;
        }
        ++index;
        pattern.push_back(
            {static_cast<std::uint8_t>((high << 4) | low), false});
    }
    return !pattern.empty();
}

inline bool MatchesAt(const std::uint8_t* candidate,
                      const std::vector<PatternByte>& pattern) {
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (!pattern[i].wildcard && candidate[i] != pattern[i].value) {
            return false;
        }
    }
    return true;
}

}  // namespace yoko::detail

namespace yoko {

// Scans one module inside a target process.
//
// Usage:
//   RemotePatternScanner scanner(GetCurrentProcess(),
//                                reinterpret_cast<std::uintptr_t>(
//                                    GetModuleHandle(L"user32.dll")));
//   std::uint8_t* hit = nullptr;
//   if (scanner.Find(L"48 8B ?? 05", &hit)) { ... }
class RemotePatternScanner {
public:
    RemotePatternScanner(HANDLE process, std::uintptr_t imageBase)
        : process_(process), imageBase_(imageBase) {}

    // Largest number of bytes the scanner will touch in one call. A pattern
    // that is not found inside this budget reports failure instead of walking
    // an unexpectedly large image.
    static constexpr std::size_t kDefaultByteBudget = 256u * 1024u * 1024u;

    bool Find(std::wstring_view patternText, std::uint8_t** match,
              std::size_t byteBudget = kDefaultByteBudget) const {
        if (match == nullptr) {
            return false;
        }
        *match = nullptr;
        if (process_ == nullptr || process_ == INVALID_HANDLE_VALUE ||
            imageBase_ == 0) {
            return false;
        }

        std::vector<detail::PatternByte> pattern;
        if (!detail::ParsePattern(patternText, pattern)) {
            return false;
        }
        if (pattern.size() > byteBudget) {
            return false;
        }

        const auto base = reinterpret_cast<const std::uint8_t*>(imageBase_);
        IMAGE_DOS_HEADER dosHeader = {};
        if (!ReadRemote(base, sizeof(dosHeader), &dosHeader)) {
            return false;
        }
        if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE || dosHeader.e_lfanew <= 0) {
            return false;
        }

        const std::uintptr_t ntOffset = static_cast<std::uintptr_t>(
            static_cast<DWORD>(dosHeader.e_lfanew));
        if (ntOffset > UINTPTR_MAX - imageBase_) {
            return false;
        }
        const auto ntBase = reinterpret_cast<const std::uint8_t*>(
            imageBase_ + ntOffset);

        DWORD signature = 0;
        if (!ReadRemote(ntBase, sizeof(signature), &signature)) {
            return false;
        }
        if (signature != IMAGE_NT_SIGNATURE) {
            return false;
        }

        constexpr std::size_t kOptionalHeaderOffset =
            offsetof(IMAGE_NT_HEADERS32, OptionalHeader);
        if (ntOffset > SIZE_MAX - kOptionalHeaderOffset) {
            return false;
        }
        const std::size_t optionalAddress = ntOffset + kOptionalHeaderOffset;
        if (optionalAddress > UINTPTR_MAX - imageBase_) {
            return false;
        }
        const auto optionalBase = reinterpret_cast<const std::uint8_t*>(
            imageBase_ + optionalAddress);
        WORD optionalMagic = 0;
        if (!ReadRemote(optionalBase, sizeof(optionalMagic), &optionalMagic)) {
            return false;
        }
        const bool pe32 = optionalMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
        const bool pe64 = optionalMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC;
        if (!pe32 && !pe64) {
            return false;
        }

        DWORD sectionCount = 0;
        DWORD sizeOfImage = 0;
        WORD sizeOfOptionalHeader = 0;
        if (pe64) {
            IMAGE_NT_HEADERS64 headers = {};
            if (!ReadRemote(ntBase, sizeof(headers), &headers)) {
                return false;
            }
            sectionCount = headers.FileHeader.NumberOfSections;
            sizeOfOptionalHeader = headers.FileHeader.SizeOfOptionalHeader;
            sizeOfImage = headers.OptionalHeader.SizeOfImage;
        } else {
            IMAGE_NT_HEADERS32 headers = {};
            if (!ReadRemote(ntBase, sizeof(headers), &headers)) {
                return false;
            }
            sectionCount = headers.FileHeader.NumberOfSections;
            sizeOfOptionalHeader = headers.FileHeader.SizeOfOptionalHeader;
            sizeOfImage = headers.OptionalHeader.SizeOfImage;
        }

        if (sectionCount == 0 || sectionCount > 96) {
            return false;
        }
        if (sizeOfImage == 0) {
            return false;
        }
        const std::size_t minimumOptionalSize =
            pe64 ? sizeof(IMAGE_OPTIONAL_HEADER64)
                 : sizeof(IMAGE_OPTIONAL_HEADER32);
        if (sizeOfOptionalHeader < minimumOptionalSize) {
            return false;
        }
        if (sizeOfImage > UINTPTR_MAX - imageBase_) {
            return false;
        }

        if (sizeOfOptionalHeader > SIZE_MAX - optionalAddress) {
            return false;
        }
        const std::size_t sectionTableOffset =
            optionalAddress + sizeOfOptionalHeader;
        const std::size_t sectionTableSize =
            sizeof(IMAGE_SECTION_HEADER) * sectionCount;
        if (sectionTableSize / sizeof(IMAGE_SECTION_HEADER) != sectionCount) {
            return false;
        }
        if (sectionTableOffset > sizeOfImage ||
            sectionTableSize > sizeOfImage - sectionTableOffset) {
            return false;
        }

        std::size_t remaining = byteBudget;
        std::vector<std::uint8_t> buffer;
        for (DWORD index = 0; index < sectionCount; ++index) {
            IMAGE_SECTION_HEADER section = {};
            const auto sectionAddress =
                reinterpret_cast<const std::uint8_t*>(imageBase_ +
                                                      sectionTableOffset +
                                                      index *
                                                          sizeof(section));
            if (!ReadRemote(sectionAddress, sizeof(section), &section)) {
                return false;
            }

            const DWORD regionStart = section.VirtualAddress;
            const DWORD regionSize = section.Misc.VirtualSize != 0
                                         ? section.Misc.VirtualSize
                                         : section.SizeOfRawData;
            if (regionSize == 0 || regionStart >= sizeOfImage ||
                regionSize > sizeOfImage - regionStart) {
                continue;
            }

            const std::size_t available =
                remaining < regionSize ? remaining : regionSize;
            if (available < pattern.size()) {
                continue;
            }

            buffer.resize(available);
            if (!ReadRemote(base + regionStart, available, buffer.data())) {
                // A partially unreadable section is skipped, not fatal.
                continue;
            }
            remaining -= available;

            for (std::size_t offset = 0;
                 offset + pattern.size() <= available; ++offset) {
                if (detail::MatchesAt(buffer.data() + offset, pattern)) {
                    *match = const_cast<std::uint8_t*>(base + regionStart +
                                                       offset);
                    return true;
                }
            }
            if (remaining < pattern.size()) {
                return false;
            }
        }
        return false;
    }

private:
    bool ReadRemote(const void* address, std::size_t size, void* buffer) const {
        if (address == nullptr || buffer == nullptr || size == 0) {
            return false;
        }
        SIZE_T read = 0;
        const BOOL result = ReadProcessMemory(process_, address, buffer, size,
                                              &read);
        return result != FALSE && read == size;
    }

    HANDLE process_ = nullptr;
    std::uintptr_t imageBase_ = 0;
};

}  // namespace yoko
