#pragma once

// Direct2D, DirectWrite, WIC and shared GDI surfaces.

#include "platform.h"
#include "constants.h"
#include "context.h"
#include "icon_sources.h"
#include "math_utils.h"
#include "text_utils.h"
#include "types.h"

#include <commoncontrols.h>

inline D2D1_COLOR_F ToColorF(uint32_t rgb, float alpha) {
    return D2D1::ColorF(static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
                        static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
                        static_cast<float>(rgb & 0xFF) / 255.0f, alpha);
}

inline HBITMAP CreateHudDib(int width, int height) {
    if (width <= 0 || height <= 0) {
        return nullptr;
    }
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    return CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
}

inline void ReleaseIconCache() {
    for (auto& pair : context.iconCache) {
        if (pair.second != nullptr) {
            pair.second->Release();
        }
    }
    context.iconCache.clear();
    context.iconRetryTick.clear();
}

inline bool ConsumeDueIconRetries(DWORD now) {
    bool due = false;
    auto retry = context.iconRetryTick.begin();
    while (retry != context.iconRetryTick.end()) {
        if (now - retry->second < kIconRetryDelayMs) {
            ++retry;
            continue;
        }
        retry = context.iconRetryTick.erase(retry);
        due = true;
    }
    return due;
}

inline void ReleaseHudSurface() {
    ReleaseIconCache();
    if (context.hudBrush != nullptr) {
        context.hudBrush->Release();
        context.hudBrush = nullptr;
    }
    if (context.hudTarget != nullptr) {
        context.hudTarget->Release();
        context.hudTarget = nullptr;
    }
    if (context.hudDc != nullptr) {
        if (context.hudOriginalBitmap != nullptr) {
            SelectObject(context.hudDc, context.hudOriginalBitmap);
            context.hudOriginalBitmap = nullptr;
        }
        DeleteDC(context.hudDc);
        context.hudDc = nullptr;
    }
    if (context.hudBitmap != nullptr) {
        DeleteObject(context.hudBitmap);
        context.hudBitmap = nullptr;
    }
    context.hudWidth = 0;
    context.hudHeight = 0;
}

inline void ReleaseTextFormats() {
    if (context.titleFormat != nullptr) {
        context.titleFormat->Release();
        context.titleFormat = nullptr;
    }
    if (context.badgeFormat != nullptr) {
        context.badgeFormat->Release();
        context.badgeFormat = nullptr;
    }
}

inline void ReleaseTextResources() {
    ReleaseTextFormats();
    if (context.writeFactory != nullptr) {
        context.writeFactory->Release();
        context.writeFactory = nullptr;
    }
}

inline void ReleaseGraphics() {
    ReleaseHudSurface();
    ReleaseTextResources();
    if (context.wicFactory != nullptr) {
        context.wicFactory->Release();
        context.wicFactory = nullptr;
    }
    if (context.d2dFactory != nullptr) {
        context.d2dFactory->Release();
        context.d2dFactory = nullptr;
    }
}

