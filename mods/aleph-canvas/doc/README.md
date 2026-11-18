# Aleph Canvas

Aleph is a standalone Windhawk tool mod. It runs in Windhawk's dedicated tool
process and uses two topmost windows: a host for DWM thumbnails and input, and
a transparent layered HUD for card chrome, labels, status badges, guides, and
the minimap. No Explorer implementation details or private virtual-desktop
interfaces are used.

## Coordinate and window policy

Canvas coordinates are physical pixels relative to the virtual-screen origin.
This differs from the guide's 96-DPI world-coordinate proposal, but avoids
applying a monitor-specific scale to windows spanning monitors with different
DPI. UI chrome scales with the monitor under the pointer. The mod never moves
real windows; dragging a canvas card only changes the saved canvas layout.

The public `IVirtualDesktopManager` interface identifies windows on other
desktops. It does not expose the current desktop identifier required by
`MoveWindowToDesktop`, so double-clicking an other-desktop card uses the normal
foreground activation path instead of claiming to move the window.

Duplicate windows keep their assigned executable/class slots while alive.
After a restart, indistinguishable windows reuse available saved slots; the
mod cannot identify which document belongs to which identical window class.
Visible idle canvases rescan windows at the configured interval; they do not
redraw continuously. Hidden canvases wait for events and pending layout saves.

## Card controls

Every card reserves a 28 DIP header above the window preview. The header shows
the window title and two buttons on the right edge.

- Pin raises the card above every unpinned card and is saved with the layout,
  so a pinned card returns pinned after a restart.
- Close removes the card from the canvas until the canvas is hidden and shown
  again. The real window keeps running and is unaffected.
- Dragging the header's title area still moves the card, and double-clicking
  the header still dives into the window; the buttons themselves only perform
  their own action.

## Manual verification

1. Enable the mod, toggle the canvas, and check that live windows show DWM
   previews while minimized and other-desktop windows show icon cards.
2. Pan with the middle button or Space+drag; zoom with the wheel and verify the
   point under the pointer stays fixed.
3. Drag cards, verify edge and center snapping, then restart the tool process
   and confirm saved positions return.
4. Navigate via the minimap, select with Tab, pan with arrows, fit with Home,
   and activate with Enter or double-click.
5. Pin two cards and confirm both stay above the rest, restart the tool process
   and confirm they are still pinned. Close one card, confirm it stays gone,
   then hide and show the canvas and confirm it returns.
6. Check mixed-DPI displays, a window spanning monitors, 15 or more windows,
   and repeated enable/disable cycles. Confirm Explorer desktop positions stay
   unchanged throughout.

These runtime checks require a Windows session and are not covered by the
repository's bundler unit tests.
