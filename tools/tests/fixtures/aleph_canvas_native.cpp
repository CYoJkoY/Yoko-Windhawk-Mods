#include "mods/aleph-canvas/src/cards.h"
#include "mods/aleph-canvas/src/card_stacks.h"
#include "mods/aleph-canvas/src/card_chrome.h"
#include "mods/aleph-canvas/src/desktop_pull.h"
#include "mods/aleph-canvas/src/icon_sources.h"
#include "mods/aleph-canvas/src/math_utils.h"
#include "mods/aleph-canvas/src/minimap.h"
#include "mods/aleph-canvas/src/layout_store.h"
#include "mods/aleph-canvas/src/render_hud.h"
#include "mods/aleph-canvas/src/resources.h"
#include "mods/aleph-canvas/src/settings.h"
#include "mods/aleph-canvas/src/windows.h"
#include "mods/aleph-canvas/src/interaction.h"
#include "mods/aleph-canvas/src/worker.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <type_traits>

CanvasContext context;

static constexpr float kTolerance = 0.002f;

static const GUID kCanvasDesktop = {0x11111111, 0x2222, 0x3333,
                                    {0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44}};
static const GUID kOtherDesktop = {0xAAAAAAAA, 0xBBBB, 0xCCCC,
                                   {0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD}};

// Test double for the public interface. Reference counting is irrelevant here,
// so every answer comes from a field that the case sets before calling the
// helper.
struct FakeDesktopManager final : IVirtualDesktopManager {
    HWND referenceHandle = reinterpret_cast<HWND>(0x1000);
    HWND windowHandle = reinterpret_cast<HWND>(0x2000);
    GUID referenceDesktop = kCanvasDesktop;
    GUID windowDesktop = kOtherDesktop;
    HRESULT referenceError = S_OK;
    HRESULT windowError = S_OK;
    HRESULT moveError = S_OK;
    unsigned int moveCalls = 0;
    HWND movedWindow = nullptr;
    GUID movedTarget = {};

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** object) override {
        if (object == nullptr) {
            return E_POINTER;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return 1;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        return 1;
    }

    HRESULT STDMETHODCALLTYPE IsWindowOnCurrentVirtualDesktop(
        HWND, BOOL* onCurrentDesktop) override {
        if (onCurrentDesktop == nullptr) {
            return E_POINTER;
        }
        *onCurrentDesktop = TRUE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetWindowDesktopId(HWND window,
                                                 GUID* desktopId) override {
        if (desktopId == nullptr) {
            return E_POINTER;
        }
        if (window == referenceHandle) {
            *desktopId = referenceDesktop;
            return referenceError;
        }
        if (window == windowHandle) {
            *desktopId = windowDesktop;
            return windowError;
        }
        return E_INVALIDARG;
    }

    HRESULT STDMETHODCALLTYPE MoveWindowToDesktop(HWND window,
                                                  REFGUID desktopId) override {
        ++moveCalls;
        movedWindow = window;
        movedTarget = desktopId;
        return moveError;
    }
};

static void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static void Near(float actual, float expected) {
    if (!std::isfinite(actual) || std::fabs(actual - expected) > kTolerance) {
        throw std::runtime_error("expected " + std::to_string(expected) +
                                 ", received " + std::to_string(actual));
    }
}

static void SameRect(const RectF& actual, const RectF& expected) {
    Near(actual.left, expected.left);
    Near(actual.top, expected.top);
    Near(actual.right, expected.right);
    Near(actual.bottom, expected.bottom);
}

static void AddCard(RectF world) {
    Card card;
    card.world = world;
    context.cards.push_back(std::move(card));
}

static void PrepareGeometry() {
    context.virtualScreen = RECT{-1920, -180, 1920, 1260};
    context.chromeDpi = 144;
    context.camera.zoom = 1.0f;
    context.camera.targetZoom = 1.0f;
    context.settings.snapDistance = 8;
}

static void CheckSnapDelta(float delta) {
    AddCard(MakeRectF(100.0f, 200.0f, 60.0f, 80.0f));
    const SnapResult result =
        ComputeSnap(MakeRectF(100.0f - delta, 200.0f - delta, 20.0f, 20.0f), -1);
    Near(result.left, 100.0f);
    Near(result.top, 200.0f);
    Require(result.snapVertical && result.snapHorizontal, "both axes must snap");
    Near(result.guideX, 100.0f);
    Near(result.guideY, 200.0f);
}

static void SnapPositive() {
    CheckSnapDelta(3.0f);
}

static void SnapNegative() {
    CheckSnapDelta(-3.0f);
}

static void SnapDisabled() {
    AddCard(MakeRectF(640.0f, -80.0f, 60.0f, 80.0f));
    context.settings.snapDistance = 0;
    const SnapResult result = ComputeSnap(MakeRectF(637.0f, -83.0f, 20.0f, 20.0f), -1);
    Near(result.left, 637.0f);
    Near(result.top, -83.0f);
    Require(!result.snapVertical && !result.snapHorizontal, "disabled snap emitted guides");
}

static void SnapUnmatched() {
    const RectF moving = MakeRectF(637.0f, -83.0f, 20.0f, 20.0f);
    AddCard(moving);
    AddCard(MakeRectF(-600.0f, 500.0f, 60.0f, 80.0f));
    const SnapResult result = ComputeSnap(moving, 0);
    Near(result.left, moving.left);
    Near(result.top, moving.top);
    Require(!result.snapVertical && !result.snapHorizontal, "self or distant card snapped");
}

static void CheckZoomAnchor(float initialZoom, float requestedZoom) {
    Camera camera;
    camera.centerX = -321.0f;
    camera.centerY = 177.0f;
    camera.zoom = initialZoom;
    const Vec2 pointer{231.0f, 1081.0f};
    const Vec2 anchor = ScreenToWorld(pointer, camera);
    ZoomAtPoint(camera, requestedZoom, pointer);
    AdvanceCamera(camera, 1.0f / 240.0f);
    Require(camera.zoom > std::min(initialZoom, requestedZoom) &&
            camera.zoom < std::max(initialZoom, requestedZoom), "no intermediate zoom frame");
    for (int frame = 0; frame < 120; ++frame) {
        const Vec2 current = ScreenToWorld(pointer, camera);
        Near(current.x, anchor.x);
        Near(current.y, anchor.y);
        AdvanceCamera(camera, frame % 2 == 0 ? 1.0f / 60.0f : 1.0f / 30.0f);
    }
    Near(camera.zoom, requestedZoom);
}

static void ZoomAnchor() {
    CheckZoomAnchor(0.6f, 2.2f);
    CheckZoomAnchor(2.2f, 0.6f);
}

static void MinimapCenter() {
    AddCard(MakeRectF(-800.0f, -300.0f, 1800.0f, 500.0f));
    context.minimapRect = MakeRectF(3010.0f, 800.0f, 260.0f, 210.0f);
    const RectF marker{130.0f, -75.0f, 130.0f, -75.0f};
    const RectF projected = MinimapWorldToScreen(marker, context.minimapRect, CanvasWorldBounds());
    context.camera.zoomAnchored = true;
    PanCameraFromMinimap(Vec2{projected.left, projected.top});
    Near(context.camera.targetCenterX, marker.left);
    Near(context.camera.targetCenterY, marker.top);
    Require(!context.camera.zoomAnchored, "minimap must end cursor-anchored zoom");
    context.camera.snapToTarget();
    const RectF centered = WorldToScreen(marker, context.camera);
    Near(centered.left, ScreenCenterX());
    Near(centered.top, ScreenCenterY());
}

static void MinimapEmpty() {
    context.minimapRect = MakeRectF(200.0f, 300.0f, 1.0f, 1.0f);
    context.camera.targetCenterX = 124.0f;
    context.camera.targetCenterY = -456.0f;
    PanCameraFromMinimap(Vec2{200.0f, 300.0f});
    Near(context.camera.targetCenterX, 124.0f);
    Near(context.camera.targetCenterY, -456.0f);
}

static void LayoutSaveDisabled() {
    context.settings.saveLayout = false;
    MarkLayoutDirty(1000);
    Require(!context.layoutDirty, "disabled layout saving scheduled a write");
    context.layoutDirty = true;
    context.layoutDirtyTick = 1000;
    FlushLayoutIfDue(1000 + kLayoutSaveDelayMs);
    Require(!context.layoutDirty, "disabled layout saving kept a pending write");
    context.settings.saveLayout = true;
    MarkLayoutDirty(2000);
    Require(context.layoutDirty, "enabled layout saving ignored a change");
}

static void JsonMalformed() {
    static const char* const documents[] = {
        R"({"version":1,"entries":[null]})",
        R"({"version":1,"entries":[{"key":"a","x":true,"y":0,"w":1,"h":1}]})",
        R"({"version":1,"entries":[{"key":"a","x":0,"y":0,"w":1,"h":1},null]})",
        R"({"version":1,"entries":[{"key":"a","x":0,"y":0,"w":1,"h":1},]})",
        R"({"version":1,"entries":[{"key":"a","x":0,"y":0,"w":1,"h":1})",
        R"({"version":1,"entries":[{"key":"a","x":0,"y":0,"w":1,"h":1}] trailing})",
        R"({"version":1,"entries":[{"key":"a","x":0,"y":0,"w":1,"h":1}]} trailing)",
        R"({"version":1,"entries":[{"key":"\uZZZZ","x":0,"y":0,"w":1,"h":1}]})",
        R"({"version":1,"entries":[{"key":"unterminated})",
        R"({"version":1,"entries":[{"key":"a","x":0,"y":0,"w":1,"h":1,"extra":[true,] }]})",
        "", "{", R"({"entries":)",
    };
    for (const char* document : documents) {
        Require(ParseLayout(document).entries.empty(), "malformed JSON accepted or partially loaded");
    }
}

static void JsonFinite() {
    static const char* const geometries[] = {
        R"("x":0,"y":0,"w":1e9999,"h":1)",
        R"("x":1e9999,"y":0,"w":1,"h":1)",
        R"("x":0,"y":-1e9999,"w":1,"h":1)",
        R"("x":0,"y":0,"w":1,"h":1e9999)",
        R"("x":0,"y":0,"w":1e100,"h":1)",
        R"("x":3e38,"y":0,"w":3e38,"h":1)",
        R"("x":0,"y":3e38,"w":1,"h":3e38)",
        R"("x":0,"y":0,"w":NaN,"h":1)",
        R"("x":0,"y":0,"w":Infinity,"h":1)",
        R"("x":0,"y":0,"w":0,"h":1)",
        R"("x":0,"y":0,"w":1,"h":-1)",
    };
    for (const char* geometry : geometries) {
        const std::string document = std::string(R"({"version":1,"entries":[{"key":"a",)") + geometry + "}]}";
        Require(ParseLayout(document).entries.empty(), "nonfinite or nonpositive geometry accepted");
    }
}

