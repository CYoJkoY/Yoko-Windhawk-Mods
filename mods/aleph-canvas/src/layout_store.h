#pragma once

// Crash-safe JSON persistence for card positions.

#include "platform.h"
#include <cerrno>
#include <limits>
#include <locale.h>
#include <shlobj.h>
#include "context.h"
#include "settings.h"
#include "text_utils.h"
#include "types.h"

static std::wstring EscapeJsonText(const std::wstring& value) {
    std::wstring result;
    for (const wchar_t character : value) {
        if (character == L'"' || character == L'\\') {
            result.push_back(L'\\');
            result.push_back(character);
            continue;
        }
        if (character == L'\n') {
            result += L"\\n";
            continue;
        }
        if (character < 0x20) {
            static constexpr wchar_t hexDigits[] = L"0123456789abcdef";
            result += L"\\u00";
            result.push_back(hexDigits[(character >> 4) & 0xF]);
            result.push_back(hexDigits[character & 0xF]);
            continue;
        }
        result.push_back(character);
    }
    return result;
}

static bool WriteAll(HANDLE file, const std::string& data) {
    size_t offset = 0;
    while (offset < data.size()) {
        const DWORD chunk = static_cast<DWORD>(
            std::min<size_t>(data.size() - offset, 1u << 20));
        DWORD written = 0;
        if (!WriteFile(file, data.data() + offset, chunk, &written, nullptr) ||
            written == 0) {
            return false;
        }
        offset += written;
    }
    return true;
}

static bool ReadWholeFile(const std::wstring& path, std::string& out) {
    yoko::win32::UniqueHandle file(CreateFileW(
        path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid()) {
        return false;
    }
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file.get(), &size) || size.QuadPart <= 0 ||
        size.QuadPart > (16 << 20)) {
        return false;
    }
    out.assign(static_cast<size_t>(size.QuadPart), '\0');
    size_t offset = 0;
    while (offset < out.size()) {
        const DWORD chunk = static_cast<DWORD>(
            std::min<size_t>(out.size() - offset, 1u << 20));
        DWORD read = 0;
        if (!ReadFile(file.get(), out.data() + offset, chunk, &read, nullptr) ||
            read == 0) {
            return false;
        }
        offset += read;
    }
    return true;
}

static bool IsLayoutWorldValid(const RectF& world) {
    if (!std::isfinite(world.left) || !std::isfinite(world.top) ||
        !std::isfinite(world.right) || !std::isfinite(world.bottom)) {
        return false;
    }
    return std::isfinite(world.width()) && std::isfinite(world.height()) &&
           world.width() > 0.0f && world.height() > 0.0f;
}

static bool FormatLayoutNumbers(const RectF& world, std::string& output) {
    if (!IsLayoutWorldValid(world)) {
        return false;
    }
    _locale_t locale = _create_locale(LC_NUMERIC, "C");
    if (locale == nullptr) {
        return false;
    }
    char numbers[192] = {};
    const int written = _snprintf_l(
        numbers, sizeof(numbers), "\"x\":%.9g,\"y\":%.9g,\"w\":%.9g,\"h\":%.9g",
        locale, static_cast<double>(world.left),
        static_cast<double>(world.top), static_cast<double>(world.width()),
        static_cast<double>(world.height()));
    _free_locale(locale);
    if (written < 0 || static_cast<size_t>(written) >= sizeof(numbers)) {
        return false;
    }
    output.assign(numbers, static_cast<size_t>(written));
    return true;
}

