// ==WindhawkMod==
// @id              infinite-window-canvas
// @name            Infinite Window Canvas
// @name:zh-CN      无限窗口画布
// @description     Pans ordinary windows as a virtual canvas when the pointer dwells at a monitor edge.
// @description:zh-CN 鼠标在屏幕边缘停留时，将普通窗口作为虚拟画布连续平移。
// @version         1.0.0
// @author          CYoJkoY
// @github          https://github.com/CYoJkoY
// @license         MIT
// @include         windhawk.exe
// @compilerOptions -lshell32 -lshcore
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
- Per-monitor window collection with shell, popup, tool, hidden, minimized, and fullscreen exclusions.
- Optional process blacklist for windows that should never move.
- Drag-aware layout tracking: automatic panning pauses during a window drag and records the new position afterward.
- Configurable reset hotkey, defaulting to Ctrl+Alt+Win+R.
- Optional bounds that keep at least part of the managed window layout on-screen.

The layout is created from the current window positions each time the mod starts.
Unloading the mod leaves windows at their current positions.

## Settings

Reset key accepts one ASCII letter or digit. The modifier checkboxes define the
global reset hotkey. Process blacklist contains one executable name per entry,
such as notepad.exe.
*/
// ==WindhawkModReadme==

// ==WindhawkModSettings==
/*
- Behavior:
    - edge_distance: 48
      $name: Edge trigger distance
      $name:zh-CN: 边缘触发距离
      $description: Distance from a monitor edge in pixels that starts the dwell timer.
      $description:zh-CN: 距离显示器边缘多少像素时开始计算停留时间。
    - dwell_time: 350
      $name: Edge dwell time
      $name:zh-CN: 边缘停留时间
      $description: Time in milliseconds the pointer must remain near an edge before panning.
      $description:zh-CN: 鼠标在边缘附近停留多少毫秒后开始平移。
    - max_speed: 900
      $name: Maximum pan speed
      $name:zh-CN: 最大平移速度
      $description: Maximum canvas speed in pixels per second.
      $description:zh-CN: 画布最大平移速度，单位为每秒像素数。
    - easing: 35
      $name: Edge easing
      $name:zh-CN: 边缘缓动
      $description: Higher values make movement accelerate more gradually toward the edge.
      $description:zh-CN: 数值越高，靠近边缘中心时的加速越平滑。
    - constrain_bounds: true
      $name: Keep part of layout visible
      $name:zh-CN: 保持部分布局可见
      $description: Prevent the complete managed layout from leaving the monitor.
      $description:zh-CN: 防止全部受管窗口同时离开显示器。
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

- Advanced:
    - process_blacklist: [""]
      $name: Process blacklist
      $name:zh-CN: 进程黑名单
      $description: Executable names to exclude, for example notepad.exe. One name per entry.
      $description:zh-CN: 要排除的可执行文件名，例如 notepad.exe，每行一个。
  $name: Advanced
  $name:zh-CN: 高级
*/
// ==WindhawkModSettings==