static void JsonOrder() {
    const std::array<std::string, 5> fields = {
        R"("key":"editor#1")", R"("x":-125.25)", R"("y":47.5)", R"("w":820.75)", R"("h":600.25)"};
    std::array<size_t, 5> order{0, 1, 2, 3, 4};
    do {
        std::string document = R"({"entries":[{)";
        for (size_t index = 0; index < order.size(); ++index) {
            document += (index == 0 ? "" : ",") + fields[order[index]];
        }
        document += R"(}],"extra":{"nested":[null,true,{"x":3}]},"version":1})";
        const LayoutSnapshot parsed = ParseLayout(document);
        Require(parsed.entries.size() == 1, "field order changed parser result");
        Require(parsed.entries[0].fingerprint == L"editor#1", "field order lost fingerprint");
        SameRect(parsed.entries[0].world, MakeRectF(-125.25f, 47.5f, 820.75f, 600.25f));
    } while (std::next_permutation(order.begin(), order.end()));
}

static void JsonRoundtrip() {
    const LayoutSnapshot original{{
        {L"app|\u4E2D\u6587\U0001F680\\\"\n#0", MakeRectF(-120.25f, 63.5f, 810.75f, 600.25f)},
        {L"second#1", MakeRectF(2160.5f, -840.25f, 1280.0f, 720.5f)},
    }};
    const std::string serialized = SerializeLayout(original);
    const LayoutSnapshot parsed = ParseLayout(serialized);
    Require(parsed.entries.size() == original.entries.size(), "roundtrip lost entries");
    for (size_t index = 0; index < original.entries.size(); ++index) {
        Require(parsed.entries[index].fingerprint == original.entries[index].fingerprint,
                "roundtrip lost escaped or Unicode key");
        SameRect(parsed.entries[index].world, original.entries[index].world);
    }
    Require(SerializeLayout(parsed) == serialized, "second serialization changed layout");
}

static void HotkeySuffix() {
    UINT modifiers = 0;
    UINT key = 0;
    Require(ParseHotkeyText(L" Ctrl + F12 ", modifiers, key), "valid F12 hotkey rejected");
    Require(modifiers == MOD_CONTROL && key == VK_F12, "valid F12 parsed incorrectly");
    static const wchar_t* const invalid[] = {
        L"Ctrl+F12garbage", L"Ctrl+F12.0", L"Ctrl+F0", L"Ctrl+F25",
        L"Ctrl+F9999999999999999999", L"Ctrl+F12+", L"Ctrl+F12+F1", L"Ctrl+"};
    for (const wchar_t* text : invalid) {
        modifiers = 999;
        key = 999;
        Require(!ParseHotkeyText(text, modifiers, key), "invalid function-key hotkey accepted");
        Require(modifiers == 0 && key == 0, "failed parse left partially usable hotkey");
    }
}

static ScannedWindow SampleWindow(UINT_PTR handle) {
    ScannedWindow window;
    window.hwnd = reinterpret_cast<HWND>(handle);
    window.processId = 42;
    window.threadId = 43;
    window.windowClass = L"sample-window";
    window.exePath = L"C:\\sample\\editor.exe";
    window.applicationKey = L"editor.exe";
    window.title = L"Document";
    window.framePx = RECT{0, 0, 640, 480};
    return window;
}

static void FingerprintLifetime() {
    context.settings.stacksEnabled = false;
    std::vector<ScannedWindow> initial{SampleWindow(0x100), SampleWindow(0x200)};
    AssignFingerprints(initial);
    Require(initial[0].fingerprint != initial[1].fingerprint, "initial duplicate collision");
    const std::wstring survivorFingerprint = initial[1].fingerprint;
    MergeScan(initial);
    const RectF survivorPosition = MakeRectF(1700.0f, -920.0f, 640.0f, 480.0f);
    context.cards[1].world = survivorPosition;
    RememberLayoutEntry(survivorFingerprint, survivorPosition);
    std::vector<ScannedWindow> surviving{initial[1]};
    AssignFingerprints(surviving);
    Require(surviving[0].fingerprint == survivorFingerprint, "closing A renumbered B");
    MergeScan(surviving);
    std::vector<ScannedWindow> reopened{SampleWindow(0x300), surviving[0]};
    AssignFingerprints(reopened);
    Require(reopened[1].fingerprint == survivorFingerprint, "opening C renumbered B");
    Require(reopened[0].fingerprint != reopened[1].fingerprint, "C collided with surviving B");
    MergeScan(reopened);
    const LayoutEntry* saved = FindLayoutEntry(survivorFingerprint);
    Require(saved != nullptr, "surviving B lost saved layout");
    SameRect(saved->world, survivorPosition);
    const int survivorIndex = FindCardIndex(reopened[1]);
    Require(survivorIndex >= 0, "surviving B disappeared during merge");
    SameRect(context.cards[static_cast<size_t>(survivorIndex)].world, survivorPosition);
}

static ScannedWindow SizedWindow(UINT_PTR handle, int size) {
    ScannedWindow window = SampleWindow(handle);
    window.framePx = RECT{0, 0, size, size};
    return window;
}

static void ScanCandidateSelection() {
    const ScannedWindow survivor = SizedWindow(0x100, 100);
    InsertCard(ScannedWindow{survivor}, MakeRectF(0.0f, 0.0f, 100.0f, 100.0f));
    std::vector<ScannedWindow> windows;
    windows.push_back(survivor);
    for (int index = 0; index < 64; ++index) {
        windows.push_back(SizedWindow(static_cast<UINT_PTR>(0x200 + index),
                                      640 - index));
    }
    SelectScanCandidates(windows);
    Require(windows.size() == 64, "candidate selection kept too many windows");
    Require(FindCardIndex(windows.front()) == 0,
            "candidate selection dropped an existing card");
    Require(windows[1].hwnd == reinterpret_cast<HWND>(0x200),
            "candidate selection did not keep the largest new window");
    const HWND smallestKept = reinterpret_cast<HWND>(0x200 + 62);
    const HWND smallestRejected = reinterpret_cast<HWND>(0x200 + 63);
    Require(std::any_of(windows.begin(), windows.end(),
                        [&](const ScannedWindow& window) {
                            return window.hwnd == smallestKept;
                        }),
            "candidate selection did not keep the expected smallest card");
    Require(std::none_of(windows.begin(), windows.end(),
                         [&](const ScannedWindow& window) {
                             return window.hwnd == smallestRejected;
                         }),
            "candidate selection kept the least useful overflow window");
}

static void FingerprintOrdinalStability() {
    std::vector<ScannedWindow> reversed{SampleWindow(0x200), SampleWindow(0x100)};
    std::vector<ScannedWindow> ordered{SampleWindow(0x100), SampleWindow(0x200)};
    AssignFingerprints(reversed);
    AssignFingerprints(ordered);
    Require(reversed[0].fingerprint == ordered[1].fingerprint,
            "ordinal assignment depended on scan order");
    Require(reversed[1].fingerprint == ordered[0].fingerprint,
            "ordinal assignment was not stable");
}

static void WindowIdentity() {
    const ScannedWindow original = SampleWindow(0x100);
    InsertCard(ScannedWindow{original}, MakeRectF(0.0f, 0.0f, 640.0f, 480.0f));
    Card& card = context.cards[0];
    Require(MatchesWindow(card, original), "live identical window failed to match");
    card.destroyed = true;
    Require(!MatchesWindow(card, original), "destroyed handle matched a new window");
    card.destroyed = false;
    ScannedWindow reused = original;
    ++reused.processId;
    Require(!MatchesWindow(card, reused), "reused HWND in another process matched");
    reused = original;
    ++reused.threadId;
    Require(!MatchesWindow(card, reused), "reused HWND in another thread matched");
    reused = original;
    reused.windowClass += L"-new";
    Require(!MatchesWindow(card, reused), "reused HWND with another class matched");
    reused = original;
    reused.exePath += L".new";
    Require(!MatchesWindow(card, reused), "reused HWND with another executable matched");
}

static void HeaderChrome() {
    Card card;
    card.world = MakeRectF(120.0f, 160.0f, 640.0f, 480.0f);
    const RectF bounds = CardScreenBounds(card);
    const RectF header = CardHeaderRect(card);
    Near(header.left, bounds.left);
    Near(header.right, bounds.right);
    Near(header.top, bounds.top);
    Near(header.height(), 44.0f * ChromeScale());

    const RectF close = CardCloseButtonRect(header, ChromeScale(), 2);
    const RectF pin = CardPinButtonRect(header, ChromeScale(), 2);
    Require(!close.empty() && !pin.empty(), "buttons missing on a wide header");
    Require(!RectsOverlap(close, pin), "header buttons overlap");
    Require(close.right <= header.right && close.left >= header.left,
            "close button escaped the header");
    Require(pin.left >= header.left, "pin button escaped the header");
    Near(close.right, header.right - kCardHeaderButtonInsetPx * ChromeScale());
    Near(pin.right, close.left - kCardHeaderButtonGapPx * ChromeScale());
    Near(close.centerY(), header.centerY());
    Near(pin.centerY(), header.centerY());

    Require(HitTestCardHeader(header, Vec2{close.centerX(), close.centerY()},
                              ChromeScale(), 2) == CardHeaderAction::Close,
            "close button did not hit");
    Require(HitTestCardHeader(header, Vec2{pin.centerX(), pin.centerY()},
                              ChromeScale(), 2) == CardHeaderAction::Pin,
            "pin button did not hit");
    Require(HitTestCardHeader(header, Vec2{header.left + 40.0f, header.centerY()},
                              ChromeScale(), 2) == CardHeaderAction::None,
            "title area reported a button");
    Require(HitTestCardHeader(header,
                              Vec2{header.centerX(), header.bottom + 4.0f},
                              ChromeScale(), 2) == CardHeaderAction::None,
            "point below the header reported a button");

    // A narrow card cannot host both buttons, so none are drawn or hit.
    Card narrow;
    narrow.world = MakeRectF(0.0f, 0.0f, 40.0f, 300.0f);
    const RectF narrowHeader = CardHeaderRect(narrow);
    Require(CardCloseButtonRect(narrowHeader, ChromeScale(), 2).empty(),
            "narrow header kept a close button");
    Require(CardPinButtonRect(narrowHeader, ChromeScale(), 2).empty(),
            "narrow header kept a pin button");

    // The strip is DPI relative: 44 DIP is 44 px at 96 DPI.
    context.chromeDpi = 96;
    const RectF scaled = CardHeaderRect(card);
    Near(scaled.height(), 44.0f);
    const RectF scaledClose = CardCloseButtonRect(scaled, ChromeScale(), 2);
    Near(scaledClose.width(), kCardHeaderButtonPx);
    Near(scaledClose.right, scaled.right - kCardHeaderButtonInsetPx);
}

static void PinnedOrder() {
    AddCard(MakeRectF(0.0f, 0.0f, 320.0f, 240.0f));
    AddCard(MakeRectF(0.0f, 0.0f, 320.0f, 240.0f));
    AddCard(MakeRectF(0.0f, 0.0f, 320.0f, 240.0f));
    context.cards[2].pinned = true;
    context.cards[0].pinned = true;

    const std::vector<size_t> order = CardDrawOrder(context.cards);
    Require(order.size() == context.cards.size(), "draw order lost a card");
    Require(order[0] == 1, "unpinned card must be drawn first");
    Require(order[1] == 0 && order[2] == 2, "pinned cards must be drawn last");

    const RectF bounds = CardScreenBounds(context.cards[1]);
    const Vec2 center{bounds.centerX(), bounds.centerY()};
    Require(HitTestCardInOrder(context.cards, center) == 2,
            "hit test ignored the topmost pinned card");
}

static void PinnedLayoutSync() {
    ScannedWindow window = SampleWindow(0x500);
    window.fingerprint = L"pinned#1";
    context.layout.entries.push_back(
        LayoutEntry{window.fingerprint, MakeRectF(0.0f, 0.0f, 640.0f, 480.0f), true});
    MergeScan(std::vector<ScannedWindow>{window});
    Require(context.cards.size() == 1, "pinned window was not added");
    Require(context.cards[0].pinned, "merge dropped the saved pin");
}

static void PinPersistence() {
    context.layout = LayoutSnapshot{};
    context.layout.entries.push_back(
        LayoutEntry{L"app#1", MakeRectF(10.0f, 20.0f, 300.0f, 200.0f), false});
    RememberLayoutPin(L"app#1", MakeRectF(10.0f, 20.0f, 300.0f, 200.0f), true, true);
    Require(context.layout.entries[0].pinned, "pin was not remembered");

    const std::string serialized = SerializeLayout(context.layout);
    Require(serialized.find("\"pin\":1") != std::string::npos,
            "serializer omitted the pin flag");
    const LayoutSnapshot parsed = ParseLayout(serialized);
    Require(parsed.entries.size() == 1 && parsed.entries[0].pinned,
            "pin was lost in the roundtrip");
    SameRect(parsed.entries[0].world, MakeRectF(10.0f, 20.0f, 300.0f, 200.0f));

    const LayoutSnapshot legacy = ParseLayout(
        R"({"version":1,"entries":[{"key":"a","x":0,"y":0,"w":1,"h":1}]})");
    Require(legacy.entries.size() == 1 && !legacy.entries[0].pinned,
            "legacy layout gained a pin");
    const LayoutSnapshot invalid = ParseLayout(
        R"({"version":1,"entries":[{"key":"a","x":0,"y":0,"w":1,"h":1,"pin":2}]})");
    Require(invalid.entries.empty(), "out-of-range pin accepted");
}

static void LayoutEntryEviction() {
    context.layout = LayoutSnapshot{};
    context.layout.entries.reserve(kMaxLayoutEntries);
    for (int index = 0; index < kMaxLayoutEntries; ++index) {
        const std::wstring fingerprint = L"old#" + std::to_wstring(index);
        context.layout.entries.push_back(
            LayoutEntry{fingerprint, MakeRectF(0.0f, 0.0f, 1.0f, 1.0f),
                        false});
    }
    RememberLayoutEntry(L"refreshed", MakeRectF(2.0f, 0.0f, 1.0f, 1.0f));
    Require(context.layout.entries.size() == kMaxLayoutEntries,
            "layout cap was not enforced");
    Require(FindLayoutEntry(L"refreshed") != nullptr,
            "refreshed layout entry was not retained");
    Require(FindLayoutEntry(L"old#0") == nullptr,
            "oldest unpinned layout entry was not evicted");
    RememberLayoutPin(L"new", MakeRectF(3.0f, 0.0f, 1.0f, 1.0f), true, true);
    Require(context.layout.entries.size() == kMaxLayoutEntries,
            "pinned eviction changed the layout cap");
    Require(FindLayoutEntry(L"new") != nullptr,
            "new pinned layout entry was not retained");
    Require(FindLayoutEntry(L"old#1") == nullptr,
            "oldest unpinned entry survived a pinned insert");
    const std::string serialized = SerializeLayout(context.layout);
    Require(serialized.find("\"version\":1") != std::string::npos,
            "layout schema version changed");
}

static void PinSurvivesLayoutMove() {
    context.layout = LayoutSnapshot{};
    context.layout.entries.push_back(
        LayoutEntry{L"pinned#1", MakeRectF(10.0f, 20.0f, 300.0f, 200.0f), true});

    const RectF moved = MakeRectF(400.0f, 500.0f, 640.0f, 480.0f);
    RememberLayoutEntry(L"pinned#1", moved);
    Require(context.layout.entries.size() == 1,
            "moving a pinned entry changed the layout entry count");
    Require(context.layout.entries[0].pinned,
            "moving a card cleared its saved pin");
    SameRect(context.layout.entries[0].world, moved);
}

static void LayoutRecencyOrder() {
    context.layout = LayoutSnapshot{};
    context.layout.entries.push_back(
        LayoutEntry{L"a", MakeRectF(0.0f, 0.0f, 1.0f, 1.0f), false});
    context.layout.entries.push_back(
        LayoutEntry{L"b", MakeRectF(1.0f, 0.0f, 1.0f, 1.0f), false});
    context.layout.entries.push_back(
        LayoutEntry{L"c", MakeRectF(2.0f, 0.0f, 1.0f, 1.0f), false});

    const RectF touched = MakeRectF(30.0f, 40.0f, 1.0f, 1.0f);
    RememberLayoutEntry(L"a", touched);
    Require(context.layout.entries.size() == 3,
            "touching an entry changed the layout entry count");
    Require(context.layout.entries[0].fingerprint == L"b",
            "least recently touched entry did not become first");
    Require(context.layout.entries[1].fingerprint == L"c",
            "middle recency entry moved unexpectedly");
    Require(context.layout.entries[2].fingerprint == L"a",
            "touched entry did not move to the back");
    SameRect(context.layout.entries[2].world, touched);
}

static void LayoutPinnedCapacity() {
    context.layout = LayoutSnapshot{};
    for (int index = 0; index < kMaxLayoutEntries; ++index) {
        const std::wstring fingerprint = L"pinned#" + std::to_wstring(index);
        context.layout.entries.push_back(
            LayoutEntry{fingerprint, MakeRectF(0.0f, 0.0f, 1.0f, 1.0f), true});
    }

    RememberLayoutPin(L"overflow", MakeRectF(2.0f, 0.0f, 1.0f, 1.0f), false, true);
    Require(context.layout.entries.size() ==
                static_cast<size_t>(kMaxLayoutEntries),
            "pinned layout exceeded its cap");
    Require(FindLayoutEntry(L"overflow") == nullptr,
            "an unpinned entry displaced a pinned one");
}

static void DesktopPullMoves() {
    FakeDesktopManager manager;
    const DesktopPullOutcome outcome =
        PullWindowToDesktop(&manager, manager.windowHandle, manager.referenceHandle);
    Require(outcome.result == DesktopPullResult::Moved,
            "other-desktop window was not moved");
    Require(outcome.error == S_OK, "successful move reported an error");
    Require(manager.moveCalls == 1, "move request was not issued exactly once");
    Require(manager.movedWindow == manager.windowHandle, "moved the wrong window");
    Require(IsEqualGUID(manager.movedTarget, kCanvasDesktop) != FALSE,
            "moved to the wrong desktop");
}

static void DesktopPullSame() {
    FakeDesktopManager manager;
    manager.windowDesktop = kCanvasDesktop;
    const DesktopPullOutcome outcome =
        PullWindowToDesktop(&manager, manager.windowHandle, manager.referenceHandle);
    Require(outcome.result == DesktopPullResult::AlreadyHere,
            "same-desktop window was not skipped");
    Require(manager.moveCalls == 0, "same-desktop window was moved");
}

static void DesktopPullNull() {
    FakeDesktopManager manager;
    const DesktopPullOutcome noManager =
        PullWindowToDesktop(nullptr, manager.windowHandle, manager.referenceHandle);
    Require(noManager.result == DesktopPullResult::Unavailable,
            "null manager was accepted");
    const DesktopPullOutcome noWindow =
        PullWindowToDesktop(&manager, nullptr, manager.referenceHandle);
    Require(noWindow.result == DesktopPullResult::Unavailable,
            "null window was accepted");
    const DesktopPullOutcome noReference =
        PullWindowToDesktop(&manager, manager.windowHandle, nullptr);
    Require(noReference.result == DesktopPullResult::Unavailable,
            "null reference was accepted");
    Require(manager.moveCalls == 0, "unusable inputs still moved the window");
}

static void DesktopPullReferenceFailure() {
    FakeDesktopManager manager;
    manager.referenceError = E_FAIL;
    const DesktopPullOutcome unknownTarget =
        PullWindowToDesktop(&manager, manager.windowHandle, manager.referenceHandle);
    Require(unknownTarget.result == DesktopPullResult::Unavailable,
            "unknown canvas desktop was accepted");
    Require(manager.moveCalls == 0, "unknown canvas desktop still moved the window");

    FakeDesktopManager unknownSource;
    unknownSource.windowError = E_FAIL;
    const DesktopPullOutcome moved = PullWindowToDesktop(
        &unknownSource, unknownSource.windowHandle, unknownSource.referenceHandle);
    Require(moved.result == DesktopPullResult::Moved,
            "unreadable source desktop blocked the move");
    Require(unknownSource.moveCalls == 1,
            "unreadable source desktop skipped the move");
}

static void DesktopPullRejected() {
    FakeDesktopManager manager;
    manager.moveError = E_ACCESSDENIED;
    const DesktopPullOutcome outcome =
        PullWindowToDesktop(&manager, manager.windowHandle, manager.referenceHandle);
    Require(outcome.result == DesktopPullResult::Rejected,
            "refused move was reported as success");
    Require(outcome.error == E_ACCESSDENIED, "refused move lost its HRESULT");
    Require(manager.moveCalls == 1, "refused move was not attempted");
}

static void IconSourceOrder() {
    Require(ARRAYSIZE(kShellIconListSizes) == 3,
            "unexpected icon candidate count");
    Require(kShellIconListSizes[0] == SHIL_JUMBO, "jumbo is not first");
    Require(kShellIconListSizes[1] == SHIL_EXTRALARGE,
            "extra-large is not second");
    Require(kShellIconListSizes[2] == SHIL_LARGE, "large is not last");
}

static void IconSourceLargestWins() {
    int queries = 0;
    int chosen = -1;
    const HICON icon = QueryFirstAvailableIcon(
        kShellIconListSizes, [&](int listSize) {
            ++queries;
            chosen = listSize;
            return reinterpret_cast<HICON>(0x1);
        });
    Require(icon != nullptr, "an available icon was dropped");
    Require(queries == 1, "the lookup continued after the first hit");
    Require(chosen == SHIL_JUMBO, "a smaller source was queried first");
}

static void IconSourceFallback() {
    int queries = 0;
    const HICON icon = QueryFirstAvailableIcon(
        kShellIconListSizes, [&](int listSize) {
            ++queries;
            return listSize == SHIL_LARGE ? reinterpret_cast<HICON>(0x2)
                                          : nullptr;
        });
    Require(icon == reinterpret_cast<HICON>(0x2), "the fallback was dropped");
    Require(queries == 3, "the lookup stopped before the last candidate");
}

static void IconSourceMissing() {
    int queries = 0;
    const HICON icon =
        QueryFirstAvailableIcon(kShellIconListSizes, [&](int) {
            ++queries;
            return static_cast<HICON>(nullptr);
        });
    Require(icon == nullptr, "a missing icon was reported as found");
    Require(queries == 3, "the lookup skipped a candidate");
}

static void LocalizationStrings() {
    for (size_t text = 0; text < static_cast<size_t>(CanvasTextId::Count);
         ++text) {
        for (size_t language = 0;
             language < static_cast<size_t>(LanguageId::Count); ++language) {
            const auto textId = static_cast<CanvasTextId>(text);
            const auto languageId = static_cast<LanguageId>(language);
            const size_t length = CanvasTextLength(textId, languageId);
            Require(length > 0,
                    "a canvas string is missing for a supported language");
            Require(wcslen(CanvasText(textId, languageId)) == length,
                    "canvas string length disagrees with its text");
        }
    }
}

static void LocalizationBadges() {
    Require(wcscmp(CanvasText(CanvasTextId::MinimizedBadge,
                              LanguageId::English),
                   L"MINIMIZED") == 0,
            "English minimized badge changed");
    Require(wcscmp(CanvasText(CanvasTextId::MinimizedBadge,
                              LanguageId::SimplifiedChinese),
                   L"已最小化") == 0,
            "Simplified Chinese minimized badge changed");
    Require(wcscmp(CanvasText(CanvasTextId::OtherDesktopBadge,
                              LanguageId::Turkish),
                   L"BAŞKA MASAÜSTÜ") == 0,
            "Turkish other-desktop badge changed");
    Require(wcscmp(CanvasText(CanvasTextId::EmptyCanvas, LanguageId::German),
                   L"Keine geeigneten Fenster") == 0,
            "German empty-canvas text changed");
    Require(wcscmp(CanvasText(CanvasTextId::PreviewUnavailableBadge,
                              LanguageId::Japanese),
                   L"プレビュー利用不可") == 0,
            "Japanese preview badge changed");
}

static void LocalizationCodes() {
    Require(ARRAYSIZE(kLanguageOptions) ==
                static_cast<size_t>(LanguageId::Count),
            "the language option table does not cover every language");
    for (const LanguageOption& option : kLanguageOptions) {
        LanguageId parsed = LanguageId::Count;
        Require(TryParseLanguageCode(option.code, parsed),
                "a listed language code did not parse");
        Require(parsed == option.language,
                "a language code resolved to the wrong language");
    }
}

static void LocalizationFallback() {
    LanguageId parsed = LanguageId::Count;
    Require(!TryParseLanguageCode(nullptr, parsed),
            "a null language code was accepted");
    Require(!TryParseLanguageCode(kAutoLanguageCode, parsed),
            "auto was treated as an explicit language");
    Require(!TryParseLanguageCode(L"xx", parsed),
            "an unknown language code was accepted");
    Require(parsed == LanguageId::Count,
            "a rejected language code modified the output");
    Require(ResolveLanguageSetting(L"xx") == LanguageId::English,
            "an unknown setting did not fall back to English");
    Require(ResolveLanguageSetting(L"zh_TW") == LanguageId::TraditionalChinese,
            "an explicit traditional Chinese code was ignored");
    const LanguageId detected = ResolveLanguageSetting(nullptr);
    Require(static_cast<size_t>(detected) <
                static_cast<size_t>(LanguageId::Count),
            "auto resolved outside the language range");
}

static void LocalizationOutOfRange() {
    Require(wcscmp(CanvasText(CanvasTextId::Count, LanguageId::English),
                   L"") == 0,
            "an out-of-range text id returned text");
    Require(CanvasTextLength(CanvasTextId::Count, LanguageId::English) == 0,
            "an out-of-range text id returned a length");
    Require(wcscmp(CanvasText(CanvasTextId::MinimizedBadge, LanguageId::Count),
                   L"") == 0,
            "an out-of-range language returned text");
    Require(CanvasTextLength(CanvasTextId::MinimizedBadge, LanguageId::Count) ==
                0,
            "an out-of-range language returned a length");
}

static void LocalizationBadgeSelection() {
    Require(CardBadgeTextId(WindowVisualState::Minimized) ==
                CanvasTextId::MinimizedBadge,
            "a minimized card lost its badge");
    Require(CardBadgeTextId(WindowVisualState::OtherDesktop) ==
                CanvasTextId::OtherDesktopBadge,
            "an other-desktop card lost its badge");
    Require(CardBadgeTextId(WindowVisualState::Live) ==
                CanvasTextId::PreviewUnavailableBadge,
            "a live card without a preview lost its placeholder badge");
}

static void AddStackCard(const wchar_t* fingerprint, const wchar_t* exePath) {
    AddCard(MakeRectF(0.0f, 0.0f, 320.0f, 240.0f));
    context.cards.back().fingerprint = fingerprint;
    context.cards.back().exePath = exePath;
    context.cards.back().applicationKey = ExecutableName(exePath);
}

static void UseStackSettings() {
    context.settings = CanvasSettings{};
    context.settings.stacksEnabled = true;
    context.settings.stackStepDegrees = 6.5f;
    context.settings.stackMaxDegrees = 24.0f;
    context.settings.stackScaleFactor = 0.975f;
    context.settings.stackMaxDepth = 5;
    context.settings.stackRotationDirection = -1;
}

static void StackGeometry() {
    UseStackSettings();
    const std::wstring key = L"c:\\app\\editor.exe";
    const float radians = 3.14159265f / 180.0f;
    const CardStackPlacement front = StackPlacementForSlot(0, key, context.settings);
    Require(front.grouped && front.visible && front.slot == 0, "front slot lost its flags");
    Require(front.groupKey == key, "front slot lost its group key");
    Near(front.rotationRadians, 0.0f);
    Near(front.scale, 1.0f);
    const CardStackPlacement second = StackPlacementForSlot(1, key, context.settings);
    Near(second.rotationRadians, -6.5f * radians);
    Near(second.scale, 0.975f);
    const CardStackPlacement fourth = StackPlacementForSlot(3, key, context.settings);
    Near(fourth.rotationRadians, -19.5f * radians);
    const CardStackPlacement fifth = StackPlacementForSlot(4, key, context.settings);
    Near(fifth.rotationRadians, -24.0f * radians);
    Near(fifth.scale, 0.975f * 0.975f * 0.975f * 0.975f);
    const CardStackPlacement sixth = StackPlacementForSlot(5, key, context.settings);
    Near(sixth.rotationRadians, -24.0f * radians);
    Near(sixth.scale, 0.975f * 0.975f * 0.975f * 0.975f * 0.975f);
    context.settings.stackRotationDirection = 0;
    Near(StackPlacementForSlot(2, key, context.settings).rotationRadians, 0.0f);
    context.settings.stackRotationDirection = 1;
    Near(StackPlacementForSlot(2, key, context.settings).rotationRadians, 13.0f * radians);
}

static void StackCycle() {
    UseStackSettings();
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"b#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"c#1", L"C:\\App\\Editor.exe");
    ReconcileCardStacks(context.cards, context.settings);
    const std::wstring key = L"editor.exe";
    for (const Card& card : context.cards) {
        Require(card.stack.groupKey == key, "stack member lost its lowercased group key");
    }
    std::vector<size_t> members;
    Require(StackMemberOrder(context.cards, key, members), "stack order was rejected");
    Require(members.size() == 3 && members[0] == 0, "initial stack front is not the first card");
    Require(!CycleCardStack(context.cards, context.settings, 0, 0), "zero-step cycle was accepted");
    Require(!CycleCardStack(context.cards, context.settings, -1, 1), "negative index cycle was accepted");
    Require(!CycleCardStack(context.cards, context.settings, 99, 1), "out-of-range cycle was accepted");
    Require(CycleCardStack(context.cards, context.settings, 0, 1), "forward cycle was rejected");
    Require(context.cards[1].stack.slot == 0, "forward cycle did not promote the second card");
    Require(context.cards[0].stack.slot == 2, "forward cycle did not move the front to the back");
    Require(context.cards[1].stack.groupKey == key, "cycle changed the group key");
    Require(CycleCardStack(context.cards, context.settings, 1, -1), "backward cycle was rejected");
    Require(context.cards[0].stack.slot == 0, "backward cycle did not restore the front");
    context.settings.stacksEnabled = false;
    Require(!CycleCardStack(context.cards, context.settings, 0, 1), "disabled stack cycled");
    context.settings.stacksEnabled = true;
    context.cards[2].stack.grouped = false;
    Require(!CycleCardStack(context.cards, context.settings, 2, 1), "ungrouped card cycled its stack");
}

static void StackRemoval() {
    UseStackSettings();
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"b#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"c#1", L"C:\\App\\Editor.exe");
    ReconcileCardStacks(context.cards, context.settings);
    const std::wstring key = L"editor.exe";
    std::vector<size_t> members;
    Require(StackMemberOrder(context.cards, key, members), "stack order was rejected");
    const std::wstring promoted = context.cards[members[1]].fingerprint;
    RemoveCardAt(members[0]);
    ReconcileCardStacks(context.cards, context.settings);
    Require(context.cards.size() == 2, "front removal lost extra cards");
    Require(StackMemberOrder(context.cards, key, members), "stack order broke after removal");
    Require(context.cards[members[0]].fingerprint == promoted, "next member was not promoted");
    Require(context.cards[members[0]].stack.slot == 0, "promoted member is not at slot zero");
    const std::wstring front = context.cards[members[0]].fingerprint;
    RemoveCardAt(members.back());
    ReconcileCardStacks(context.cards, context.settings);
    Require(context.cards.size() == 1, "back removal lost extra cards");
    Require(context.cards[0].fingerprint == front && context.cards[0].stack.slot == 0,
            "back removal changed the front");
}

static void StackInvalid() {
    UseStackSettings();
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"b#1", L"C:\\App\\Editor.exe");
    ReconcileCardStacks(context.cards, context.settings);
    Require(!context.cards[0].stack.groupKey.empty(), "stack was not formed before disable");
    context.settings.stacksEnabled = false;
    ReconcileCardStacks(context.cards, context.settings);
    for (const Card& card : context.cards) {
        Require(card.stack.groupKey.empty() && card.stack.slot == 0, "disabled stacks kept a placement");
        Near(card.stack.rotationRadians, 0.0f);
        Near(card.stack.scale, 1.0f);
        Require(card.stack.visible, "disabled stacks hid a card");
    }
    context.settings.stacksEnabled = true;
    context.cards.clear();
    AddStackCard(L"x#1", L"");
    AddStackCard(L"y#1", L"");
    AddStackCard(L"z#1", L"C:\\App\\Editor.exe");
    ReconcileCardStacks(context.cards, context.settings);
    Require(context.cards[0].stack.groupKey.empty(), "empty path joined a stack");
    Require(context.cards[1].stack.groupKey.empty(), "empty path joined a stack");
    Require(context.cards[2].stack.groupKey.empty() && context.cards[2].stack.grouped,
            "single member kept a stack or lost its grouping preference");
    context.cards.clear();
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"b#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"c#1", L"C:\\App\\Editor.exe");
    ReconcileCardStacks(context.cards, context.settings);
    context.cards[1].stack.slot = context.cards[0].stack.slot;
    std::vector<size_t> members;
    Require(!StackMemberOrder(context.cards, L"c:\\app\\editor.exe", members),
            "duplicate slots were accepted");
    Require(!CycleCardStack(context.cards, context.settings, 99, 1), "out-of-range cycle was accepted");
}