static std::string SerializeLayout(const LayoutSnapshot& layout) {
    std::string output = "{\"version\":1,\"entries\":[";
    bool first = true;
    for (const LayoutEntry& entry : layout.entries) {
        if (entry.fingerprint.empty()) {
            continue;
        }
        std::string numbers;
        if (!FormatLayoutNumbers(entry.world, numbers)) {
            return std::string();
        }
        if (!first) {
            output += ",";
        }
        first = false;
        output += "{\"key\":\"";
        output += ToUtf8(EscapeJsonText(entry.fingerprint));
        output += "\",";
        output += numbers;
        if (entry.pinned) {
            output += ",\"pin\":1";
        }
        if (!entry.grouped) {
            output += ",\"is_grouped\":0";
        }
        if (!entry.stackId.empty()) {
            output += ",\"stack\":\"";
            output += ToUtf8(EscapeJsonText(entry.stackId));
            output += "\"";
        }
        output += "}";
    }
    output += "]}\n";
    return output;
}

static bool IsJsonDigit(char character) {
    return character >= '0' && character <= '9';
}

static constexpr int kMaxJsonDepth = 64;

static void SkipJsonSpace(const std::string& text, size_t& index) {
    while (index < text.size() && (text[index] == ' ' || text[index] == '\t' ||
                                   text[index] == '\n' || text[index] == '\r')) {
        ++index;
    }
}

static bool ReadJsonHexQuad(const std::string& text, size_t& index,
                            uint32_t& value) {
    if (index + 4 > text.size()) {
        return false;
    }
    value = 0;
    for (int digitIndex = 0; digitIndex < 4; ++digitIndex) {
        const char character = text[index];
        uint32_t digit = 0;
        if (character >= '0' && character <= '9') {
            digit = static_cast<uint32_t>(character - '0');
        } else if (character >= 'a' && character <= 'f') {
            digit = static_cast<uint32_t>(character - 'a' + 10);
        } else if (character >= 'A' && character <= 'F') {
            digit = static_cast<uint32_t>(character - 'A' + 10);
        } else {
            return false;
        }
        value = value * 16 + digit;
        ++index;
    }
    return true;
}

static bool AppendJsonCodePoint(std::string& output, uint32_t codePoint) {
    if (codePoint <= 0x7F) {
        output.push_back(static_cast<char>(codePoint));
        return true;
    }
    if (codePoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        return true;
    }
    if (codePoint >= 0xD800 && codePoint <= 0xDFFF) {
        return false;
    }
    if (codePoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        output.push_back(
            static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        return true;
    }
    if (codePoint > 0x10FFFF) {
        return false;
    }
    output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
    output.push_back(
        static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    return true;
}

static bool ReadJsonUnicodeEscape(const std::string& text, size_t& index,
                                  std::string& output) {
    uint32_t codePoint = 0;
    if (!ReadJsonHexQuad(text, index, codePoint)) {
        return false;
    }
    if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
        if (index + 2 > text.size() || text[index] != '\\' ||
            text[index + 1] != 'u') {
            return false;
        }
        index += 2;
        uint32_t lowSurrogate = 0;
        if (!ReadJsonHexQuad(text, index, lowSurrogate)) {
            return false;
        }
        if (lowSurrogate < 0xDC00 || lowSurrogate > 0xDFFF) {
            return false;
        }
        codePoint = 0x10000 + ((codePoint - 0xD800) << 10) +
                    (lowSurrogate - 0xDC00);
    } else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
        return false;
    }
    return AppendJsonCodePoint(output, codePoint);
}

static bool ReadJsonEscape(const std::string& text, size_t& index,
                           std::string& output) {
    if (index >= text.size()) {
        return false;
    }
    const char escape = text[index];
    ++index;
    switch (escape) {
        case '"':
            output.push_back('"');
            return true;
        case '\\':
            output.push_back('\\');
            return true;
        case '/':
            output.push_back('/');
            return true;
        case 'b':
            output.push_back('\b');
            return true;
        case 'f':
            output.push_back('\f');
            return true;
        case 'n':
            output.push_back('\n');
            return true;
        case 'r':
            output.push_back('\r');
            return true;
        case 't':
            output.push_back('\t');
            return true;
        case 'u':
            return ReadJsonUnicodeEscape(text, index, output);
        default:
            return false;
    }
}

