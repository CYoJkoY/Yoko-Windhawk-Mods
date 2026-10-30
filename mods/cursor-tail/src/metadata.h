// ==WindhawkMod==
// @id              cursor-tail
// @name            Cursor Tail
// @name:zh-CN      光标拖尾
// @description     Adds a speed-reactive cursor trail with optional interaction feedback.
// @description:zh-CN 为鼠标指针添加速度响应式拖尾与可选的交互反馈。
// @version         3.23
// @author          CYoJkoY
// @github          https://github.com/CYoJkoY
// @license         MIT
// @include         windhawk.exe
// @compilerOptions -ladvapi32 -ld2d1 -lole32 -lgdi32 -lshell32 -lshcore -lwindowscodecs
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Cursor Tail

Cursor Tail adds a smooth, tapered motion-blur trail behind the mouse pointer
when it moves quickly, with optional click ripples and animated directional
scroll-wheel arrows for clearer interaction cues. All effects use a transparent
Direct2D overlay, independently of the application under the pointer.

## Preview

![Cursor Tail Demo](https://i.imgur.com/mbiW6QU.gif)

## Features

- **Speed-reactive trail:** Width, opacity, and effective length respond to pointer velocity. Trigger and stop speeds are configured in pixels per second, so they remain meaningful when the sampling rate changes.
- **Continuous fast-motion paths:** A configurable, hard-bounded interpolation step count fills large gaps between samples without allocating extra memory in the draw loop.
- **Motion accents:** Directional acceleration bursts pair a forward arrowhead with expanding speed lines. Optional turn markers trace the incoming and outgoing cursor directions, making sharp changes easier to recognize at a glance.
- **Smooth motion:** Historical cursor samples can be smoothed with Chaikin subdivision for a continuous ribbon.
- **Custom appearance:** Configure trail color, outline color, gradient, and optional glow.
- **Adaptive color:** Automatically sample the active cursor image and prefer colors with sufficient contrast against the screen background.
- **Fade-out:** Smoothly fade the trail after the pointer slows down.
- **Interaction feedback:** Optional expanding rings for left, right, and middle clicks, plus animated scroll arrows. Native horizontal/tilt-wheel input points left or right directly. Hold Shift to map vertical wheel feedback to horizontal arrows: wheel up points left and wheel down points right. These effects can run without a motion trail.
- **Pendant ornaments:** Hang a preset pendant from the pointer on a simulated rope. Choose Classic, Bouncy, Floaty, or Comet, optionally supply one image, and scale the rope length. The rope follows the pointer, swings at direction changes, and stretches while moving without decoding an animated GIF.
- **Per-app rules:** Enable or disable the trail for selected executables.
- **Fullscreen suppression:** Automatically hide the trail for exclusive or borderless fullscreen applications and restore it after fullscreen ends.

## Color modes

**Manual** mode uses the configured hexadecimal trail color. **Auto** mode samples the current cursor image and chooses a visible color based on the luminance around the pointer. Automatic resampling can be disabled or limited to a custom interval.

The outline can either be derived automatically from the trail luminance or set to a fixed hexadecimal color.

**Screen capture:** Auto mode reads the screen background around the pointer, and that sample must not pick up the overlay's own pixels. While Auto mode is active the entire overlay is therefore excluded from screen capture (`WDA_EXCLUDEFROMCAPTURE`), including click rings and directional wheel arrows. Manual color mode never reads the screen, so the overlay stays capturable and its effects show up normally in captures.

## Per-app rules

Use one rule per entry in the settings list:

`program.exe=on`

`program.exe=off`

Entries beginning with `#` are comments. Executable names are matched case-insensitively.

Rules follow the foreground window. An `exe=off` rule suppresses the trail everywhere on screen while that application has focus. An `exe=on` rule also overrides fullscreen suppression, so a force-enabled application keeps its trail even when it reports a fullscreen state.

## Fullscreen behavior

The trail is suppressed when the foreground application reports a D3D fullscreen state or when a borderless, captionless window covers the monitor under the pointer. Desktop shell windows are excluded, and a fullscreen app on another monitor does not suppress the trail where the pointer currently resides. Leaving fullscreen must remain stable for 500 ms before suppression ends, which reduces flicker during application transitions.

## Pendant ornaments

Pendant ornaments hang a preset from the pointer on a rope. Pick a preset and, optionally, one image; the rope physics, rope look, node layout, and reaction strength are built in. Each rope is a fixed set of Verlet points pinned to the pointer, so the animation starts at the cursor instead of a detached chain.

- **Classic:** a single pendant on a thin silk cord with a natural swing.
- **Bouncy:** a beaded chain that swings farther and reacts strongly to clicks and wheel input.
- **Floaty:** low gravity and high damping for a slow, gentle drift on a wide ribbon.
- **Comet:** three shrinking nodes spread along a glowing neon line.

`Pendant image` accepts one file path. PNG, JPEG, BMP, GIF, and TIFF are supported; an animated GIF is decoded to its first frame. Leave the path empty to draw a circular pendant in the current trail color.

Image metadata is read only when the settings change. Decoded pixels are uploaded once to a device bitmap and then released, and are decoded again on demand if the Direct2D target is recreated, so the pendant does not keep a full decoded copy in memory. The rope is simulated at 125 Hz with at most eight substeps per sample. Rope points, smoothing buffers, and ribbon outlines are fixed-size. A path geometry cannot be repopulated after it is closed, so each drawn trail layer creates one transient Direct2D path geometry per frame; one fill per layer replaces a stroke per trail segment.

## Performance

Mouse movement and foreground changes wake the worker from an indefinite wait during ordinary idle, avoiding periodic cursor polling. Sampling runs at 125 Hz while an effect is active. If raw input or foreground notifications are unavailable, or a higher-integrity foreground window blocks raw input, fallback polling samples every 10 ms for one second after detected movement, then backs off to 100 ms while idle. Layered-window submission is paced separately at 60 FPS while actively moving and 30 FPS while fading. Raw input reports that fall inside the short wake-throttling window are coalesced and handled by a one-shot timer, and the idle back buffer is released after five seconds. Background luminance sampling enforces a 50 ms minimum interval and also requires cursor travel before taking another screen sample. The back buffer and Direct2D resources are reused between frames and resized only when the required trail bounds grow beyond the current buffer. Growing the buffer rebinds the existing render target to the new surface, so brushes, layers, and uploaded pendant bitmaps survive the resize.

## Related mods

Cursor Tail shares the general cursor-trail space with other Windhawk mods, but focuses on event-driven sampling with pixel-per-second thresholds, a configurable two-layer ribbon, cursor-image color candidates with contrast-aware selection, per-app rules, fullscreen suppression, and optional motion-specific acceleration/turn cues. Click rings and directional wheel arrows are optional and disabled by default.

- **Sampling and timing:** input-driven wakeups with bounded sampling while active, plus a separate render cadence. Trigger and stop speeds are measured in pixels per second rather than pixels per sample.
- **Appearance:** separately configurable outer/core widths, optional gradient and glow, and automatic or manual outline colors.
- **Adaptive color:** cursor-image color extraction with histogram ranking, contrast-aware selection, and hysteresis to avoid rapid color switching near the contrast boundary.
- **Context awareness:** per-app `exe=on|off` rules and fullscreen suppression for exclusive or borderless applications.
- **Interaction layer:** optional button-specific click rings and animated scroll arrows, including native horizontal/tilt-wheel arrows and left/right arrows for Shift+vertical-wheel. Interaction events use asynchronous raw mouse input instead of a system-wide low-level mouse hook.
- **Motion cues:** bounded directional burst glyphs and incoming-to-outgoing turn markers, plus interpolation for large movement gaps. Acceleration feedback is enabled by default; sharp-turn feedback remains optional to limit visual clutter.

Comparable mods include:

- [**Cursor Motion Blur**](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/cursor-motion-blur.wh.cpp): the original basis for Cursor Tail, with a tapered motion-blur ribbon.
- [**Mouse Trail**](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/mouse-trail.wh.cpp): a ribbon-style trail with glow and cursor-extracted color; it also provides an expanding click ring.
- [**Simple Cursor Trail**](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/cursor-trail.wh.cpp): a simpler line trail with a head-to-tail gradient.
- [**Pointer Afterimage Motion Blur**](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/pointer-afterimage-motion-blur.wh.cpp): an afterimage-based motion blur with event-driven sampling.

## Attribution

This mod is derived in part from the Windhawk **Cursor Motion Blur** mod by TheatriChris. The original project is licensed under the MIT License.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- Behavior:
    - trigger_velocity: 3000
      $name: Trigger speed
      $name:zh-CN: 触发速度
      $description: Minimum pointer speed in pixels per second required to start the trail. The speed is measured from the real time between samples, so the value is independent of the sampling rate.
      $description:zh-CN: 开始显示拖尾所需的最低指针速度，单位为像素每秒。速度按采样之间的真实时间计算，因此该数值与采样率无关。
    - stop_velocity: 1200
      $name: Stop speed
      $name:zh-CN: 停止速度
      $description: Pointer speed in pixels per second below which an active trail begins fading. If this is set to the trigger speed or higher, it is automatically reduced to half the trigger speed.
      $description:zh-CN: 指针速度（像素每秒）低于此值时，正在显示的拖尾开始淡出。如果该值大于或等于触发速度，会自动降低为触发速度的一半。
    - tail_length: 10
      $name: Tail length
      $name:zh-CN: 拖尾长度
      $description: Number of cursor samples kept for the trail. Higher values make the trail longer.
      $description:zh-CN: 保留的指针采样数量。数值越大，拖尾越长。
    - tail_offset_x: 6
      $name: Trail X offset
      $name:zh-CN: 拖尾 X 偏移
      $description: Horizontal offset from the cursor hotspot to the trail head, in pixels.
      $description:zh-CN: 拖尾起点相对于指针热点的水平偏移量，单位为像素。
    - tail_offset_y: 10
      $name: Trail Y offset
      $name:zh-CN: 拖尾 Y 偏移
      $description: Vertical offset from the cursor hotspot to the trail head, in pixels.
      $description:zh-CN: 拖尾起点相对于指针热点的垂直偏移量，单位为像素。
    - speed_scaling: true
      $name: Speed-reactive shape
      $name:zh-CN: 随速度变化的形状
      $description: Increase or decrease trail width, opacity, and effective length according to pointer speed.
      $description:zh-CN: 根据指针速度动态调整拖尾宽度、不透明度和实际长度。
    - fade_enabled: true
      $name: Fade-out
      $name:zh-CN: 淡出
      $description: Smoothly fade the trail after the pointer slows down.
      $description:zh-CN: 指针减速后平滑淡出拖尾。
    - fade_decay: 90
      $name: Fade speed
      $name:zh-CN: 淡出速度
      $description: Controls how quickly the trail fades. 90 means the remaining opacity is multiplied by 0.90 for each simulation sample.
      $description:zh-CN: 控制拖尾的淡出速度。90 表示每个模拟采样都会将剩余不透明度乘以 0.90。
  $name: Behavior
  $name:zh-CN: 行为

- Appearance:
    - width_min: 4
      $name: Minimum trail width
      $name:zh-CN: 最小拖尾宽度
      $description: Half-width of the outer trail at lower speeds, in pixels.
      $description:zh-CN: 低速时外层拖尾的半宽，单位为像素。
    - width_max: 14
      $name: Maximum trail width
      $name:zh-CN: 最大拖尾宽度
      $description: Half-width of the outer trail at higher speeds, in pixels.
      $description:zh-CN: 高速时外层拖尾的半宽，单位为像素。
    - core_width_min: 2
      $name: Minimum core width
      $name:zh-CN: 最小核心宽度
      $description: Half-width of the inner core at lower speeds, in pixels.
      $description:zh-CN: 低速时内部核心的半宽，单位为像素。
    - core_width_max: 9
      $name: Maximum core width
      $name:zh-CN: 最大核心宽度
      $description: Half-width of the inner core at higher speeds, in pixels.
      $description:zh-CN: 高速时内部核心的半宽，单位为像素。
    - alpha_min: 39
      $name: Minimum opacity
      $name:zh-CN: 最小不透明度
      $description: Trail opacity at lower speeds, from 0 to 100 percent.
      $description:zh-CN: 低速时的拖尾不透明度，范围为 0 到 100%。
    - alpha_max: 77
      $name: Maximum opacity
      $name:zh-CN: 最大不透明度
      $description: Trail opacity at higher speeds, from 0 to 100 percent.
      $description:zh-CN: 高速时的拖尾不透明度，范围为 0 到 100%。
    - taper_power: 10
      $name: Tail taper
      $name:zh-CN: 拖尾渐缩
      $description: Controls how quickly the trail narrows toward its tail. 10 is linear; larger values produce a sharper taper. Range 5-30.
      $description:zh-CN: 控制拖尾向末端收窄的速度。10 为线性效果；数值越大，收窄越明显。范围 5-30。
    - smooth_iterations: 2
      $name: Smoothing iterations
      $name:zh-CN: 平滑迭代次数
      $description: Number of Chaikin subdivision passes. Higher values produce a smoother trail at the cost of more points to render. Range 0-4.
      $description:zh-CN: Chaikin 细分次数。数值越高，拖尾越平滑，但需要绘制更多点。范围 0-4。
  $name: Appearance
  $name:zh-CN: 外观

- Color:
    - trail_color_mode: manual
      $name: Trail color mode
      $name:zh-CN: 拖尾颜色模式
      $description: Choose a fixed trail color or automatically sample the cursor image.
      $description:zh-CN: 选择固定的拖尾颜色，或自动从当前指针图像采样颜色。
      $options:
        - manual: Manual color
        - auto: Auto-sample cursor colors
      $options:zh-CN:
        - manual: 手动颜色
        - auto: 自动采样指针颜色
    - trail_color_manual: "#FFFFFF"
      $name: Manual trail color
      $name:zh-CN: 手动拖尾颜色
      $description: Hexadecimal RGB color used when Trail color mode is Manual.
      $description:zh-CN: 拖尾颜色模式设为“手动颜色”时使用的十六进制 RGB 颜色。
    - outline_color_mode: auto
      $name: Outline color mode
      $name:zh-CN: 轮廓颜色模式
      $description: Choose an automatic outline color or a fixed manual color.
      $description:zh-CN: 选择自动轮廓颜色或固定的手动颜色。
      $options:
        - auto: Automatic outline
        - manual: Manual color
      $options:zh-CN:
        - auto: 自动轮廓
        - manual: 手动颜色
    - outline_color_manual: "#000000"
      $name: Manual outline color
      $name:zh-CN: 手动轮廓颜色
      $description: Hexadecimal RGB color used when Outline color mode is Manual.
      $description:zh-CN: 轮廓颜色模式设为“手动颜色”时使用的十六进制 RGB 颜色。
    - auto_resample_interval: 0
      $name: Auto-color refresh interval
      $name:zh-CN: 自动颜色刷新间隔
      $description: How often Auto color mode resamples the cursor color, in milliseconds. Set to 0 to resample only when the cursor image changes.
      $description:zh-CN: 自动颜色模式重新采样指针颜色的间隔，单位为毫秒。设为 0 时，仅在指针图像发生变化时重新采样。
  $name: Color
  $name:zh-CN: 颜色

- Effects:
    - gradient_enabled: false
      $name: Tail gradient
      $name:zh-CN: 拖尾渐变
      $description: Fade the core color toward the configured tail color.
      $description:zh-CN: 将核心颜色向设定的末端颜色渐变。
    - gradient_tail_color: "#FF00FF"
      $name: Gradient tail color
      $name:zh-CN: 渐变末端颜色
      $description: Hexadecimal RGB color used at the tail end when Tail gradient is enabled.
      $description:zh-CN: 启用拖尾渐变时，拖尾末端使用的十六进制 RGB 颜色。
    - glow_enabled: false
      $name: Glow
      $name:zh-CN: 发光
      $description: Draw an additional soft glow around the trail.
      $description:zh-CN: 在拖尾周围绘制额外的柔和光晕。
    - glow_color: "#FFFFFF"
      $name: Glow color
      $name:zh-CN: 光晕颜色
      $description: Hexadecimal RGB color used for the glow.
      $description:zh-CN: 光晕使用的十六进制 RGB 颜色。
    - glow_width_factor: 18
      $name: Glow width
      $name:zh-CN: 光晕宽度
      $description: Glow width relative to the outer trail width. 18 means 1.8 times the outer width.
      $description:zh-CN: 光晕相对于外层拖尾宽度的比例。18 表示外层宽度的 1.8 倍。
    - glow_alpha: 25
      $name: Glow opacity
      $name:zh-CN: 光晕不透明度
      $description: Glow opacity, from 0 to 100 percent.
      $description:zh-CN: 光晕不透明度，范围为 0 到 100%。
  $name: Effects
  $name:zh-CN: 效果

- Interaction:
    - click_feedback_enabled: false
      $name: Mouse click rings
      $name:zh-CN: 鼠标点击圆环
      $description: Show an expanding ring when the left, right, or middle mouse button is pressed. Click feedback uses asynchronous raw mouse input and does not install a low-level mouse hook.
      $description:zh-CN: 按下鼠标左键、右键或中键时显示扩散圆环。点击反馈使用异步原始鼠标输入，不会安装低级鼠标钩子。
    - wheel_feedback_enabled: false
      $name: Scroll-wheel feedback
      $name:zh-CN: 滚轮反馈
      $description: Show an animated arrow in the wheel direction. Native horizontal/tilt-wheel input points left or right directly. Hold Shift to map vertical wheel input to horizontal feedback, with wheel up pointing left and wheel down pointing right. Colors remain associated with the wheel input direction.
      $description:zh-CN: 滚轮滚动时显示沿对应方向移动的箭头。原生水平/倾斜滚轮直接显示左/右箭头；按住 Shift 时，垂直滚轮改为水平反馈：向上滚动指向左侧，向下滚动指向右侧。颜色仍按滚轮输入方向区分。
    - left_click_color: "#00D7FF"
      $name: Left-click color
      $name:zh-CN: 左键颜色
      $description: Hexadecimal RGB color used for left-button presses.
      $description:zh-CN: 左键按下时使用的十六进制 RGB 颜色。
    - right_click_color: "#FF4C8B"
      $name: Right-click color
      $name:zh-CN: 右键颜色
      $description: Hexadecimal RGB color used for right-button presses.
      $description:zh-CN: 右键按下时使用的十六进制 RGB 颜色。
    - middle_click_color: "#FFD166"
      $name: Middle-click color
      $name:zh-CN: 中键颜色
      $description: Hexadecimal RGB color used for middle-button presses.
      $description:zh-CN: 中键按下时使用的十六进制 RGB 颜色。
    - wheel_up_color: "#7CFF6B"
      $name: Scroll-up color
      $name:zh-CN: 向上滚动颜色
      $description: Hexadecimal RGB color used for wheel-up input, including the left-pointing arrow shown while Shift is held.
      $description:zh-CN: 向上滚动输入对应的十六进制 RGB 颜色；按住 Shift 时该颜色用于指向左侧的箭头。
    - wheel_down_color: "#A78BFA"
      $name: Scroll-down color
      $name:zh-CN: 向下滚动颜色
      $description: Hexadecimal RGB color used for wheel-down input, including the right-pointing arrow shown while Shift is held.
      $description:zh-CN: 向下滚动输入对应的十六进制 RGB 颜色；按住 Shift 时该颜色用于指向右侧的箭头。
    - wheel_arrow_size: 18
      $name: Scroll-arrow size
      $name:zh-CN: 滚轮箭头大小
      $description: Overall size of the animated scroll arrow in pixels. Range 6-40.
      $description:zh-CN: 滚轮动画箭头的整体大小，单位为像素。范围为 6-40。
    - wheel_arrow_distance: 32
      $name: Scroll-arrow travel distance
      $name:zh-CN: 滚轮箭头移动距离
      $description: Distance travelled by each scroll arrow during its animation, in pixels. Range 8-120.
      $description:zh-CN: 滚轮箭头在动画过程中移动的距离，单位为像素。范围为 8-120。
    - ripple_radius: 36
      $name: Click-ring radius
      $name:zh-CN: 点击圆环半径
      $description: Maximum radius of the expanding click ring in pixels. Range 10-200.
      $description:zh-CN: 点击扩散圆环的最大半径，单位为像素。范围为 10-200。
    - ripple_duration_ms: 420
      $name: Feedback duration
      $name:zh-CN: 交互反馈持续时间
      $description: Duration of each click ring or scroll arrow in milliseconds. Range 100-2000.
      $description:zh-CN: 每个点击圆环或滚轮箭头的动画持续时间，单位为毫秒。范围为 100-2000。
    - ripple_width: 3
      $name: Interaction stroke width
      $name:zh-CN: 交互线条粗细
      $description: Stroke width of click-ring outlines and scroll arrows in pixels. Range 1-10.
      $description:zh-CN: 点击圆环轮廓与滚轮箭头的线宽，单位为像素。范围为 1-10。
    - ripple_opacity: 78
      $name: Feedback opacity
      $name:zh-CN: 交互反馈不透明度
      $description: Starting opacity of each click ring or scroll arrow, from 5 to 100 percent.
      $description:zh-CN: 每个点击圆环或滚轮箭头的初始不透明度，范围为 5%-100%。
  $name: Interaction
  $name:zh-CN: 交互反馈

- Motion:
    - acceleration_accent_enabled: true
      $name: Acceleration burst accents
      $name:zh-CN: 加速爆发提示
      $description: Show a directional burst with a forward arrowhead and expanding speed lines when the pointer accelerates sharply above the trail trigger speed. Enabled by default and limited by a cooldown to avoid visual clutter.
      $description:zh-CN: 指针超过拖尾触发速度并明显加速时，短暂显示由前向箭头和向后扩展的速度线组成的爆发标记。默认开启，并通过冷却间隔避免提示过密。
    - turn_accent_enabled: false
      $name: Sharp-turn accents
      $name:zh-CN: 急转弯提示
      $description: Optionally show a high-contrast path marker that connects the incoming and outgoing directions when the pointer makes a sharp turn at high speed. Disabled by default to keep routine movement uncluttered.
      $description:zh-CN: 可选地在指针高速急转弯时，用高对比折线路径连接转向前后的方向。默认关闭，避免日常移动产生过多视觉干扰。
    - fast_move_interpolation: true
      $name: Fast-move interpolation
      $name:zh-CN: 快速移动轨迹补点
      $description: Insert a bounded number of intermediate trail samples when the pointer moves a long distance between samples.
      $description:zh-CN: 相邻采样点距离较大时，在轨迹中插入数量受限的中间采样点。
    - acceleration_threshold: 30000
      $name: Acceleration threshold
      $name:zh-CN: 加速度阈值
      $description: Minimum acceleration required for a burst accent, in pixels per second squared. Speed is compared across approximately 40 ms windows to reduce raw-input report jitter. Lower values are more sensitive. Range 3000-100000.
      $description:zh-CN: 触发加速提示所需的最小加速度，单位为像素每秒平方。速度基于约 40 毫秒的测量窗口进行比较，以降低原始鼠标报告抖动的影响。数值越低越灵敏。范围 3000-100000。
    - turn_angle_degrees: 65
      $name: Turn angle threshold
      $name:zh-CN: 转向角度阈值
      $description: Minimum direction change required for a turn accent, in degrees. Lower values detect gentler turns. Range 45-150.
      $description:zh-CN: 触发急转弯提示所需的最小方向变化角度，单位为度。数值越低越容易识别转向。范围 45-150。
    - motion_accent_duration_ms: 300
      $name: Motion accent duration
      $name:zh-CN: 运动提示持续时间
      $description: How long acceleration and turn accents remain visible. Range 80-1000 ms.
      $description:zh-CN: 加速和转向提示的显示时长。范围 80-1000 毫秒。
    - motion_accent_size: 22
      $name: Motion accent size
      $name:zh-CN: 运动提示大小
      $description: Approximate size of the acceleration streaks and turn chevrons, in pixels. Range 4-40.
      $description:zh-CN: 加速速度线和转向标记的大致尺寸，单位为像素。范围 4-40。
    - motion_accent_color: "#FFD166"
      $name: Motion accent color
      $name:zh-CN: 运动提示颜色
      $description: Hexadecimal RGB color used for acceleration and turn accents.
      $description:zh-CN: 加速和转向提示使用的十六进制 RGB 颜色。
    - motion_accent_opacity: 90
      $name: Motion accent opacity
      $name:zh-CN: 运动提示不透明度
      $description: Maximum opacity of acceleration and turn accents, from 10 to 100 percent.
      $description:zh-CN: 加速和转向提示的最大不透明度，范围为 10%-100%。
    - max_interpolation_steps: 4
      $name: Maximum interpolation steps
      $name:zh-CN: 最大补点数量
      $description: Maximum intermediate samples inserted for one long movement. Range 1-6.
      $description:zh-CN: 单次长距离移动最多插入的中间采样数量。范围 1-6。
  $name: Motion
  $name:zh-CN: 运动反馈

- Ornament:
    - enabled: false
      $name: Enable pendant ornaments
      $name:zh-CN: 启用鼠标挂坠
      $description: Hang a preset pendant from the pointer on a simulated rope. Choose a preset below; no additional tuning is required.
      $description:zh-CN: 用模拟绳子在指针上挂载预设挂坠。选择下方预设即可，无需额外调参。
    - preset: classic
      $name: Pendant preset
      $name:zh-CN: 挂坠预设
      $description: Classic hangs one pendant on a silk cord, Bouncy swings on a beaded chain, Floaty drifts on a ribbon, and Comet spreads three shrinking nodes along a neon line.
      $description:zh-CN: “经典”为丝线挂坠，“弹跳”为摆动更强的珠链，“漂浮”为缓慢漂移的缎带，“彗星”为三个沿霓虹线分布的逐渐缩小节点。
      $options:
        - classic: Classic
        - bouncy: Bouncy
        - floaty: Floaty
        - comet: Comet
      $options:zh-CN:
        - classic: 经典
        - bouncy: 弹跳
        - floaty: 漂浮
        - comet: 彗星
    - rope_length_scale: 100
      $name: Rope length
      $name:zh-CN: 绳子长度
      $description: Pendant rope length as a percentage of the selected preset. 100 keeps the preset length; higher values hang the pendant farther from the pointer. Range 25-400.
      $description:zh-CN: 挂坠绳子长度，按所选预设长度的百分比计算。100 为预设长度，数值越大挂坠离指针越远。范围 25-400。
    - image: ""
      $name: Pendant image
      $name:zh-CN: 挂坠图片
      $description: Optional image path. Leave empty to use a circular pendant in the current trail color. PNG, JPEG, BMP, GIF, and TIFF are supported; only the first frame is decoded.
      $description:zh-CN: 可选的图片路径。留空时使用当前拖尾颜色的圆形挂坠。支持 PNG、JPEG、BMP、GIF 和 TIFF，并且只解码第一帧。
  $name: Ornament
  $name:zh-CN: 鼠标挂坠

- Application:
    - app_rules: [""]
      $name: Per-app rules
      $name:zh-CN: 按应用规则
      $description: One rule per entry, `program.exe=on` or `program.exe=off`. Entries beginning with `#` are comments. Executable names are matched case-insensitively.
      $description:zh-CN: 每个条目填写一条规则，格式为 `program.exe=on` 或 `program.exe=off`。以 `#` 开头的条目为注释。可执行文件名不区分大小写。
  $name: Application
  $name:zh-CN: 应用程序
*/
// ==/WindhawkModSettings==