static void StackDepthVisibility() {
    UseStackSettings();
    const std::wstring key = L"c:\\app\\editor.exe";
    context.settings.stackMaxDepth = 2;
    Require(StackPlacementForSlot(2, key, context.settings).visible, "slot at max depth was hidden");
    Require(!StackPlacementForSlot(3, key, context.settings).visible, "slot past max depth stayed visible");
    context.settings.stackMaxDepth = 99;
    Require(StackPlacementForSlot(8, key, context.settings).visible, "slot at hard cap was hidden");
    Require(!StackPlacementForSlot(9, key, context.settings).visible, "slot past hard cap stayed visible");
    context.settings.stackMaxDepth = -1;
    Require(StackPlacementForSlot(0, key, context.settings).visible, "front was hidden at zero depth");
    Require(!StackPlacementForSlot(1, key, context.settings).visible, "slot past zero depth stayed visible");
}

static void StackHitRotation() {
    const RectF bounds = MakeRectF(100.0f, 200.0f, 640.0f, 480.0f);
    CardStackPlacement placement;
    placement.groupKey = L"c:\\app\\editor.exe";
    placement.slot = 1;
    placement.rotationRadians = -0.4f;
    placement.scale = 0.9f;
    const Vec2 center{bounds.centerX(), bounds.centerY()};
    const Vec2 local{200.0f, 300.0f};
    const Vec2 scaled{center.x + (local.x - center.x) * placement.scale,
                      center.y + (local.y - center.y) * placement.scale};
    const Vec2 screen = RotatePoint(scaled, center, placement.rotationRadians);
    const Vec2 roundtrip = CardLocalPoint(screen, bounds, placement);
    Near(roundtrip.x, local.x);
    Near(roundtrip.y, local.y);
    const CardStackPlacement identity;
    const std::array<Vec2, 4> quad = CardScreenQuad(bounds, identity);
    Near(quad[0].x, bounds.left);
    Near(quad[0].y, bounds.top);
    Near(quad[1].x, bounds.right);
    Near(quad[1].y, bounds.top);
    Near(quad[2].x, bounds.right);
    Near(quad[2].y, bounds.bottom);
    Near(quad[3].x, bounds.left);
    Near(quad[3].y, bounds.bottom);
    const std::array<Vec2, 4> rotated = CardScreenQuad(bounds, placement);
    float middleX = 0.0f;
    float middleY = 0.0f;
    for (const Vec2& corner : rotated) {
        middleX += corner.x;
        middleY += corner.y;
    }
    Near(middleX * 0.25f, center.x);
    Near(middleY * 0.25f, center.y);
}