static bool ReadJsonString(const std::string& text, size_t& index,
                           std::string& output) {
    if (index >= text.size() || text[index] != '"') {
        return false;
    }
    ++index;
    output.clear();
    while (index < text.size() && text[index] != '"') {
        const char character = text[index];
        const bool escaped = character == '\\';
        if (escaped) {
            ++index;
        }
        if (escaped && !ReadJsonEscape(text, index, output)) {
            return false;
        }
        if (escaped) {
            continue;
        }
        ++index;
        if (static_cast<unsigned char>(character) < 0x20) {
            return false;
        }
        output.push_back(character);
    }
    if (index >= text.size()) {
        return false;
    }
    ++index;
    return true;
}

static bool ReadJsonDigits(const std::string& text, size_t& index) {
    const size_t start = index;
    while (index < text.size() && IsJsonDigit(text[index])) {
        ++index;
    }
    return index > start;
}

static bool ReadJsonInteger(const std::string& text, size_t& index) {
    if (index < text.size() && text[index] == '-') {
        ++index;
    }
    if (index >= text.size()) {
        return false;
    }
    if (text[index] != '0') {
        return ReadJsonDigits(text, index);
    }
    ++index;
    return true;
}

static bool ReadJsonFraction(const std::string& text, size_t& index) {
    if (index < text.size() && text[index] == '.') {
        ++index;
        return ReadJsonDigits(text, index);
    }
    return true;
}

static bool ReadJsonExponent(const std::string& text, size_t& index) {
    if (index >= text.size() ||
        (text[index] != 'e' && text[index] != 'E')) {
        return true;
    }
    ++index;
    if (index < text.size() &&
        (text[index] == '+' || text[index] == '-')) {
        ++index;
    }
    return ReadJsonDigits(text, index);
}

static bool ReadJsonNumber(const std::string& text, size_t& index,
                           double& value) {
    const size_t start = index;
    if (!ReadJsonInteger(text, index) || !ReadJsonFraction(text, index) ||
        !ReadJsonExponent(text, index)) {
        return false;
    }
    _locale_t locale = _create_locale(LC_NUMERIC, "C");
    if (locale == nullptr) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const double parsed = _strtod_l(text.c_str() + start, &end, locale);
    const bool valid = end == text.c_str() + index && errno != ERANGE &&
                       std::isfinite(parsed);
    _free_locale(locale);
    if (!valid) {
        return false;
    }
    value = parsed;
    return true;
}

static bool ConsumeJsonLiteral(const std::string& text, size_t& index,
                               const char* literal) {
    for (size_t offset = 0; literal[offset] != '\0'; ++offset) {
        if (index >= text.size() || text[index] != literal[offset]) {
            return false;
        }
        ++index;
    }
    return true;
}

static bool SkipJsonValue(const std::string& text, size_t& index, int depth);

template <typename FieldReader>
static bool ReadJsonObject(const std::string& text, size_t& index,
                           FieldReader readField) {
    SkipJsonSpace(text, index);
    if (index >= text.size() || text[index] != '{') {
        return false;
    }
    ++index;
    SkipJsonSpace(text, index);
    if (index < text.size() && text[index] == '}') {
        ++index;
        return true;
    }
    while (true) {
        std::string key;
        if (!ReadJsonString(text, index, key)) {
            return false;
        }
        SkipJsonSpace(text, index);
        if (index >= text.size() || text[index] != ':') {
            return false;
        }
        ++index;
        SkipJsonSpace(text, index);
        if (!readField(key)) {
            return false;
        }
        SkipJsonSpace(text, index);
        if (index >= text.size()) {
            return false;
        }
        const char delimiter = text[index];
        ++index;
        if (delimiter == '}') {
            return true;
        }
        if (delimiter != ',') {
            return false;
        }
        SkipJsonSpace(text, index);
    }
}

static bool SkipJsonObject(const std::string& text, size_t& index, int depth) {
    if (depth > kMaxJsonDepth) {
        return false;
    }
    return ReadJsonObject(text, index, [&](const std::string&) {
        return SkipJsonValue(text, index, depth + 1);
    });
}

