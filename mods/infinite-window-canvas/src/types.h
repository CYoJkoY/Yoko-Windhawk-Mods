#pragma once

// Monitor edge that currently drives the canvas pan direction.
enum Edge {
    EDGE_LEFT,
    EDGE_TOP,
    EDGE_RIGHT,
    EDGE_BOTTOM,
};

// One ordinary top-level window tracked by the virtual canvas.
//
// origin is the window rectangle with the canvas offset removed, so it stays
// stable while the canvas pans. It is refreshed whenever the user moves the
// window by other means.
struct ManagedWindow {
    HWND hwnd = nullptr;
    RECT origin = {};
};