static float RectArea(const RectF& rect) {
    return rect.width() * rect.height();
}

static void StackThumbnailOcclusionClips() {
    const RectF viewport = MakeRectF(0.0f, 0.0f, 100.0f, 100.0f);
    const RectF card = MakeRectF(10.0f, 10.0f, 80.0f, 80.0f);
    const CardStackPlacement untransformed;
    const std::array<Vec2, 4> cardQuad = CardScreenQuad(card, untransformed);

    const CardDrawClips noOcclusion = CardDrawRegions(
        viewport, cardQuad, {MakeRectF(90.0f, 90.0f, 10.0f, 10.0f)});
    Require(!noOcclusion.clipped && noOcclusion.regions.empty(),
            "non-overlapping thumbnail added a clip");

    const std::vector<RectF> holes = {
        MakeRectF(20.0f, 20.0f, 40.0f, 40.0f),
        MakeRectF(40.0f, 40.0f, 40.0f, 40.0f)};
    const CardDrawClips clipped = CardDrawRegions(viewport, cardQuad, holes);
    Require(clipped.clipped && !clipped.regions.empty(),
            "overlapping thumbnails did not clip the background card");
    float visibleArea = 0.0f;
    for (const RectF& region : clipped.regions) {
        visibleArea += RectArea(region);
        for (const RectF& hole : holes) {
            Require(!RectsOverlap(region, hole),
                    "background card clip overlaps a live thumbnail");
        }
    }
    Near(visibleArea, 7200.0f);

    CardStackPlacement rotated;
    rotated.rotationRadians = -0.35f;
    rotated.scale = 0.9f;
    const std::array<Vec2, 4> rotatedQuad = CardScreenQuad(card, rotated);
    const CardDrawClips rotatedClip = CardDrawRegions(
        viewport, rotatedQuad, {MakeRectF(5.0f, 5.0f, 20.0f, 20.0f)});
    Require(rotatedClip.clipped && !rotatedClip.regions.empty(),
            "rotated background card did not reserve the thumbnail hole");
    for (const RectF& region : rotatedClip.regions) {
        Require(!RectsOverlap(region, MakeRectF(5.0f, 5.0f, 20.0f, 20.0f)),
                "rotated card clip covers the live thumbnail");
    }

    std::vector<Card> cards(2);
    cards[0].state = WindowVisualState::Live;
    cards[0].thumbnail.reset(reinterpret_cast<HTHUMBNAIL>(1));
    cards[0].thumbnailVisible = true;
    cards[0].thumbnailDestination = RECT{20, 20, 60, 60};
    cards[1].state = WindowVisualState::Live;
    cards[1].thumbnail.reset(reinterpret_cast<HTHUMBNAIL>(2));
    cards[1].thumbnailVisible = true;
    cards[1].thumbnailDestination = RECT{40, 40, 80, 80};
    const std::vector<size_t> stackOrder = {1, 0};
    Require(HigherThumbnailRects(cards, stackOrder, 0).size() == 1,
            "lower card did not reserve the upper preview");
    Require(HigherThumbnailRects(cards, stackOrder, 1).empty(),
            "stack front reserved a lower preview");
    const CardDrawClips frontClip = CardDrawRegions(
        viewport, cardQuad, HigherThumbnailRects(cards, stackOrder, 1));
    Require(!frontClip.clipped,
            "upright stack front was clipped by lower cards");
    const RectF frontPreview = RectFromRect(cards[0].thumbnailDestination);
    const CardDrawClips backgroundClip = CardDrawRegions(
        viewport, cardQuad, HigherThumbnailRects(cards, stackOrder, 0));
    Require(backgroundClip.clipped,
            "stack background ignored the front preview");
    for (const RectF& region : backgroundClip.regions) {
        Require(!RectsOverlap(region, frontPreview),
                "stack background was painted over the live front preview");
    }

    const RectF destination = MakeRectF(10.0f, 10.0f, 50.0f, 50.0f);
    const RectF visible = UnoccludedThumbnailRect(destination,
        {MakeRectF(40.0f, 0.0f, 50.0f, 80.0f)});
    SameRect(visible, MakeRectF(10.0f, 10.0f, 30.0f, 50.0f));
    ThumbnailGeometry geometry{MakeRectF(5.0f, 5.0f, 100.0f, 100.0f), destination};
    Require(ClipThumbnailGeometry(geometry, visible), "visible strip did not crop");
    SameRect(geometry.destination, visible);
    SameRect(geometry.source, MakeRectF(5.0f, 5.0f, 60.0f, 100.0f));
    Require(UnoccludedThumbnailRect(destination, {destination}).empty(),
            "fully covered thumbnail stayed visible");
    Require(UnoccludedThumbnailRect(destination,
        {MakeRectF(40.0f, 40.0f, 50.0f, 50.0f)}).empty(),
        "a non-rectangular visible area was submitted as a thumbnail");
    Card covered;
    Require(!CardShowsBadge(covered), "a covered card reported a preview error");
    covered.thumbnailFailed = true;
    Require(CardShowsBadge(covered), "a failed live preview did not report its error");
}