inline bool CreateTitleFormat() {
    const float size = 13.0f * ChromeScale();
    const HRESULT result = context.writeFactory->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"",
        &context.titleFormat);
    if (FAILED(result)) {
        return false;
    }
    return SUCCEEDED(context.titleFormat->SetWordWrapping(
               DWRITE_WORD_WRAPPING_NO_WRAP)) &&
           SUCCEEDED(context.titleFormat->SetTextAlignment(
               DWRITE_TEXT_ALIGNMENT_LEADING)) &&
           SUCCEEDED(context.titleFormat->SetParagraphAlignment(
               DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
}

inline bool CreateBadgeFormat() {
    const float badgeSize = 11.0f * ChromeScale();
    const HRESULT result = context.writeFactory->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, badgeSize, L"", &context.badgeFormat);
    if (FAILED(result)) {
        return false;
    }
    return SUCCEEDED(context.badgeFormat->SetWordWrapping(
               DWRITE_WORD_WRAPPING_NO_WRAP)) &&
           SUCCEEDED(context.badgeFormat->SetTextAlignment(
               DWRITE_TEXT_ALIGNMENT_TRAILING)) &&
           SUCCEEDED(context.badgeFormat->SetParagraphAlignment(
               DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
}

inline bool CreateTextFormats() {
    ReleaseTextFormats();
    if (context.writeFactory == nullptr) {
        return false;
    }
    if (!CreateTitleFormat() || !CreateBadgeFormat()) {
        ReleaseTextFormats();
        return false;
    }
    return true;
}

inline bool CreateHudBackingStore(int width, int height) {
    context.hudDc = CreateCompatibleDC(nullptr);
    if (context.hudDc == nullptr) {
        return false;
    }
    context.hudBitmap = CreateHudDib(width, height);
    if (context.hudBitmap == nullptr) {
        return false;
    }
    const HGDIOBJ original = SelectObject(context.hudDc, context.hudBitmap);
    if (original == nullptr || original == HGDI_ERROR) {
        return false;
    }
    context.hudOriginalBitmap = original;
    return true;
}

inline bool CreateHudRenderTarget(int width, int height) {
    if (context.d2dFactory == nullptr) {
        return false;
    }
    const D2D1_RENDER_TARGET_PROPERTIES properties =
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                              D2D1_ALPHA_MODE_PREMULTIPLIED));
    const HRESULT result = context.d2dFactory->CreateDCRenderTarget(
        &properties, &context.hudTarget);
    if (FAILED(result) || context.hudTarget == nullptr) {
        return false;
    }
    const RECT bounds = {0, 0, width, height};
    if (FAILED(context.hudTarget->BindDC(context.hudDc, &bounds))) {
        return false;
    }
    return SUCCEEDED(context.hudTarget->CreateSolidColorBrush(
               D2D1::ColorF(D2D1::ColorF::White), &context.hudBrush)) &&
           context.hudBrush != nullptr;
}

inline bool CreateHudSurface(int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (context.hudTarget != nullptr && context.hudDc != nullptr &&
        context.hudBrush != nullptr && context.hudBitmap != nullptr &&
        context.hudWidth >= width && context.hudHeight >= height) {
        return true;
    }
    ReleaseHudSurface();
    if (!CreateHudBackingStore(width, height) ||
        !CreateHudRenderTarget(width, height)) {
        ReleaseHudSurface();
        return false;
    }
    context.hudWidth = width;
    context.hudHeight = height;
    return true;
}

inline bool EnsureGraphics() {
    if (context.d2dFactory == nullptr) {
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                     &context.d2dFactory))) {
            Wh_Log(L"Aleph: D2D1CreateFactory failed");
            return false;
        }
    }
    if (context.writeFactory == nullptr) {
        if (FAILED(DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(&context.writeFactory)))) {
            Wh_Log(L"Aleph: DWriteCreateFactory failed");
            return false;
        }
    }
    if (context.wicFactory == nullptr) {
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&context.wicFactory)))) {
            Wh_Log(L"Aleph: WIC factory creation failed");
            return false;
        }
    }
    if ((context.titleFormat == nullptr || context.badgeFormat == nullptr) &&
        !CreateTextFormats()) {
        Wh_Log(L"Aleph: text format creation failed");
        return false;
    }
    return true;
}

inline HICON QueryWindowIcon(HWND window) {
    DWORD_PTR iconResult = 0;
    const LRESULT sent = SendMessageTimeoutW(
        window, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 50,
        &iconResult);
    HICON icon = sent != 0 ? reinterpret_cast<HICON>(iconResult) : nullptr;
    if (icon == nullptr) {
        icon = reinterpret_cast<HICON>(
            GetClassLongPtrW(window, GCLP_HICON));
    }
    if (icon == nullptr) {
        icon = reinterpret_cast<HICON>(
            GetClassLongPtrW(window, GCLP_HICONSM));
    }
    return icon;
}

