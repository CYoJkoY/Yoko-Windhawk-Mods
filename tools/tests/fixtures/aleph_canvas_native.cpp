#include "mods/aleph-canvas/src/cards.h"
#include "mods/aleph-canvas/src/card_chrome.h"
#include "mods/aleph-canvas/src/math_utils.h"
#include "mods/aleph-canvas/src/minimap.h"
#include "mods/aleph-canvas/src/layout_store.h"
#include "mods/aleph-canvas/src/settings.h"

#include <array>
#include <iostream>
#include <stdexcept>

CanvasContext context;

static constexpr float kTolerance = 0.002f;

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
    window.title = L"Document";
    window.framePx = RECT{0, 0, 640, 480};
    return window;
}

static void FingerprintLifetime() {
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
    Near(header.height(), 28.0f * ChromeScale());

    const RectF close = CardCloseButtonRect(header, ChromeScale());
    const RectF pin = CardPinButtonRect(header, ChromeScale());
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
                              ChromeScale()) == CardHeaderAction::Close,
            "close button did not hit");
    Require(HitTestCardHeader(header, Vec2{pin.centerX(), pin.centerY()},
                              ChromeScale()) == CardHeaderAction::Pin,
            "pin button did not hit");
    Require(HitTestCardHeader(header, Vec2{header.left + 40.0f, header.centerY()},
                              ChromeScale()) == CardHeaderAction::None,
            "title area reported a button");
    Require(HitTestCardHeader(header,
                              Vec2{header.centerX(), header.bottom + 4.0f},
                              ChromeScale()) == CardHeaderAction::None,
            "point below the header reported a button");

    // A narrow card cannot host both buttons, so none are drawn or hit.
    Card narrow;
    narrow.world = MakeRectF(0.0f, 0.0f, 40.0f, 300.0f);
    const RectF narrowHeader = CardHeaderRect(narrow);
    Require(CardCloseButtonRect(narrowHeader, ChromeScale()).empty(),
            "narrow header kept a close button");
    Require(CardPinButtonRect(narrowHeader, ChromeScale()).empty(),
            "narrow header kept a pin button");

    // The strip is DPI relative: 28 DIP is 28 px at 96 DPI.
    context.chromeDpi = 96;
    const RectF scaled = CardHeaderRect(card);
    Near(scaled.height(), 28.0f);
    const RectF scaledClose = CardCloseButtonRect(scaled, ChromeScale());
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
    RememberLayoutPin(L"app#1", MakeRectF(10.0f, 20.0f, 300.0f, 200.0f), true);
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

static void DismissedCards() {
    ScannedWindow window = SampleWindow(0x600);
    window.fingerprint = L"dismissed#1";
    const std::vector<ScannedWindow> scanned{window};
    context.dismissedFingerprints.insert(window.fingerprint);
    MergeScan(scanned);
    Require(context.cards.empty(), "dismissed window returned during a scan");
    context.dismissedFingerprints.clear();
    MergeScan(scanned);
    Require(context.cards.size() == 1,
            "clearing the dismissal did not restore the card");
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
    {"header_chrome", HeaderChrome}, {"pinned_order", PinnedOrder},
    {"pinned_layout_sync", PinnedLayoutSync}, {"pin_persistence", PinPersistence},
    {"dismissed_cards", DismissedCards},
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