static void HotkeyDefaultAndDisable() {
    Require(CanvasSettings{}.toggleHotkey == L"Ctrl+Alt+Shift+F12", "wrong default hotkey");
    UINT modifiers = 0;
    UINT key = 0;
    Require(ParseHotkeyText(CanvasSettings{}.toggleHotkey, modifiers, key),
            "default hotkey was rejected");
    Require(key >= VK_F1 && key <= VK_F24, "default hotkey uses a character key");
    Require(ParseHotkeyText(L"Alt+Z", modifiers, key), "Alt+Z hotkey was rejected");
    Require(modifiers == MOD_ALT && key == static_cast<UINT>('Z'), "Alt+Z parsed incorrectly");
    Require(ParseHotkeyText(L"alt+z", modifiers, key), "lowercase alt+z hotkey was rejected");
    Require(modifiers == MOD_ALT && key == static_cast<UINT>('Z'), "lowercase alt+z parsed incorrectly");
    context.hotkeyRetryDelayMs = kHotkeyRetryInitialMs;
    context.hotkeyRetryTick = 1;
    context.settings.toggleHotkey.clear();
    Require(RegisterCanvasHotkey(true), "blank setting was not accepted");
    Require(!context.hotkeyRegistered && HotkeyRetryRemaining(2) == INFINITE,
            "disabling did not cancel the hotkey retry");
    context.settings.toggleHotkey = L"Ctrl+F99";
    Require(!RegisterCanvasHotkey(true) && !HotkeyRetryPending(),
            "invalid setting must not retry");
}

static void HotkeyRegistrationConflictAndRecovery() {
    constexpr UINT probeId = 0x4a20;
    constexpr UINT modifiers = MOD_CONTROL | MOD_SHIFT | MOD_ALT;
    UINT probeKey = VK_F8;
    while (probeKey <= VK_F11 &&
           RegisterHotKey(nullptr, probeId, modifiers, probeKey) == FALSE) {
        ++probeKey;
    }
    Require(probeKey <= VK_F11, "no free hotkey for the probe");
    const std::wstring combination = L"Ctrl+Shift+Alt+F" +
        std::to_wstring(probeKey - VK_F1 + 1);
    context.settings.toggleHotkey = combination;
    Require(!RegisterCanvasHotkey(true) && HotkeyRetryPending(),
            "conflicting hotkey did not schedule a retry");
    Require(UnregisterHotKey(nullptr, probeId) != FALSE, "could not release probe hotkey");
    RetryCanvasHotkeyIfDue(context.hotkeyRetryTick + context.hotkeyRetryDelayMs);
    Require(context.hotkeyRegistered && !HotkeyRetryPending(),
            "released hotkey did not register on retry");
    context.settings.toggleHotkey.clear();
    Require(RegisterCanvasHotkey(true) && !context.hotkeyRegistered,
            "blank setting did not unregister the active hotkey");
    Require(RegisterHotKey(nullptr, probeId, modifiers, probeKey) != FALSE,
            "disabled hotkey was still held");
    Require(UnregisterHotKey(nullptr, probeId) != FALSE, "could not release probe hotkey");
    context.settings.toggleHotkey = combination;
    Require(RegisterCanvasHotkey(true) && context.hotkeyRegistered,
            "new setting did not re-enable the hotkey");
    context.settings.toggleHotkey.clear();
    Require(RegisterCanvasHotkey(true), "could not release the test hotkey");
}

// A failed registration must schedule a capped exponential backoff so a hotkey
// reloaded while another program holds the combination becomes active again
// without the user editing the setting a second time.
static void HotkeyRetryBackoff() {
    Require(NextHotkeyRetryDelay(0) == kHotkeyRetryInitialMs,
            "first retry delay is not the initial value");
    Require(NextHotkeyRetryDelay(kHotkeyRetryInitialMs) == 2 * kHotkeyRetryInitialMs,
            "second retry delay did not double");
    Require(NextHotkeyRetryDelay(2 * kHotkeyRetryInitialMs) == 4 * kHotkeyRetryInitialMs,
            "third retry delay did not double again");
    Require(NextHotkeyRetryDelay(kHotkeyRetryMaxMs) == kHotkeyRetryMaxMs,
            "retry delay overshot the cap");
    Require(NextHotkeyRetryDelay(kHotkeyRetryMaxMs / 2 + 1) == kHotkeyRetryMaxMs,
            "retry delay halved growth past the cap");
}

