// ==WindhawkMod==
// @id              infinite-window-canvas
// @name            Infinite Window Canvas
// @name:zh-CN      无限窗口画布
// @description     Pans ordinary windows as a virtual canvas at monitor edges, with a picker to jump to any window.
// @description:zh-CN 鼠标在屏幕边缘停留时将普通窗口作为虚拟画布平移，并可用窗口选择器快速跳转。
// @version         1.1.0
// @author          CYoJkoY
// @github          https://github.com/CYoJkoY
// @license         MIT
// @include         windhawk.exe
// @compilerOptions -lgdi32 -lshell32 -lshcore
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Infinite Window Canvas

Infinite Window Canvas turns ordinary windows on the monitor under the pointer
into a pannable virtual canvas. Dwell near a monitor edge and the windows move
together, exposing the layout beyond the physical screen boundary without
requiring Alt+Tab.

The pointer remains inside the physical desktop. Windows are moved normally by
Windows; this mod does not copy, mirror, or render window contents off-screen.

## Features

- Edge dwell panning with configurable edge distance, dwell time, speed, and easing.
- Panning starts only at outer monitor edges; shared edges keep switching displays.
- Quick jump picker that centers any managed window on the monitor, opened with Ctrl+Alt+Win+Q by default.
- Per-monitor window collection with shell, popup, tool, hidden, minimized, and fullscreen exclusions.
- Optional process blacklist for windows that should never move.
- Drag-aware layout tracking: automatic panning pauses during a window drag and records the new position afterward.
- Configurable reset hotkey, defaulting to Ctrl+Alt+Win+R.
- Optional bounds that keep at least part of the managed window layout on-screen.

The layout is created from the current window positions each time the mod starts.
Unloading the mod leaves windows at their current positions.

## Usage

Move the pointer toward a monitor edge. Once the dwell time passes, the windows
pan in that direction. The speed rises as the pointer gets closer to the edge;
leaving the edge stops the pan immediately.

Press the quick jump hotkey to open a picker listing every managed window. Use
the mouse, Enter, or the number keys 1 to 9 to pick a window, and Esc to cancel.
The chosen window is centered on the monitor and brought to the foreground.

## Settings

Reset key accepts one ASCII letter or digit. The modifier checkboxes define the
global reset hotkey. The quick jump hotkey is a whole combination such as
Ctrl+Alt+Win+Q or Ctrl+Shift+F9; leave it empty to disable the picker. Process
blacklist contains one executable name per entry, such as notepad.exe.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- Behavior:
    - edge_distance: 48
      $name: Edge trigger distance
      $name:zh-CN: 边缘触发距离
      $description: Distance from a monitor edge in pixels that starts the dwell timer.
      $description:zh-CN: 距离显示器边缘多少像素时开始计算停留时间。
    - dwell_time: 250
      $name: Edge dwell time
      $name:zh-CN: 边缘停留时间
      $description: Time in milliseconds the pointer must remain near an edge before panning.
      $description:zh-CN: 鼠标在边缘附近停留多少毫秒后开始平移。
    - max_speed: 1800
      $name: Maximum pan speed
      $name:zh-CN: 最大平移速度
      $description: Maximum canvas speed in pixels per second at the outer screen edge.
      $description:zh-CN: 鼠标贴住屏幕最外侧时的画布最大平移速度，单位为每秒像素数。
    - easing: 20
      $name: Edge easing
      $name:zh-CN: 边缘缓动
      $description: Higher values start slower near the trigger boundary; lower values respond sooner.
      $description:zh-CN: 数值越高，刚进入触发区域时越慢；数值越低，响应越直接。
    - constrain_bounds: false
      $name: Keep part of layout visible
      $name:zh-CN: 保持部分布局可见
      $description: Stop panning once the whole managed layout leaves the monitor. Disable for an unbounded canvas.
      $description:zh-CN: 当全部受管窗口离开显示器后停止平移。关闭后画布可以无限延伸。
  $name: Behavior
  $name:zh-CN: 行为

- Reset:
    - reset_key: R
      $name: Reset key
      $name:zh-CN: 重置按键
      $description: One ASCII letter or digit used with the selected modifiers.
      $description:zh-CN: 与所选修饰键组合使用的一个 ASCII 字母或数字。
    - ctrl: true
      $name: Ctrl
      $name:zh-CN: Ctrl
    - alt: true
      $name: Alt
      $name:zh-CN: Alt
    - win: true
      $name: Windows key
      $name:zh-CN: Windows 键
    - shift: false
      $name: Shift
      $name:zh-CN: Shift
  $name: Reset hotkey
  $name:zh-CN: 重置热键

- QuickJump:
    - jump_hotkey: Ctrl+Alt+Win+Q
      $name: Quick jump hotkey
      $name:zh-CN: 快速跳转热键
      $description: Opens the window picker. Empty disables it. Format like Ctrl+Alt+Win+Q.
      $description:zh-CN: 打开窗口选择器。留空可禁用。示例：Ctrl+Alt+Win+Q。
  $name: Quick jump
  $name:zh-CN: 快速跳转

- Advanced:
    - process_blacklist: [""]
      $name: Process blacklist
      $name:zh-CN: 进程黑名单
      $description: Executable names to exclude, for example notepad.exe. One name per entry.
      $description:zh-CN: 要排除的可执行文件名，例如 notepad.exe，每行一个。
  $name: Advanced
  $name:zh-CN: 高级
*/
// ==/WindhawkModSettings==