static bool SkipJsonArray(const std::string& text, size_t& index, int depth) {
    if (depth > kMaxJsonDepth || index >= text.size() || text[index] != '[') {
        return false;
    }
    ++index;
    SkipJsonSpace(text, index);
    if (index < text.size() && text[index] == ']') {
        ++index;
        return true;
    }
    while (true) {
        if (!SkipJsonValue(text, index, depth + 1)) {
            return false;
        }
        SkipJsonSpace(text, index);
        if (index >= text.size()) {
            return false;
        }
        const char delimiter = text[index];
        ++index;
        if (delimiter == ']') {
            return true;
        }
        if (delimiter != ',') {
            return false;
        }
        SkipJsonSpace(text, index);
    }
}

static bool SkipJsonValue(const std::string& text, size_t& index, int depth) {
    if (depth > kMaxJsonDepth) {
        return false;
    }
    SkipJsonSpace(text, index);
    if (index >= text.size()) {
        return false;
    }
    const char character = text[index];
    if (character == '"') {
        std::string ignored;
        return ReadJsonString(text, index, ignored);
    }
    if (character == '{') {
        return SkipJsonObject(text, index, depth);
    }
    if (character == '[') {
        return SkipJsonArray(text, index, depth);
    }
    if (character == 't') {
        return ConsumeJsonLiteral(text, index, "true");
    }
    if (character == 'f') {
        return ConsumeJsonLiteral(text, index, "false");
    }
    if (character == 'n') {
        return ConsumeJsonLiteral(text, index, "null");
    }
    double ignored = 0.0;
    return ReadJsonNumber(text, index, ignored);
}

struct JsonEntryFields {
    std::wstring fingerprint;
    std::wstring stackId;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    double pin = 0.0;
    double grouped = 1.0;
    bool hasFingerprint = false;
    bool hasStack = false;
    bool hasX = false;
    bool hasY = false;
    bool hasWidth = false;
    bool hasHeight = false;
    bool hasPin = false;
    bool hasGrouped = false;
};

struct JsonEntryCursor {
    const std::string& text;
    size_t& index;
    JsonEntryFields& fields;
};

static bool ReadJsonEntryNumber(JsonEntryCursor& cursor, bool& assigned,
                                double& value) {
    if (assigned || !ReadJsonNumber(cursor.text, cursor.index, value)) {
        return false;
    }
    assigned = true;
    return true;
}

static bool ReadJsonEntryField(JsonEntryCursor& cursor,
                               const std::string& key) {
    if (key == "key") {
        if (cursor.fields.hasFingerprint) {
            return false;
        }
        std::string encoded;
        if (!ReadJsonString(cursor.text, cursor.index, encoded)) {
            return false;
        }
        cursor.fields.fingerprint = FromUtf8(encoded);
        if (cursor.fields.fingerprint.empty()) {
            return false;
        }
        cursor.fields.hasFingerprint = true;
        return true;
    }
    if (key == "x") {
        return ReadJsonEntryNumber(cursor, cursor.fields.hasX,
                                   cursor.fields.x);
    }
    if (key == "y") {
        return ReadJsonEntryNumber(cursor, cursor.fields.hasY,
                                   cursor.fields.y);
    }
    if (key == "w") {
        return ReadJsonEntryNumber(cursor, cursor.fields.hasWidth,
                                   cursor.fields.width);
    }
    if (key == "h") {
        return ReadJsonEntryNumber(cursor, cursor.fields.hasHeight,
                                   cursor.fields.height);
    }
    if (key == "pin") {
        return ReadJsonEntryNumber(cursor, cursor.fields.hasPin,
                                   cursor.fields.pin);
    }
    if (key == "is_grouped") {
        return ReadJsonEntryNumber(cursor, cursor.fields.hasGrouped,
                                   cursor.fields.grouped);
    }
    if (key == "stack") {
        if (cursor.fields.hasStack) {
            return false;
        }
        std::string encoded;
        if (!ReadJsonString(cursor.text, cursor.index, encoded)) {
            return false;
        }
        cursor.fields.stackId = FromUtf8(encoded);
        if (cursor.fields.stackId.empty()) {
            return false;
        }
        cursor.fields.hasStack = true;
        return true;
    }
    return SkipJsonValue(cursor.text, cursor.index, 2);
}