// The reported bug: with a real stack, the upright card at the front of the
// stack was covered by the cards stacked beneath it. Every background member
// must reserve the front preview, and the front itself must stay unclipped.
static void StackFrontOcclusionFullStack() {
    PrepareGeometry();
    UseStackSettings();
    context.cards.clear();
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"b#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"c#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"d#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"e#1", L"C:\\App\\Editor.exe");
    ReconcileCardStacks(context.cards, context.settings);
    const std::vector<size_t> order = CardDrawOrder(context.cards);
    Require(order.size() == 5, "draw order lost a stack member");
    size_t frontPosition = order.size();
    for (size_t position = 0; position < order.size(); ++position) {
        if (context.cards[order[position]].stack.slot == 0) {
            frontPosition = position;
        }
    }
    Require(frontPosition == order.size() - 1,
            "stack front was not painted last");

    Card& front = context.cards[order[frontPosition]];
    front.state = WindowVisualState::Live;
    front.thumbnail.reset(reinterpret_cast<HTHUMBNAIL>(9));
    front.thumbnailVisible = true;
    front.thumbnailDestination =
        ThumbnailRect(WorldToScreen(StackWorldRect(front), context.camera));
    const RectF frontPreview = RectFromRect(front.thumbnailDestination);
    Require(!frontPreview.empty(), "live stack front had no preview");
    const RectF viewport = MakeRectF(
        frontPreview.left - 64.0f, frontPreview.top - 64.0f,
        frontPreview.width() + 128.0f, frontPreview.height() + 128.0f);

    for (size_t position = 0; position < frontPosition; ++position) {
        const Card& member = context.cards[order[position]];
        Require(IsStackBackground(member),
                "stack background was painted after the front");
        const CardDrawClips clips = CardDrawRegions(
            viewport, CardScreenQuad(CardScreenBounds(member), member.stack),
            HigherThumbnailRects(context.cards, order, position));
        Require(clips.clipped && !clips.regions.empty(),
                "stack background was not clipped around the live front");
        for (const RectF& region : clips.regions) {
            Require(!RectsOverlap(region, frontPreview),
                    "stack background covered the live front preview");
        }
    }

    const CardDrawClips frontClips = CardDrawRegions(
        viewport, CardScreenQuad(CardScreenBounds(front), front.stack),
        HigherThumbnailRects(context.cards, order, frontPosition));
    Require(!frontClips.clipped, "live stack front was clipped by lower cards");
}

static void StackSameExeNameGroups() {
    UseStackSettings();
    context.cards.clear();
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"b#1", L"D:\\Tools\\v2\\Editor.exe");
    AddStackCard(L"c#1", L"C:\\App\\EDITOR.EXE");
    AddStackCard(L"d#1", L"C:\\App\\Notepad.exe");
    context.cards[0].world = MakeRectF(0.0f, 0.0f, 320.0f, 240.0f);
    context.cards[1].world = MakeRectF(1000.0f, 0.0f, 320.0f, 240.0f);
    context.cards[2].world = MakeRectF(2000.0f, 0.0f, 320.0f, 240.0f);
    context.cards[3].world = MakeRectF(3000.0f, 0.0f, 320.0f, 240.0f);
    ReconcileCardStacks(context.cards, context.settings);
    const RectF anchor = MakeRectF(0.0f, 0.0f, 320.0f, 240.0f);
    for (size_t index = 0; index < 3; ++index) {
        Require(context.cards[index].stack.groupKey == L"editor.exe", "same exe name from different paths did not group");
        Require(context.cards[index].stack.slot == index, "stack slot order is wrong");
        SameRect(StackWorldRect(context.cards[index]), anchor);
        SameRect(CardScreenBounds(context.cards[index]), CardScreenBounds(context.cards[0]));
    }
    Require(!IsStackBackground(context.cards[0]), "stack front was marked as background");
    Require(IsStackBackground(context.cards[1]) && IsStackBackground(context.cards[2]), "stack background was not detected");
    Require(context.cards[3].stack.groupKey.empty(), "a lone app got a stack");
    Require(context.cards[3].stack.slot == 0, "different exe lost its own front slot");
    SameRect(StackWorldRect(context.cards[3]), context.cards[3].world);
    const std::vector<size_t> order = CardDrawOrder(context.cards);
    Require(order == std::vector<size_t>{2, 1, 3, 0},
            "singleton was treated as a stack front or the stack was painted in reverse");
    const RectF bounds = CardScreenBounds(context.cards[0]);
    const Vec2 center{bounds.centerX(), bounds.centerY()};
    Require(HitTestCardInOrder(context.cards, center) == 0, "stack hit test did not reach the front card");
    Require(CycleCardStack(context.cards, context.settings, 0, 1), "stack cycle was rejected");
    Require(context.cards[1].stack.slot == 0, "cycle did not promote the second card");
    for (size_t index = 0; index < 3; ++index) {
        SameRect(StackWorldRect(context.cards[index]), anchor);
    }
    SameRect(CardScreenBounds(context.cards[1]), bounds);
}

static void StackZOrderFront() {
    UseStackSettings();
    context.cards.clear();
    // Fingerprint order disagrees with Z-order on purpose: a#1 is first
    // by fingerprint but last on the desktop, c#1 is topmost.
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    context.cards.back().zOrder = 2;
    AddStackCard(L"b#1", L"C:\\App\\Editor.exe");
    context.cards.back().zOrder = 1;
    AddStackCard(L"c#1", L"C:\\App\\Editor.exe");
    context.cards.back().zOrder = 0;
    ReconcileCardStacks(context.cards, context.settings);
    std::vector<size_t> members;
    Require(StackMemberOrder(context.cards, L"editor.exe", members), "stack order was rejected");
    Require(members.size() == 3 && members[0] == 2 && members[1] == 1 && members[2] == 0, "stack did not fan in Z-order");
    Require(context.cards[2].stack.slot == 0, "topmost window is not the stack front");
    Near(context.cards[2].stack.rotationRadians, 0.0f);
    Near(context.cards[2].stack.scale, 1.0f);
    Require(!IsStackBackground(context.cards[2]), "topmost window was marked as background");
    Require(context.cards[1].stack.slot == 1 && context.cards[0].stack.slot == 2, "background slots do not follow Z-order");
    Require(context.cards[1].stack.rotationRadians != 0.0f, "background card lost its fan rotation");
    const std::vector<size_t> order = CardDrawOrder(context.cards);
    Require(order.size() == 3 && order.back() == 2, "stack front was not drawn last");
    // A front chosen by cycling survives a rescan: Z-order only picks
    // the front for a newly formed stack.
    Require(CycleCardStack(context.cards, context.settings, 2, 1), "forward cycle was rejected");
    Require(context.cards[1].stack.slot == 0, "cycle did not promote the next card");
    ReconcileCardStacks(context.cards, context.settings);
    Require(context.cards[1].stack.slot == 0, "rescan reverted the cycled front");
    Require(context.cards[2].stack.slot == 2, "rescan did not keep the old front at the back");
}

static void StackWindowSizes() {
    PrepareGeometry();
    UseStackSettings();
    ScannedWindow wide = SampleWindow(0x601);
    wide.fingerprint = L"wide#1";
    ScannedWindow narrow = SampleWindow(0x602);
    narrow.fingerprint = L"narrow#1";
    narrow.framePx = RECT{0, 0, 240, 600};
    narrow.zOrder = 1;
    MergeScan({wide, narrow});
    SameRect(StackWorldRect(context.cards[0]), context.cards[0].world);
    const RectF anchor = context.cards[0].stack.anchorWorld;
    Require(CycleCardStack(context.cards, context.settings, 0, 1), "cannot cycle to narrow card");
    const RectF narrowWorld = StackWorldRect(context.cards[1]);
    Near(narrowWorld.centerX(), anchor.centerX());
    Near(narrowWorld.width(), 240.0f);
    Near(narrowWorld.height(), 600.0f);
    const SnapResult snap = ComputeSnap(narrowWorld, 1);
    Require(!snap.snapVertical && !snap.snapHorizontal, "drag snapped to its own stack");

    narrow.framePx = RECT{0, 0, 900, 320};
    MergeScan({wide, narrow});
    const RectF resized = StackWorldRect(context.cards[1]);
    Near(resized.centerX(), anchor.centerX());
    Near(resized.centerY(), anchor.centerY());
    Near(resized.width(), 900.0f);
    Near(resized.height(), 320.0f);
    ToggleCardGroup(1);
    Require(!RectsOverlap(context.cards[0].world, context.cards[1].world),
            "different-width cards overlapped after split");
    Near(context.cards[0].world.width(), 640.0f);
    Near(context.cards[1].world.width(), 900.0f);
    ToggleCardGroup(1);
    const RectF beforeRemoval = StackWorldRect(context.cards[1]);
    MergeScan({narrow});
    Require(context.cards[0].stack.anchorWorld.empty() && context.cards[0].stack.grouped,
            "lone card kept an anchor or lost its grouping preference");
    SameRect(StackWorldRect(context.cards[0]), beforeRemoval);
}

static void StackApplicationIds() {
    UseStackSettings();
    ScannedWindow first = SampleWindow(0x601);
    first.exePath = L"C:\\Windows\\ApplicationFrameHost.exe";
    first.fingerprint = L"frame#1";
    first.applicationKey = L"aumid:calculator";
    ScannedWindow second = first;
    second.hwnd = reinterpret_cast<HWND>(0x602);
    second.fingerprint = L"frame#2";
    second.applicationKey = L"aumid:settings";
    ScannedWindow third = first;
    third.hwnd = reinterpret_cast<HWND>(0x603);
    third.fingerprint = L"frame#3";
    MergeScan({first, second, third});
    Require(SameActiveStack(context.cards[0], context.cards[2]),
            "same application IDs did not stack");
    Require(!IsGroupedStackCard(context.cards[1]) && !context.cards[1].hasApplicationPeers,
            "different application IDs stacked because they share an executable");
    third.applicationKey = second.applicationKey;
    Require(MergeScan({first, second, third}), "app identity change did not invalidate stacks");
    Require(!IsGroupedStackCard(context.cards[0]) &&
                SameActiveStack(context.cards[1], context.cards[2]),
            "app identity change did not reconcile peer groups");
}

static void StackDrawOrderGroups() {
    UseStackSettings();
    context.cards.clear();
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"b#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"c#1", L"C:\\App\\Player.exe");
    AddStackCard(L"d#1", L"C:\\App\\Player.exe");
    Card looseCard;
    looseCard.fingerprint = L"loose#1";
    looseCard.world = MakeRectF(0.0f, 0.0f, 320.0f, 240.0f);
    looseCard.pinned = true;
    context.cards.push_back(std::move(looseCard));
    ReconcileCardStacks(context.cards, context.settings);
    const std::vector<size_t> order = CardDrawOrder(context.cards);
    Require(order.size() == 5, "draw order lost a card");
    Require(order[0] == 1 && order[1] == 3 && order[2] == 4,
            "stack backgrounds were not drawn before pinned cards");
    Require(order[3] == 0 && order[4] == 2,
            "stack fronts were not drawn above pinned cards");
}