inline HICON QueryShellIconAtSize(int imageIndex, int listSize) {
    IImageList* images = nullptr;
    const HRESULT queried =
        SHGetImageList(listSize, IID_PPV_ARGS(&images));
    if (FAILED(queried) || images == nullptr) {
        if (images != nullptr) {
            images->Release();
        }
        return nullptr;
    }
    HICON icon = nullptr;
    const HRESULT result = images->GetIcon(imageIndex, ILD_TRANSPARENT, &icon);
    images->Release();
    if (FAILED(result)) {
        if (icon != nullptr) {
            DestroyIcon(icon);
        }
        return nullptr;
    }
    return icon;
}

inline HICON QueryShellIcon(const std::wstring& exePath) {
    if (exePath.empty()) {
        return nullptr;
    }
    SHFILEINFOW info = {};
    if (SHGetFileInfoW(exePath.c_str(), 0, &info, sizeof(info),
                       SHGFI_SYSICONINDEX) == 0) {
        return nullptr;
    }
    return QueryFirstAvailableIcon(
        kShellIconListSizes,
        [&](int listSize) {
            return QueryShellIconAtSize(info.iIcon, listSize);
        });
}

inline ID2D1Bitmap* CreateHudIconBitmap(IWICBitmapSource* bitmap) {
    IWICBitmapSource* converted = nullptr;
    const HRESULT conversion = WICConvertBitmapSource(
        GUID_WICPixelFormat32bppPBGRA, bitmap, &converted);
    if (FAILED(conversion) || converted == nullptr) {
        if (converted != nullptr) {
            converted->Release();
        }
        return nullptr;
    }
    ID2D1Bitmap* result = nullptr;
    const HRESULT created = context.hudTarget->CreateBitmapFromWicBitmap(
        converted, nullptr, &result);
    converted->Release();
    if (FAILED(created)) {
        if (result != nullptr) {
            result->Release();
        }
        return nullptr;
    }
    return result;
}

inline ID2D1Bitmap* CreateIconBitmap(HICON icon) {
    if (icon == nullptr || context.wicFactory == nullptr ||
        context.hudTarget == nullptr) {
        return nullptr;
    }
    IWICBitmap* bitmap = nullptr;
    const HRESULT converted =
        context.wicFactory->CreateBitmapFromHICON(icon, &bitmap);
    if (FAILED(converted) || bitmap == nullptr) {
        if (bitmap != nullptr) {
            bitmap->Release();
        }
        return nullptr;
    }
    ID2D1Bitmap* result = CreateHudIconBitmap(bitmap);
    bitmap->Release();
    return result;
}

inline ID2D1Bitmap* LoadShellIconBitmap(const std::wstring& exePath) {
    HICON icon = QueryShellIcon(exePath);
    if (icon == nullptr) {
        return nullptr;
    }
    ID2D1Bitmap* bitmap = CreateIconBitmap(icon);
    DestroyIcon(icon);
    return bitmap;
}

inline ID2D1Bitmap* LoadIconBitmap(const std::wstring& exePath,
                                   const std::wstring& windowClass,
                                   HWND window) {
    if (context.hudTarget == nullptr || context.wicFactory == nullptr) {
        return nullptr;
    }
    const std::wstring key = exePath + L"|" + windowClass;
    const auto cached = context.iconCache.find(key);
    if (cached != context.iconCache.end() && cached->second != nullptr) {
        return cached->second;
    }
    const auto retry = context.iconRetryTick.find(key);
    if (retry != context.iconRetryTick.end() &&
        GetTickCount() - retry->second < kIconRetryDelayMs) {
        return nullptr;
    }

    // The shell icon is the largest source, so it wins whenever it exists; the
    // window icon covers executables the shell cannot resolve.
    ID2D1Bitmap* result = LoadShellIconBitmap(exePath);
    if (result == nullptr) {
        result = CreateIconBitmap(QueryWindowIcon(window));
    }
    if (result == nullptr) {
        context.iconRetryTick[key] = GetTickCount();
        return nullptr;
    }
    context.iconCache[key] = result;
    context.iconRetryTick.erase(key);
    return result;
}