static bool IsLayoutNumberInRange(double value) {
    constexpr double maximum = std::numeric_limits<float>::max();
    return std::isfinite(value) && value >= -maximum && value <= maximum;
}

static bool BuildLayoutEntry(const JsonEntryFields& fields, LayoutEntry& entry) {
    if (!fields.hasFingerprint || !fields.hasX || !fields.hasY ||
        !fields.hasWidth || !fields.hasHeight) {
        return false;
    }
    if (!IsLayoutNumberInRange(fields.x) ||
        !IsLayoutNumberInRange(fields.y) ||
        !IsLayoutNumberInRange(fields.width) ||
        !IsLayoutNumberInRange(fields.height)) {
        return false;
    }
    if (fields.hasPin && fields.pin != 0.0 && fields.pin != 1.0) {
        return false;
    }
    if (fields.hasGrouped && fields.grouped != 0.0 && fields.grouped != 1.0) {
        return false;
    }
    const float left = static_cast<float>(fields.x);
    const float top = static_cast<float>(fields.y);
    const float width = static_cast<float>(fields.width);
    const float height = static_cast<float>(fields.height);
    const float right = left + width;
    const float bottom = top + height;
    const RectF world{left, top, right, bottom};
    if (!IsLayoutWorldValid(world)) {
        return false;
    }
    entry.fingerprint = fields.fingerprint;
    entry.world = world;
    entry.pinned = fields.hasPin && fields.pin == 1.0;
    entry.grouped = !fields.hasGrouped || fields.grouped == 1.0;
    entry.stackId = fields.hasStack ? fields.stackId : std::wstring();
    return true;
}

static bool ReadJsonEntryObject(const std::string& text, size_t& index,
                                LayoutEntry& entry) {
    JsonEntryFields fields;
    JsonEntryCursor cursor{text, index, fields};
    const bool parsed = ReadJsonObject(text, index, [&](const std::string& key) {
        return ReadJsonEntryField(cursor, key);
    });
    if (!parsed) {
        return false;
    }
    return BuildLayoutEntry(fields, entry);
}

static bool ReadJsonEntries(const std::string& text, size_t& index,
                            LayoutSnapshot& layout) {
    SkipJsonSpace(text, index);
    if (index >= text.size() || text[index] != '[') {
        return false;
    }
    ++index;
    SkipJsonSpace(text, index);
    if (index < text.size() && text[index] == ']') {
        ++index;
        return true;
    }
    while (true) {
        LayoutEntry entry;
        if (!ReadJsonEntryObject(text, index, entry)) {
            return false;
        }
        if (layout.entries.size() <
            static_cast<size_t>(kMaxLayoutEntries)) {
            layout.entries.push_back(std::move(entry));
        }
        SkipJsonSpace(text, index);
        if (index >= text.size()) {
            return false;
        }
        const char delimiter = text[index];
        ++index;
        if (delimiter == ']') {
            return true;
        }
        if (delimiter != ',') {
            return false;
        }
        SkipJsonSpace(text, index);
    }
}

struct JsonRootFields {
    LayoutSnapshot& layout;
    bool hasVersion = false;
    bool hasEntries = false;
    double version = 0.0;
};

struct JsonRootCursor {
    const std::string& text;
    size_t& index;
    JsonRootFields& fields;
};