static void StackWheelCycle() {
    Require(StackWheelSteps(-120) == 1, "wheel down did not map to the next card");
    Require(StackWheelSteps(120) == -1, "wheel up did not map to the previous card");
    Require(StackWheelSteps(-240) == 2, "two wheel-down detents did not map to two steps");
    Require(StackWheelSteps(0) == 0, "zero wheel delta mapped to a cycle step");
    Require(StackWheelSteps(60) == 0, "sub-detent wheel delta mapped to a cycle step");
    UseStackSettings();
    context.cards.clear();
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"b#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"c#1", L"C:\\App\\Editor.exe");
    ReconcileCardStacks(context.cards, context.settings);
    Require(!CycleStackForWheel(context.cards, context.settings, 0, 0), "zero wheel delta cycled the stack");
    Require(CycleStackForWheel(context.cards, context.settings, 0, -120), "wheel down was rejected");
    Require(context.cards[1].stack.slot == 0, "wheel down did not bring the next card to the front");
    Require(CycleStackForWheel(context.cards, context.settings, 1, 120), "wheel up was rejected");
    Require(context.cards[0].stack.slot == 0, "wheel up did not bring the previous card back");
    Require(!CycleStackForWheel(context.cards, context.settings, -1, -120), "wheel cycle accepted a bad hit index");
    context.cards.clear();
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    ReconcileCardStacks(context.cards, context.settings);
    Require(!CycleStackForWheel(context.cards, context.settings, 0, -120), "wheel cycle accepted a single-card stack");
}

static void StackCycleAnimationFrames() {
    UseStackSettings();
    context.settings.animationMs = 300;
    context.cards.clear();
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"b#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"c#1", L"C:\\App\\Editor.exe");
    ReconcileCardStacks(context.cards, context.settings);
    std::vector<StackCycleTransition> transitions;
    Require(CycleStackWithAnimation(context.cards,
                StackCycleRequest{context.settings, 0, 1, 100}, transitions),
            "animated cycle was rejected");
    Require(transitions.size() == 1, "animated cycle did not keep its transition");
    Require(!CycleStackWithAnimation(context.cards,
                StackCycleRequest{context.settings, 0, 0, 110}, transitions),
            "zero steps triggered an animation");
    Require(transitions.size() == 1,
            "zero steps changed the pending transition");
    Require(context.cards[0].stack.slot == 2 && context.cards[1].stack.slot == 0,
            "animated cycle did not immediately exchange stack slots");
    Require(context.cards[0].stack.drawAboveStack,
            "outgoing front was not rendered above the stack at animation start");
    Require(CardDrawOrder(context.cards).back() == 0,
            "outgoing card did not remain visually on top during extraction");
    Near(context.cards[1].stack.opacity, 0.0f);

    Require(AdvanceStackCycleTransitions(context.cards, transitions, 250),
            "midpoint transition became inactive");
    Require(!context.cards[0].stack.drawAboveStack,
            "outgoing card stayed above the stack after extraction");
    Require(CardDrawOrder(context.cards).back() == 1,
            "incoming card was not rendered on top after extraction");
    Near(context.cards[0].stack.opacity, 0.0f);
    Near(context.cards[1].stack.opacity, 0.5f);
    Near(context.cards[1].stack.rotationRadians,
         -3.25f * (3.14159265f / 180.0f));

    Require(AdvanceStackCycleTransitions(context.cards, transitions, 295),
            "fade transition became inactive before settling");
    Near(context.cards[0].stack.opacity, 0.0f);
    Near(context.cards[1].stack.opacity, 1.0f);
    Require(AdvanceStackCycleTransitions(context.cards, transitions, 340),
            "outgoing card fade-in became inactive before completion");
    Near(context.cards[0].stack.opacity, 3.0f / 7.0f);

    Require(!AdvanceStackCycleTransitions(context.cards, transitions, 400),
            "completed transition remained active");
    Require(transitions.empty(), "completed transition was not removed");
    Require(!context.cards[0].stack.drawAboveStack &&
                !context.cards[1].stack.drawAboveStack,
            "completed transition left a temporary draw order");
    Near(context.cards[1].stack.opacity, 1.0f);
    Near(context.cards[1].stack.rotationRadians, 0.0f);
}

static void StackCycleAnimationInterrupt() {
    UseStackSettings();
    context.settings.animationMs = 300;
    context.cards.clear();
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"b#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"c#1", L"C:\\App\\Editor.exe");
    ReconcileCardStacks(context.cards, context.settings);
    std::vector<StackCycleTransition> transitions;
    Require(CycleStackWithAnimation(context.cards,
                StackCycleRequest{context.settings, 0, 1, 100}, transitions),
            "first animated cycle was rejected");
    AdvanceStackCycleTransitions(context.cards, transitions, 175);
    const float interruptedRotation = context.cards[1].stack.rotationRadians;
    Require(CycleStackWithAnimation(context.cards,
                StackCycleRequest{context.settings, 1, 1, 175}, transitions),
            "interrupted animated cycle was rejected");
    Require(transitions.size() == 1, "interruption kept a stale transition");
    Near(transitions.front().cards.front().fromRotation,
         context.cards[1].stack.rotationRadians);
    Near(context.cards[1].stack.rotationRadians, interruptedRotation);
    Require(context.cards[2].stack.slot == 0,
            "second wheel event did not continue from the current stack order");
    Require(context.cards[1].stack.drawAboveStack,
            "new outgoing card was not raised during the interrupted cycle");
}

static void StackCycleAnimationClearedOnRescan() {
    UseStackSettings();
    context.settings.animationMs = 300;
    std::vector<ScannedWindow> scanned{SampleWindow(0x100), SampleWindow(0x200)};
    AssignFingerprints(scanned);
    Require(MergeScan(scanned), "initial stack scan did not add cards");
    Require(context.cards.size() == 2, "initial stack scan lost a card");
    Require(CycleStackWithAnimation(context.cards,
                StackCycleRequest{context.settings, 0, 1, 100},
                context.stackTransitions),
            "animated cycle was rejected");
    Require(!context.stackTransitions.empty(), "cycle did not create a transition");

    Require(MergeScan(std::vector<ScannedWindow>{scanned[1]}),
            "rescan did not remove the closed window");
    Require(context.stackTransitions.empty(), "rescan retained stale stack animation");
    Require(context.cards.size() == 1 && context.cards[0].stack.slot == 0,
            "remaining card was not reconciled as the stack front");
    Near(context.cards[0].stack.opacity, 1.0f);
    Require(!context.cards[0].stack.drawAboveStack,
            "rescan retained temporary draw order");
}

static void StackCycleAnimationDisabled() {
    UseStackSettings();
    context.settings.animationMs = 0;
    context.cards.clear();
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"b#1", L"C:\\App\\Editor.exe");
    ReconcileCardStacks(context.cards, context.settings);
    std::vector<StackCycleTransition> transitions;
    Require(CycleStackWithAnimation(context.cards,
                StackCycleRequest{context.settings, 0, 1, 100}, transitions),
            "zero-duration cycle was rejected");
    Require(transitions.empty(), "zero-duration cycle created an animation");
    Require(context.cards[1].stack.slot == 0 &&
                CardDrawOrder(context.cards).back() == 1,
            "zero-duration cycle did not immediately render the new front");
    for (const Card& card : context.cards) {
        Near(card.stack.opacity, 1.0f);
        Require(!card.stack.drawAboveStack,
                "zero-duration cycle left the outgoing card raised");
    }
}

static void LayoutGroupedRoundtrip() {
    const LayoutSnapshot legacy = ParseLayout(
        R"({"version":1,"entries":[{"key":"a","x":0,"y":0,"w":1,"h":1}]})");
    Require(legacy.entries.size() == 1 && legacy.entries[0].grouped, "legacy layout lost grouping");
    Require(SerializeLayout(legacy).find("is_grouped") == std::string::npos,
            "grouped entry emitted is_grouped");
    const LayoutSnapshot ungrouped = ParseLayout(
        R"({"version":1,"entries":[{"key":"a","x":0,"y":0,"w":1,"h":1,"is_grouped":0}]})");
    Require(ungrouped.entries.size() == 1 && !ungrouped.entries[0].grouped,
            "is_grouped zero was not parsed");
    Require(SerializeLayout(ungrouped).find("\"is_grouped\":0") != std::string::npos,
            "ungrouped entry was not serialized");
    const LayoutSnapshot groupedOne = ParseLayout(
        R"({"version":1,"entries":[{"key":"a","x":0,"y":0,"w":1,"h":1,"is_grouped":1}]})");
    Require(groupedOne.entries.size() == 1 && groupedOne.entries[0].grouped,
            "is_grouped one was not parsed");
    Require(ParseLayout(R"({"version":1,"entries":[{"key":"a","x":0,"y":0,"w":1,"h":1,"is_grouped":2}]})").entries.empty(),
            "out-of-range is_grouped was accepted");
    Require(ParseLayout(R"({"version":1,"entries":[{"key":"a","x":0,"y":0,"w":1,"h":1,"is_grouped":0,"is_grouped":1}]})").entries.empty(),
            "duplicate is_grouped was accepted");
    Require(ParseLayout(R"({"version":1,"entries":[{"key":"a","x":0,"y":0,"w":1,"h":1,"is_grouped":"0"}]})").entries.empty(),
            "string is_grouped was accepted");
    context.layout = LayoutSnapshot{};
    RememberLayoutPin(L"g#1", MakeRectF(0.0f, 0.0f, 10.0f, 10.0f), true, false);
    Require(context.layout.entries.size() == 1 && !context.layout.entries[0].grouped,
            "remembered pin lost its ungrouped flag");
    Require(context.layout.entries[0].pinned, "remembered pin lost its pin flag");
    RememberLayoutEntry(L"g#1", MakeRectF(5.0f, 5.0f, 10.0f, 10.0f));
    Require(context.layout.entries.size() == 1 && !context.layout.entries[0].grouped,
            "layout move restored grouping");
    SameRect(context.layout.entries[0].world, MakeRectF(5.0f, 5.0f, 10.0f, 10.0f));
}

// Two windows of one executable get the extra group button; a lone window and
// an out-of-range index keep the two-button row.
static void HeaderThreeButtons() {
    UseStackSettings();
    context.cards.clear();
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"b#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"c#1", L"C:\\App\\Player.exe");
    context.cards[1].stack.grouped = false;
    ReconcileCardStacks(context.cards, context.settings);
    Require(CardHeaderButtonCount(context.cards, 0, true) == 3,
            "a grouped candidate lost its group button");
    Require(CardHeaderButtonCount(context.cards, 1, true) == 3,
            "the second member lost its group button");
    Require(CardHeaderButtonCount(context.cards, 2, true) == 2,
            "a lone window gained a group button");
    Require(CardHeaderButtonCount(context.cards, 3, true) == 2,
            "an out-of-range index gained a group button");
    Require(CardHeaderButtonCount(context.cards, 0, false) == 2,
            "a disabled stack kept a group button");

    const float scale = ChromeScale();
    const RectF header = CardHeaderRect(context.cards[0]);
    const RectF close = CardCloseButtonRect(header, scale, 3);
    const RectF pin = CardPinButtonRect(header, scale, 3);
    const RectF group = CardGroupButtonRect(header, scale, 3);
    Require(!close.empty() && !pin.empty() && !group.empty(),
            "the three-button row is missing a button");
    Require(!RectsOverlap(close, pin) && !RectsOverlap(pin, group) &&
                !RectsOverlap(group, close),
            "header buttons overlap");
    Require(close.right <= header.right && group.left >= header.left,
            "a header button escaped the strip");
    Near(group.right, pin.left - kCardHeaderButtonGapPx * scale);
    Require(CardGroupButtonRect(header, scale, 2).empty(),
            "a two-button row drew a group button");
    Require(HitTestCardHeader(header, Vec2{close.centerX(), close.centerY()},
                              scale, 3) == CardHeaderAction::Close,
            "close button did not hit in a three-button row");
    Require(HitTestCardHeader(header, Vec2{pin.centerX(), pin.centerY()}, scale,
                              3) == CardHeaderAction::Pin,
            "pin button did not hit in a three-button row");
    Require(HitTestCardHeader(header, Vec2{group.centerX(), group.centerY()},
                              scale, 3) == CardHeaderAction::Group,
            "group button did not hit");

    // A 200 px card fits close and pin but not the third button, so the row
    // drops the group button instead of colliding with the title.
    Card narrow;
    narrow.world = MakeRectF(0.0f, 0.0f, 200.0f, 300.0f);
    const RectF narrowHeader = CardHeaderRect(narrow);
    const int narrowButtons =
        CardHeaderButtonCountForWidth(narrowHeader, scale, 3);
    Require(narrowButtons == 2,
            "a 200 px card did not drop the group button");
    Require(!CardCloseButtonRect(narrowHeader, scale, narrowButtons).empty(),
        "a 200 px card lost the buttons it can still fit");
    narrow.world = MakeRectF(0.0f, 0.0f, 110.0f, 300.0f);
    const RectF tinyHeader = CardHeaderRect(narrow);
    Require(CardCloseButtonRect(tinyHeader, scale, 2).empty(),
            "a 110 px card kept header buttons");
}

// Ungrouping fans the members out from the stack anchor and saves the
// ungrouped flag, so a rescan keeps them apart; grouping them again restores
// the stack and clears the saved flag.
static void GroupTogglePersistence() {
    UseStackSettings();
    context.cards.clear();
    context.layout = LayoutSnapshot{};
    context.stackTransitions.clear();
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"b#1", L"C:\\App\\Editor.exe");
    ReconcileCardStacks(context.cards, context.settings);
    Require(IsGroupedStackCard(context.cards[0]) &&
                IsGroupedStackCard(context.cards[1]),
            "two windows of one executable did not group");
    Require(!context.cards[0].stack.anchorWorld.empty(),
            "the formed group lost its anchor");

    const RectF anchor = context.cards[0].stack.anchorWorld;
    ToggleCardGroup(0);
    Require(!IsGroupedStackCard(context.cards[0]) &&
                !IsGroupedStackCard(context.cards[1]),
            "ungrouping left the cards stacked");
    Require(context.cards[0].stack.groupKey.empty() &&
                context.cards[1].stack.groupKey.empty(),
            "an ungrouped card kept its group key");
    Require(!RectsOverlap(context.cards[0].world, context.cards[1].world),
            "ungrouped cards still overlap");
    Near(context.cards[0].world.left, anchor.left);
    Near(context.cards[1].world.left,
         anchor.left + anchor.width() + static_cast<float>(context.settings.gap));
    Require(SerializeLayout(context.layout).find("\"is_grouped\":0") !=
                std::string::npos,
            "the ungrouped state was not saved");

    ToggleCardGroup(0);
    Require(IsGroupedStackCard(context.cards[0]) &&
                IsGroupedStackCard(context.cards[1]),
            "regrouping did not stack the windows again");
    Require(context.cards[0].stack.slot == 0 && context.cards[1].stack.slot == 1,
            "regrouping assigned the wrong slots");
    Require(context.cards[0].stack.groupKey == context.cards[1].stack.groupKey,
            "regrouped cards disagree on their key");
    Require(SerializeLayout(context.layout).find("is_grouped") == std::string::npos,
            "the regrouped state was still saved as ungrouped");
}

// A window of the same executable that the user kept apart shares the group
// key but not the stack, so ungrouping must leave it where it stands instead of
// dragging it into the fan beside the released windows.
static void UngroupKeepsLooseWindow() {
    UseStackSettings();
    context.cards.clear();
    context.layout = LayoutSnapshot{};
    context.stackTransitions.clear();
    AddStackCard(L"a#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"b#1", L"C:\\App\\Editor.exe");
    AddStackCard(L"loose#1", L"C:\\App\\Editor.exe");
    context.cards[2].stack.grouped = false;
    context.cards[2].stack.groupKey.clear();
    const RectF looseWorld = MakeRectF(900.0f, 400.0f, 320.0f, 240.0f);
    context.cards[2].world = looseWorld;
    ReconcileCardStacks(context.cards, context.settings);
    Require(IsGroupedStackCard(context.cards[0]) &&
                IsGroupedStackCard(context.cards[1]),
            "the two windows did not form a stack");
    Require(!IsGroupedStackCard(context.cards[2]),
            "the loose window was pulled into the stack");

    ToggleCardGroup(0);
    Require(!IsGroupedStackCard(context.cards[0]) &&
                !IsGroupedStackCard(context.cards[1]),
            "ungrouping left the cards stacked");
    SameRect(context.cards[2].world, looseWorld);
    Require(!RectsOverlap(context.cards[0].world, context.cards[1].world),
            "the released windows still overlap");
}

// Closing a card reaches the real window. Without a live window the card falls
// back to a local removal; with one WM_CLOSE is posted and the card stays until
// a rescan reports the window gone.
static void CloseRequestFallback() {
    context.cards.clear();
    context.scanRequested = false;
    AddStackCard(L"closeme#1", L"C:\\App\\Editor.exe");
    CloseCardWindow(0);
    Require(context.cards.empty(), "a card without a window was not dismissed");
    Require(context.scanRequested, "removing a stale card did not request a scan");
    CloseCardWindow(-1);
    CloseCardWindow(99);
    Require(context.cards.empty(), "an out-of-range close changed the cards");
    ScannedWindow replacement = SampleWindow(0x600);
    replacement.fingerprint = L"closeme#1";
    Require(MergeScan({replacement}) && context.cards.size() == 1,
            "the replacement window's reused fingerprint was suppressed");

    // A window with no message pump keeps its queue, so a posted WM_CLOSE can
    // be observed without closing it. Skip the block when the session cannot
    // create a window at all.
    const HWND probe = CreateWindowExW(0, L"STATIC", L"AlephCloseProbe", WS_POPUP,
                                       0, 0, 120, 80, nullptr, nullptr, nullptr,
                                       nullptr);
    if (probe == nullptr) {
        return;
    }
    context.cards.clear();
    context.scanRequested = false;
    AddStackCard(L"live#1", L"C:\\App\\Editor.exe");
    context.cards[0].hwnd = probe;
    context.cards[0].threadId = GetWindowThreadProcessId(probe, &context.cards[0].processId);
    CloseCardWindow(0);
    Require(context.cards.size() == 1,
            "a live window was dismissed before it closed");
    Require(context.scanRequested, "a live close did not request a rescan");
    Require(IsWindow(probe) != FALSE, "the probe window closed itself");
    DestroyWindow(probe);
}

struct NativeCase {
    const char* name;
    void (*run)();
};

static constexpr NativeCase kCases[] = {
    {"snap_positive", SnapPositive}, {"snap_negative", SnapNegative},
    {"snap_disabled", SnapDisabled}, {"snap_unmatched", SnapUnmatched},
    {"zoom_anchor", ZoomAnchor}, {"minimap_center", MinimapCenter},
    {"minimap_empty", MinimapEmpty}, {"json_malformed", JsonMalformed},
    {"layout_save_disabled", LayoutSaveDisabled},
    {"json_finite", JsonFinite}, {"json_order", JsonOrder},
    {"json_roundtrip", JsonRoundtrip}, {"hotkey_suffix", HotkeySuffix},
    {"fingerprint_lifetime", FingerprintLifetime}, {"window_identity", WindowIdentity},
    {"fingerprint_ordinals", FingerprintOrdinalStability},
    {"scan_candidate_selection", ScanCandidateSelection},
    {"header_chrome", HeaderChrome}, {"pinned_order", PinnedOrder},
    {"pinned_layout_sync", PinnedLayoutSync}, {"pin_persistence", PinPersistence},
    {"layout_entry_eviction", LayoutEntryEviction},
    {"pin_survives_layout_move", PinSurvivesLayoutMove},
    {"layout_recency_order", LayoutRecencyOrder},
    {"layout_pinned_capacity", LayoutPinnedCapacity},
    {"desktop_pull_moves", DesktopPullMoves},
    {"desktop_pull_same", DesktopPullSame},
    {"desktop_pull_null", DesktopPullNull},
    {"desktop_pull_reference_failure", DesktopPullReferenceFailure},
    {"desktop_pull_rejected", DesktopPullRejected},
    {"icon_source_order", IconSourceOrder},
    {"icon_source_largest", IconSourceLargestWins},
    {"icon_source_fallback", IconSourceFallback},
    {"icon_source_missing", IconSourceMissing},
    {"localization_strings", LocalizationStrings},
    {"localization_badges", LocalizationBadges},
    {"localization_codes", LocalizationCodes},
    {"localization_fallback", LocalizationFallback},
    {"localization_out_of_range", LocalizationOutOfRange},
    {"localization_badge_selection", LocalizationBadgeSelection},
    {"stack_geometry", StackGeometry}, {"stack_cycle", StackCycle},
    {"stack_removal", StackRemoval}, {"stack_invalid", StackInvalid},
    {"stack_same_exe_name_groups", StackSameExeNameGroups},
    {"stack_window_sizes", StackWindowSizes},
    {"stack_app_ids", StackApplicationIds},
    {"hotkey_default_and_disable", HotkeyDefaultAndDisable},
    {"hotkey_registration_recovery", HotkeyRegistrationConflictAndRecovery},
    {"hotkey_retry_backoff", HotkeyRetryBackoff},
    {"stack_z_order_front", StackZOrderFront},
    {"stack_draw_order_groups", StackDrawOrderGroups},
    {"stack_wheel_cycle", StackWheelCycle},
    {"stack_cycle_animation_frames", StackCycleAnimationFrames},
    {"stack_cycle_animation_interrupt", StackCycleAnimationInterrupt},
    {"stack_cycle_animation_rescan", StackCycleAnimationClearedOnRescan},
    {"stack_cycle_animation_disabled", StackCycleAnimationDisabled},
    {"stack_depth_visibility", StackDepthVisibility},
    {"stack_hit_rotation", StackHitRotation},
    {"stack_thumbnail_occlusion", StackThumbnailOcclusionClips},
    {"stack_front_occlusion_full_stack", StackFrontOcclusionFullStack},
    {"layout_grouped_roundtrip", LayoutGroupedRoundtrip},
    {"header_three_buttons", HeaderThreeButtons},
    {"group_toggle_persistence", GroupTogglePersistence},
    {"ungroup_keeps_loose_window", UngroupKeepsLooseWindow},
    {"close_request_fallback", CloseRequestFallback},
};

static int RunCase(const char* name) {
    for (const NativeCase& entry : kCases) {
        if (std::strcmp(entry.name, name) != 0) {
            continue;
        }
        PrepareGeometry();
        entry.run();
        std::cout << "PASS " << name << '\n';
        return 0;
    }
    throw std::runtime_error(std::string("unknown native case: ") + name);
}

int main(int count, char** arguments) {
    try {
        Require(count == 2, "provide one native case name");
        return RunCase(arguments[1]);
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