static bool ReadJsonRootField(JsonRootCursor& cursor,
                              const std::string& key) {
    if (key == "version") {
        if (cursor.fields.hasVersion ||
            !ReadJsonNumber(cursor.text, cursor.index,
                            cursor.fields.version)) {
            return false;
        }
        cursor.fields.hasVersion = true;
        return true;
    }
    if (key == "entries") {
        if (cursor.fields.hasEntries ||
            !ReadJsonEntries(cursor.text, cursor.index,
                             cursor.fields.layout)) {
            return false;
        }
        cursor.fields.hasEntries = true;
        return true;
    }
    return SkipJsonValue(cursor.text, cursor.index, 1);
}

static bool ParseLayoutDocument(const std::string& text,
                                LayoutSnapshot& layout) {
    if (FromUtf8(text).empty()) {
        return false;
    }
    size_t index = 0;
    JsonRootFields fields{layout};
    JsonRootCursor cursor{text, index, fields};
    const bool parsed = ReadJsonObject(text, index, [&](const std::string& key) {
        return ReadJsonRootField(cursor, key);
    });
    if (!parsed) {
        return false;
    }
    SkipJsonSpace(text, index);
    return index == text.size() && fields.hasVersion &&
           fields.hasEntries && fields.version == 1.0;
}

static LayoutSnapshot ParseLayout(const std::string& text) {
    LayoutSnapshot layout;
    if (!ParseLayoutDocument(text, layout)) {
        return LayoutSnapshot{};
    }
    return layout;
}

static void LoadLayout() {
    context.layout.entries.clear();
    if (!context.settings.saveLayout) {
        return;
    }
    const std::wstring path = LayoutFilePath();
    if (path.empty()) {
        return;
    }
    std::string text;
    if (!ReadWholeFile(path, text)) {
        return;
    }
    context.layout = ParseLayout(text);
}

static bool EnsureLayoutDirectory(const std::wstring& directory) {
    if (directory.empty()) {
        return false;
    }
    const int result =
        SHCreateDirectoryExW(nullptr, directory.c_str(), nullptr);
    if (result != ERROR_SUCCESS && result != ERROR_ALREADY_EXISTS) {
        SetLastError(static_cast<DWORD>(result));
        return false;
    }
    const DWORD attributes = GetFileAttributesW(directory.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        SetLastError(ERROR_DIRECTORY);
        return false;
    }
    return true;
}

static bool SaveLayoutNow() {
    if (!context.settings.saveLayout) {
        return false;
    }
    const std::wstring path = LayoutFilePath();
    if (path.empty()) {
        return false;
    }
    const std::wstring directory = LayoutDirectory();
    if (directory.empty()) {
        return false;
    }
    if (!EnsureLayoutDirectory(directory)) {
        Wh_Log(L"Aleph: cannot create %s: %lu", directory.c_str(),
               GetLastError());
        return false;
    }

    const std::string data = SerializeLayout(context.layout);
    if (data.empty()) {
        return false;
    }
    const std::wstring temporaryPath = path + L".tmp";
    {
        yoko::win32::UniqueHandle file(CreateFileW(
            temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!file.valid()) {
            Wh_Log(L"Aleph: cannot create %s", temporaryPath.c_str());
            return false;
        }
        if (!WriteAll(file.get(), data) || !FlushFileBuffers(file.get())) {
            Wh_Log(L"Aleph: cannot write %s", temporaryPath.c_str());
            return false;
        }
    }
    if (!MoveFileExW(temporaryPath.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        Wh_Log(L"Aleph: cannot replace %s: %lu", path.c_str(), GetLastError());
        return false;
    }
    return true;
}

static void MarkLayoutDirty(DWORD now) {
    if (!context.settings.saveLayout) {
        return;
    }
    context.layoutDirty = true;
    context.layoutDirtyTick = now;
}

static void FlushLayoutIfDue(DWORD now) {
    if (!context.layoutDirty) {
        return;
    }
    if (!context.settings.saveLayout) {
        context.layoutDirty = false;
        return;
    }
    if (now - context.layoutDirtyTick < kLayoutSaveDelayMs) {
        return;
    }
    if (SaveLayoutNow()) {
        context.layoutDirty = false;
        return;
    }
    context.layoutDirtyTick = now;
}
