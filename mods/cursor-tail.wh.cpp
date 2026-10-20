// ==WindhawkMod==
// @id              cursor-tail
// @name            Cursor Tail
// @name:zh-CN      光标拖尾
// @description     Adds a speed-reactive cursor trail with optional interaction feedback.
// @description:zh-CN 为鼠标指针添加速度响应式拖尾与可选的交互反馈。
// @version         3.21
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

Images are read only when the settings change. The rope is simulated at 125 Hz with at most eight substeps per sample. Rope points, smoothing buffers, and draw geometry are fixed-size, so the draw loop allocates nothing.

## Performance

Mouse movement and foreground changes wake the worker from an indefinite wait during ordinary idle, avoiding periodic cursor polling. Sampling runs at 125 Hz while an effect is active. If raw input or foreground notifications are unavailable, or a higher-integrity foreground window blocks raw input, fallback polling samples every 10 ms for one second after detected movement, then backs off to 100 ms while idle. Layered-window submission is paced separately at 60 FPS while actively moving and 30 FPS while fading. Raw input reports that fall inside the short wake-throttling window are coalesced and handled by a one-shot timer, and the idle back buffer is released after five seconds. Background luminance sampling enforces a 50 ms minimum interval and also requires cursor travel before taking another screen sample. The back buffer and Direct2D resources are reused between frames and resized only when the required trail bounds grow beyond the current buffer.

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

#include <windows.h>
#include <windhawk_utils.h>
#include <d2d1.h>
#include <wincodec.h>
#include <math.h>
#include <cmath>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <cwctype>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002u
#endif

constexpr UINT kSettingsChangedMessage = WM_APP + 1;
constexpr int kTargetFrameRate = 125;
constexpr int kActiveRenderFrameRate = 60;
constexpr int kFadeRenderFrameRate = 30;
constexpr DWORD kInputWakeThrottleMs = 8;
constexpr DWORD kFullscreenStrongCheckIntervalMs = 100;
constexpr DWORD kFullscreenExitConfirmMs = 500;
constexpr LONG kFullscreenTolerancePx = 2;
constexpr int kMaxTailLength = 64;
constexpr int kHistoryCapacity = kMaxTailLength;
constexpr int kRenderPadding = 2;
constexpr float kFadeCutoff = 0.05f;
constexpr int kAutoResampleIntervalMaxMs = 10000;
constexpr int kBackgroundSampleRadius = 24;
constexpr int kBackgroundSampleGrid = 5;
constexpr DWORD kBackgroundResampleIntervalMs = 50;
constexpr LONG kBackgroundResampleDistancePx = 20;
constexpr float kMinColorContrast = 3.0f;
constexpr float kAutoColorHysteresisContrast = 2.5f;
constexpr DWORD kFallbackActivePollIntervalMs = 10;
constexpr DWORD kFallbackIdlePollIntervalMs = 100;
constexpr DWORD kFallbackIdleDelayMs = 1000;
constexpr uint8_t kCursorAlphaThreshold = 96;
constexpr uint32_t kFallbackCoreColor = 0x00FFFFFF;
constexpr uint32_t kFallbackOuterColor = 0x00000000;
constexpr DWORD kBackbufferIdleReleaseDelayMs = 5000;
constexpr int kBackbufferGrowthGranularity = 64;
constexpr int kMaxActiveRipples = 12;
constexpr float kRippleInitialRadius = 4.0f;
constexpr int kMaxRippleClusterRadiusPx = 512;
constexpr int kMaxActiveMotionAccents = 8;
constexpr float kFastMoveInterpolationSpacingPx = 28.0f;
constexpr DWORD kMotionResumeGapMs = 120;
constexpr DWORD kAccelerationMeasurementWindowMs = 40;
constexpr DWORD kAccelerationAccentCooldownMs = 180;
constexpr DWORD kTurnAccentCooldownMs = 220;
constexpr int kOrnamentMaxPresetNodes = 3;
constexpr int kOrnamentMaxRopeSegments = 12;
constexpr int kOrnamentMaxRopePoints = kOrnamentMaxRopeSegments + 1;
// Two Chaikin passes keep at most 4N-6 points, so this fits the longest rope.
constexpr int kOrnamentMaxRopeDrawPoints = kOrnamentMaxRopePoints * 4;
constexpr int kOrnamentMaxImageDimension = 4096;
constexpr uint64_t kOrnamentMaxDecodedBytes = 128ull * 1024ull * 1024ull;
constexpr float kOrnamentDefaultRopeLengthPx = 48.0f;
constexpr float kOrnamentDefaultSizePx = 32.0f;
constexpr float kOrnamentConstraintEpsilonPx = 0.0001f;
constexpr float kOrnamentSpeedReferencePxPerSecond = 2400.0f;
constexpr float kOrnamentSpeedSmoothing = 0.35f;
constexpr float kOrnamentRestVelocityPx = 0.25f;
constexpr float kOrnamentSimulationHz = 125.0f;
constexpr int kOrnamentMaxSubSteps = 8;
constexpr float kOrnamentMaxCatchUpSeconds = 0.25f;
constexpr float kOrnamentMinDrawOpacity = 0.004f;
constexpr float kOrnamentPi = 3.14159265f;
constexpr float kOrnamentDefaultClickImpulse = 240.0f;
constexpr float kOrnamentDefaultWheelImpulse = 200.0f;

// The preset table below is ordered to match this enum, so a preset key
// resolves to its definition by index.
enum class OrnamentPreset : uint8_t {
    Classic,
    Bouncy,
    Floaty,
    Comet,
};

enum class OrnamentRopeStyle : uint8_t {
    Silk,
    Neon,
    Chain,
    Ribbon,
};

// Every preset owns the physics, reaction strength, rope look, and node layout
// so the user only chooses a look. ropeT places a node along the rope, where
// 0 is the pointer anchor and 1 is the rope end.
struct OrnamentPresetNode {
    float ropeT;
    float sizePx;
    float opacity;
    float angleDegrees;
};

struct OrnamentPresetDefinition {
    const wchar_t* key;
    float anchorOffsetX;
    float anchorOffsetY;
    float gravity;
    float damping;
    int constraintIterations;
    float stretch;
    float opacity;
    float clickImpulse;
    float wheelImpulse;
    int ropeSegments;
    float ropeLengthPx;
    OrnamentRopeStyle ropeStyle;
    float ropeWidthPx;
    float ropeOpacity;
    int nodeCount;
    OrnamentPresetNode nodes[kOrnamentMaxPresetNodes];
};

constexpr OrnamentPresetDefinition kOrnamentPresets[] = {
    {
        L"classic",
        10.0f,
        12.0f,
        1400.0f,
        8.0f,
        12,
        0.18f,
        1.0f,
        240.0f,
        200.0f,
        8,
        48.0f,
        OrnamentRopeStyle::Silk,
        1.5f,
        0.8f,
        1,
        {{1.0f, 32.0f, 1.0f, 0.0f}},
    },
    {
        L"bouncy",
        12.0f,
        10.0f,
        1800.0f,
        3.0f,
        14,
        0.30f,
        1.0f,
        420.0f,
        320.0f,
        10,
        58.0f,
        OrnamentRopeStyle::Chain,
        1.6f,
        0.9f,
        1,
        {{1.0f, 34.0f, 1.0f, 0.0f}},
    },
    {
        L"floaty",
        8.0f,
        8.0f,
        220.0f,
        16.0f,
        10,
        0.06f,
        0.9f,
        120.0f,
        100.0f,
        12,
        68.0f,
        OrnamentRopeStyle::Ribbon,
        1.2f,
        0.65f,
        1,
        {{1.0f, 30.0f, 0.95f, 0.0f}},
    },
    {
        L"comet",
        10.0f,
        12.0f,
        1200.0f,
        6.0f,
        12,
        0.22f,
        1.0f,
        300.0f,
        240.0f,
        10,
        64.0f,
        OrnamentRopeStyle::Neon,
        1.7f,
        0.9f,
        3,
        {
            {0.35f, 30.0f, 1.0f, 0.0f},
            {0.67f, 22.0f, 0.7f, 8.0f},
            {1.0f, 15.0f, 0.4f, 16.0f},
        },
    },
};

std::atomic<HWND> g_overlayHwnd{nullptr};
HANDLE g_threadHandle = nullptr;
HANDLE g_stopEvent = nullptr;
HANDLE g_inputWakeEvent = nullptr;
std::atomic_bool g_settingsDirty{false};
bool g_interactionPending = false;
bool g_rawInputRegistered = false;
bool g_rawInputWakePending = false;
bool g_rawMouseMovementPending = false;
bool g_foregroundChangePending = false;
bool g_elevatedForeground = false;
bool g_pollingFallback = true;
bool g_currentIntegrityLevelAttempted = false;
bool g_currentIntegrityLevelKnown = false;
DWORD g_currentIntegrityLevelRid = SECURITY_MANDATORY_MEDIUM_RID;
POINT g_fallbackCursorPosition = {};
DWORD g_fallbackLastMovementTick = 0;
bool g_fallbackCursorPositionValid = false;
LONGLONG g_nextSampleDeadline = 0;
LONGLONG g_lastRawInputWakeQpc = 0;
HWINEVENTHOOK g_foregroundEventHook = nullptr;

POINT g_history[kHistoryCapacity] = {};
int g_historyHead = 0;
int g_historyCount = 0;
POINT g_lastPos = {0, 0};
LONGLONG g_lastSampleQpc = 0;
LONGLONG g_qpcFrequency = 0;
bool g_isSmearing = false;
int g_lowVelocityFrames = 0;
float g_fadeAlpha = 0.0f;
POINT g_lastMotionCuePoint = {};
LONGLONG g_lastMotionCueQpc = 0;
float g_lastMotionCueSpeed = 0.0f;
POINT g_accelerationWindowPoint = {};
LONGLONG g_accelerationWindowQpc = 0;
float g_accelerationWindowSpeed = 0.0f;
bool g_hasAccelerationWindowSample = false;
bool g_hasAccelerationWindowSpeed = false;
float g_lastMotionDirectionX = 0.0f;
float g_lastMotionDirectionY = 0.0f;
bool g_hasPreviousMotionCueSample = false;
LONGLONG g_lastAccelerationAccentQpc = 0;
LONGLONG g_lastTurnAccentQpc = 0;
float g_smoothedSpeedNorm = 0.0f;
float g_frozenSpeedNorm = 0.5f;
bool g_windowVisible = false;

ID2D1Factory* g_d2dFactory = nullptr;
ID2D1DCRenderTarget* g_renderTarget = nullptr;
ID2D1Layer* g_trailOpacityLayer = nullptr;
ID2D1Layer* g_glowOpacityLayer = nullptr;
ID2D1SolidColorBrush* g_outerBrush = nullptr;
ID2D1SolidColorBrush* g_coreBrush = nullptr;
ID2D1SolidColorBrush* g_glowBrush = nullptr;
ID2D1SolidColorBrush* g_rippleBrush = nullptr;
ID2D1LinearGradientBrush* g_gradientBrush = nullptr;
ID2D1GradientStopCollection* g_gradientStops = nullptr;
ID2D1StrokeStyle* g_strokeStyle = nullptr;
uint32_t g_gradientHeadCache = 0xFFFFFFFFu;
uint32_t g_gradientTailCache = 0xFFFFFFFFu;
bool g_dcBound = false;
HDC g_backBufferDc = nullptr;
HBITMAP g_backBufferBitmap = nullptr;
HGDIOBJ g_originalBitmap = nullptr;
int g_cachedWidth = 0;
int g_cachedHeight = 0;
DWORD g_backbufferIdleSince = 0;
HDC g_screenDc = nullptr;

float g_triggerVelocity = 3000.0f;
float g_stopVelocity = 1200.0f;
int g_tailOffsetX = 6;
int g_tailOffsetY = 10;
int g_tailLength = 10;
int g_speedScaling = 1;
float g_widthMin = 4.0f;
float g_widthMax = 14.0f;
float g_coreWidthMin = 2.0f;
float g_coreWidthMax = 9.0f;
float g_alphaMin = 0.39f;
float g_alphaMax = 0.77f;
float g_taperPower = 1.0f;
int g_smoothIterations = 2;
int g_gradientEnabled = 0;
uint32_t g_gradientTailRGB = 0x00FF00FF;
int g_glowEnabled = 0;
uint32_t g_glowRGB = 0x00FFFFFF;
float g_glowWidthFactor = 1.8f;
float g_glowAlpha = 0.25f;
int g_fadeEnabled = 1;
float g_fadeDecay = 0.90f;
int g_trailColorMode = 0;
uint32_t g_manualColorRGB = 0x00FFFFFF;
int g_outlineColorMode = 0;
uint32_t g_manualOutlineRGB = 0x00000000;
int g_autoResampleInterval = 0;
uint32_t g_currentCoreRGB = kFallbackCoreColor;
uint32_t g_currentOuterRGB = kFallbackOuterColor;
DWORD g_lastAutoColorUpdate = 0;
HCURSOR g_lastAutoColorCursor = nullptr;
bool g_autoColorInitialized = false;
float g_lastAutoColorPickLuminance = 0.5f;
bool g_autoColorPickLuminanceValid = false;

HMONITOR g_cursorMonitor = nullptr;
UINT g_cursorDpi = 96;
float g_currentDpiScale = 1.0f;

int g_clickFeedbackEnabled = 0;
int g_wheelFeedbackEnabled = 0;
uint32_t g_leftClickRGB = 0x0000D7FF;
uint32_t g_rightClickRGB = 0x00FF4C8B;
uint32_t g_middleClickRGB = 0x00FFD166;
uint32_t g_wheelUpRGB = 0x007CFF6B;
uint32_t g_wheelDownRGB = 0x00A78BFA;
float g_rippleRadius = 36.0f;
float g_wheelArrowSize = 18.0f;
float g_wheelArrowDistance = 32.0f;
int g_rippleDurationMs = 420;
float g_rippleStrokeWidth = 3.0f;
float g_rippleOpacity = 0.78f;
int g_accelerationAccentEnabled = 1;
int g_turnAccentEnabled = 0;
int g_fastMoveInterpolationEnabled = 1;
int g_accelerationThreshold = 30000;
int g_turnAngleDegrees = 65;
int g_motionAccentDurationMs = 300;
float g_motionAccentSize = 22.0f;
float g_motionAccentOpacity = 0.90f;
int g_maxInterpolationSteps = 4;
uint32_t g_motionAccentRGB = 0x00FFD166;

int g_ornamentEnabled = 0;
float g_ornamentAnchorOffsetX = 10.0f;
float g_ornamentAnchorOffsetY = 12.0f;
float g_ornamentGravity = 1400.0f;
float g_ornamentDamping = 8.0f;
int g_ornamentConstraintIterations = 4;
float g_ornamentStretch = 0.18f;
float g_ornamentOpacity = 1.0f;
float g_ornamentClickImpulse = kOrnamentDefaultClickImpulse;
float g_ornamentWheelImpulse = kOrnamentDefaultWheelImpulse;
int g_ornamentRopeSegments = 8;
float g_ornamentRopeLengthPx = kOrnamentDefaultRopeLengthPx;
float g_ornamentRopeLengthScale = 1.0f;
OrnamentRopeStyle g_ornamentRopeStyle = OrnamentRopeStyle::Silk;
float g_ornamentRopeWidthPx = 1.5f;
float g_ornamentRopeOpacity = 0.8f;

struct OrnamentImageSpec {
    std::wstring id;
    std::wstring path;
};

struct OrnamentNodeSpec {
    std::wstring imageId;
    float ropeT = 1.0f;
    float sizePx = kOrnamentDefaultSizePx;
    float angleDegrees = 0.0f;
    float pivotX = 0.5f;
    float pivotY = 0.18f;
    float opacity = 1.0f;
};

struct OrnamentImage {
    std::wstring id;
    std::wstring path;
    UINT width = 0;
    UINT height = 0;
    std::vector<uint8_t> pixels;
    ID2D1Bitmap* bitmap = nullptr;
};

struct OrnamentNode {
    int imageIndex = -1;
    float ropeT = 1.0f;
    float sizePx = kOrnamentDefaultSizePx;
    float angleDegrees = 0.0f;
    float pivotX = 0.5f;
    float pivotY = 0.18f;
    float opacity = 1.0f;
    D2D1_POINT_2F position = {};
    D2D1_RECT_F localBounds = {};
    float drawAngleDegrees = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};

std::vector<OrnamentImageSpec> g_ornamentImageSpecs;
std::vector<OrnamentNodeSpec> g_ornamentNodeSpecs;
std::vector<OrnamentImage> g_ornamentImages;
std::vector<OrnamentNode> g_ornamentNodes;
D2D1_POINT_2F g_ornamentRopePoints[kOrnamentMaxRopePoints] = {};
D2D1_POINT_2F g_ornamentRopePrevious[kOrnamentMaxRopePoints] = {};
D2D1_POINT_2F g_ornamentRopeSmoothA[kOrnamentMaxRopeDrawPoints] = {};
D2D1_POINT_2F g_ornamentRopeSmoothB[kOrnamentMaxRopeDrawPoints] = {};
int g_ornamentRopePointCount = 0;
float g_ornamentRopeSegmentPx = 0.0f;
std::atomic_bool g_ornamentAssetsDirty{false};
bool g_ornamentAssetsReady = false;
bool g_ornamentPhysicsInitialized = false;
bool g_ornamentRedrawPending = false;
LONGLONG g_ornamentLastStepQpc = 0;
LONGLONG g_ornamentNextRenderQpc = 0;
D2D1_POINT_2F g_ornamentLastAnchor = {};
float g_ornamentSmoothedSpeedNorm = 0.0f;
float g_ornamentSmoothedSpeedPx = 0.0f;
IWICImagingFactory* g_wicFactory = nullptr;
ID2D1SolidColorBrush* g_ornamentBrush = nullptr;

enum class RippleKind : uint8_t {
    Click,
    WheelUp,
    WheelDown,
    WheelLeft,
    WheelRight,
};

struct RippleEffect {
    POINT center = {};
    LONGLONG startQpc = 0;
    uint32_t color = 0x0000D7FF;
    RippleKind kind = RippleKind::Click;
    bool active = false;
};

enum class MotionAccentKind : uint8_t {
    Acceleration,
    Turn,
};

struct MotionAccent {
    POINT center = {};
    float directionX = 1.0f;
    float directionY = 0.0f;
    float previousDirectionX = 0.0f;
    float previousDirectionY = 0.0f;
    float intensity = 1.0f;
    LONGLONG startQpc = 0;
    MotionAccentKind kind = MotionAccentKind::Acceleration;
    bool active = false;
};

RippleEffect g_ripples[kMaxActiveRipples] = {};
int g_nextRippleSlot = 0;
MotionAccent g_motionAccents[kMaxActiveMotionAccents] = {};
int g_nextMotionAccentSlot = 0;

struct AppRule { std::wstring exe; bool enabled; };
std::vector<AppRule> g_appRules;
HWND g_cachedForegroundWindow = nullptr;
int g_cachedAppRule = 0;

bool g_fullscreenSuppressed = false;
DWORD g_fullscreenFalseSince = 0;
HWND g_fullscreenForegroundWindow = nullptr;
DWORD g_lastFullscreenStrongCheck = 0;
bool g_fullscreenStrongSignal = false;
bool g_fullscreenCoversMonitor = false;

struct TrailRenderCache {
    std::vector<D2D1_POINT_2F> smoothed;
    std::vector<D2D1_POINT_2F> subdivision;
    std::vector<float> taper;
    size_t cachedTaperPointCount = 0;
    float cachedTaperPower = 0.0f;
    bool taperCacheValid = false;

    void ReserveForTailLength(int tailLength) {
        const size_t baseCount = static_cast<size_t>(std::max(tailLength, 2));
        const size_t maxPointCount = baseCount * 16 - 15;
        smoothed.reserve(maxPointCount);
        subdivision.reserve(maxPointCount);
        taper.reserve(maxPointCount);
    }

    void PrepareTaper() {
        const size_t pointCount = smoothed.size();
        if (taperCacheValid &&
            cachedTaperPointCount == pointCount &&
            cachedTaperPower == g_taperPower) {
            return;
        }

        taper.resize(pointCount);
        if (pointCount <= 1) {
            if (!taper.empty()) taper[0] = 0.0f;
        } else {
            const float denominator = static_cast<float>(pointCount - 1);
            for (size_t i = 0; i + 1 < pointCount; ++i) {
                const float ratio = static_cast<float>(i) / denominator;
                taper[i] = powf(std::max(0.0f, 1.0f - ratio), g_taperPower);
            }
            taper.back() = 0.0f;
        }

        cachedTaperPointCount = pointCount;
        cachedTaperPower = g_taperPower;
        taperCacheValid = true;
    }
};
TrailRenderCache g_renderCache;

static inline float RgbLuminance(uint8_t r, uint8_t g, uint8_t b) {
    const auto linear = [](float channel) {
        channel /= 255.0f;
        return channel <= 0.03928f ? channel / 12.92f : powf((channel + 0.055f) / 1.055f, 2.4f);
    };
    return 0.2126f * linear(r) + 0.7152f * linear(g) + 0.0722f * linear(b);
}

static inline float RgbLuminance(uint32_t rgb) {
    return RgbLuminance(
        static_cast<uint8_t>((rgb >> 16) & 0xFF),
        static_cast<uint8_t>((rgb >> 8) & 0xFF),
        static_cast<uint8_t>(rgb & 0xFF));
}

static inline float ContrastRatio(float a, float b) {
    const float lo = std::min(a, b);
    const float hi = std::max(a, b);
    return (hi + 0.05f) / (lo + 0.05f);
}

static inline D2D1_COLOR_F ToColorF(uint32_t rgb, float alpha) {
    return D2D1::ColorF(
        ((rgb >> 16) & 0xFF) / 255.0f,
        ((rgb >> 8) & 0xFF) / 255.0f,
        (rgb & 0xFF) / 255.0f,
        alpha);
}

static bool ParseHexColor(const wchar_t* str, uint32_t& out) {
    if (!str) return false;
    while (*str == L' ' || *str == L'\t') ++str;
    if (*str == L'#') ++str;
    else if (str[0] == L'0' && (str[1] == L'x' || str[1] == L'X')) str += 2;
    uint32_t value = 0;
    int digits = 0;
    while (digits < 6) {
        const wchar_t c = str[digits];
        int digit = -1;
        if (c >= L'0' && c <= L'9') digit = c - L'0';
        else if (c >= L'a' && c <= L'f') digit = c - L'a' + 10;
        else if (c >= L'A' && c <= L'F') digit = c - L'A' + 10;
        if (digit < 0) break;
        value = (value << 4) | static_cast<uint32_t>(digit);
        ++digits;
    }
    if (digits != 6) return false;
    out = value;
    return true;
}

static std::wstring ToLowerW(std::wstring value) {
    for (wchar_t& c : value) c = static_cast<wchar_t>(towlower(c));
    return value;
}

static LONGLONG QpcNow() {
    LARGE_INTEGER value = {};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

static uint32_t ResolveOutlineColor(uint32_t core);
static void ReleaseOrnamentBitmaps();

void ReleaseRenderResources() {
    ReleaseOrnamentBitmaps();
    if (g_ornamentBrush) { g_ornamentBrush->Release(); g_ornamentBrush = nullptr; }
    if (g_glowOpacityLayer) { g_glowOpacityLayer->Release(); g_glowOpacityLayer = nullptr; }
    if (g_trailOpacityLayer) { g_trailOpacityLayer->Release(); g_trailOpacityLayer = nullptr; }
    if (g_gradientBrush) { g_gradientBrush->Release(); g_gradientBrush = nullptr; }
    if (g_gradientStops) { g_gradientStops->Release(); g_gradientStops = nullptr; }
    if (g_strokeStyle) { g_strokeStyle->Release(); g_strokeStyle = nullptr; }
    if (g_rippleBrush) { g_rippleBrush->Release(); g_rippleBrush = nullptr; }
    if (g_glowBrush) { g_glowBrush->Release(); g_glowBrush = nullptr; }
    if (g_coreBrush) { g_coreBrush->Release(); g_coreBrush = nullptr; }
    if (g_outerBrush) { g_outerBrush->Release(); g_outerBrush = nullptr; }
    if (g_renderTarget) { g_renderTarget->Release(); g_renderTarget = nullptr; }
    g_gradientHeadCache = 0xFFFFFFFFu;
    g_gradientTailCache = 0xFFFFFFFFu;
    g_dcBound = false;
}

void ReleaseBackbuffer() {
    ReleaseRenderResources();
    if (g_backBufferDc) {
        if (g_originalBitmap) { SelectObject(g_backBufferDc, g_originalBitmap); g_originalBitmap = nullptr; }
        DeleteDC(g_backBufferDc);
        g_backBufferDc = nullptr;
    }
    if (g_backBufferBitmap) { DeleteObject(g_backBufferBitmap); g_backBufferBitmap = nullptr; }
    g_cachedWidth = 0;
    g_cachedHeight = 0;
    g_backbufferIdleSince = 0;
}

HBITMAP Create32BitDIB(int width, int height) {
    if (width <= 0 || height <= 0) return nullptr;
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

HDC GetScreenDC() {
    if (!g_screenDc) g_screenDc = GetDC(nullptr);
    return g_screenDc;
}

void ReleaseScreenDC() {
    if (!g_screenDc) return;
    ReleaseDC(nullptr, g_screenDc);
    g_screenDc = nullptr;
}

void HideOverlay() {
    const HWND hwnd = g_overlayHwnd.load();
    if (!hwnd || !IsWindow(hwnd) || !g_windowVisible) return;
    ShowWindow(hwnd, SW_HIDE);
    g_windowVisible = false;
}

void ShowOverlay() {
    const HWND hwnd = g_overlayHwnd.load();
    if (!hwnd || !IsWindow(hwnd) || g_windowVisible) return;
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    g_windowVisible = true;
}

void HistoryClear() { g_historyHead = 0; g_historyCount = 0; }

static void ResetTrailState() {
    HistoryClear();
    g_isSmearing = false;
    g_lowVelocityFrames = 0;
    g_fadeAlpha = 0.0f;
}

// The overlay only needs to be hidden from screen capture while Auto color mode
// is sampling the screen background, otherwise the sample would pick up the
// trail's own pixels. In Manual mode the overlay stays capturable so the trail
// shows up in screenshots and screen shares.
static void ApplyOverlayCaptureAffinity() {
    const HWND hwnd = g_overlayHwnd.load(std::memory_order_acquire);
    if (!hwnd || !IsWindow(hwnd)) return;
    if (!SetWindowDisplayAffinity(hwnd, g_trailColorMode == 1 ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE)) {
        Wh_Log(L"SetWindowDisplayAffinity failed: %lu", GetLastError());
    }
}

void HistoryPushFront(const POINT& point) {
    if (g_historyCount == kHistoryCapacity) --g_historyCount;
    g_historyHead = (g_historyHead - 1 + kHistoryCapacity) % kHistoryCapacity;
    g_history[g_historyHead] = point;
    ++g_historyCount;
}

void HistoryPopBack() { if (g_historyCount > 0) --g_historyCount; }

const POINT& HistoryAt(int index) { return g_history[(g_historyHead + index) % kHistoryCapacity]; }

static bool GetProcessExeName(DWORD pid, std::wstring& out) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    WCHAR path[512] = {};
    DWORD size = ARRAYSIZE(path);
    const BOOL success = QueryFullProcessImageNameW(process, 0, path, &size);
    CloseHandle(process);
    if (!success) return false;
    const wchar_t* name = wcsrchr(path, L'\\');
    out = ToLowerW(name ? name + 1 : path);
    return true;
}

static void ParseAppRuleEntry(const wchar_t* text) {
    if (!text) return;

    std::wstring ruleText(text);
    const size_t comment = ruleText.find(L'#');
    if (comment != std::wstring::npos) ruleText.resize(comment);
    const size_t equals = ruleText.find(L'=');
    if (equals == std::wstring::npos) return;

    std::wstring exe = ruleText.substr(0, equals);
    std::wstring value = ruleText.substr(equals + 1);
    const auto trim = [](std::wstring& item) {
        const size_t first = item.find_first_not_of(L" \t");
        if (first == std::wstring::npos) {
            item.clear();
            return;
        }
        const size_t last = item.find_last_not_of(L" \t");
        item = item.substr(first, last - first + 1);
    };
    trim(exe);
    trim(value);

    const bool enabled = _wcsicmp(value.c_str(), L"on") == 0
        || value == L"1"
        || _wcsicmp(value.c_str(), L"true") == 0
        || _wcsicmp(value.c_str(), L"yes") == 0;
    const bool disabled = _wcsicmp(value.c_str(), L"off") == 0
        || value == L"0"
        || _wcsicmp(value.c_str(), L"false") == 0
        || _wcsicmp(value.c_str(), L"no") == 0;
    if (!exe.empty() && (enabled || disabled)) {
        g_appRules.push_back({ToLowerW(exe), enabled});
    }
}

static void ParseAppRules() {
    g_appRules.clear();

    constexpr int kMaxAppRuleEntries = 64;
    int consecutiveEmptyEntries = 0;
    for (int index = 0; index < kMaxAppRuleEntries; ++index) {
        WindhawkUtils::StringSetting setting =
            WindhawkUtils::StringSetting::make(L"Application.app_rules[%d]", index);
        const wchar_t* value = setting.get();
        if (!value || !*value) {
            if (++consecutiveEmptyEntries >= 2) break;
            continue;
        }

        consecutiveEmptyEntries = 0;
        ParseAppRuleEntry(value);
    }
}

static int CheckAppRuleCached(HWND foreground) {
    if (foreground == g_cachedForegroundWindow) return g_cachedAppRule;
    g_cachedForegroundWindow = foreground;
    g_cachedAppRule = 0;
    if (!foreground || g_appRules.empty()) return 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);
    if (!pid) return 0;
    std::wstring exe;
    if (!GetProcessExeName(pid, exe)) return 0;
    for (const auto& rule : g_appRules) {
        if (wcscmp(rule.exe.c_str(), exe.c_str()) == 0) {
            g_cachedAppRule = rule.enabled ? 1 : -1;
            break;
        }
    }
    return g_cachedAppRule;
}

static void ClearRipples();
static void ClearMotionAccents();
static void ResetMotionCueTracking();
static bool OrnamentActive();

static void TrimOrnamentText(std::wstring& text) {
    const size_t first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) {
        text.clear();
        return;
    }
    const size_t last = text.find_last_not_of(L" \t\r\n");
    text = text.substr(first, last - first + 1);
}

static bool LooksLikeOrnamentPath(const std::wstring& text) {
    return text.find(L'\\') != std::wstring::npos
        || text.find(L'/') != std::wstring::npos
        || text.find(L':') != std::wstring::npos;
}

// Preset keys are matched case-insensitively. An unknown value falls back to
// the first preset so a stale or hand-edited key still renders something.
static OrnamentPreset ParseOrnamentPreset(const wchar_t* value) {
    if (value && *value) {
        for (size_t i = 0; i < _countof(kOrnamentPresets); ++i) {
            if (_wcsicmp(kOrnamentPresets[i].key, value) == 0) {
                return static_cast<OrnamentPreset>(i);
            }
        }
    }
    return OrnamentPreset::Classic;
}

// The legacy `Ornament.images[0]` entry is read only when the new single-image
// setting is empty, so an upgrading user keeps the pendant they configured.
static void LoadOrnamentImageSpec() {
    g_ornamentImageSpecs.clear();
    std::wstring path;
    WindhawkUtils::StringSetting imageSetting =
        WindhawkUtils::StringSetting::make(L"Ornament.image");
    if (imageSetting.get()) path = imageSetting.get();
    TrimOrnamentText(path);
    if (path.empty()) {
        WindhawkUtils::StringSetting legacySetting =
            WindhawkUtils::StringSetting::make(L"Ornament.images[0]");
        if (legacySetting.get()) {
            std::wstring entry = legacySetting.get();
            TrimOrnamentText(entry);
            const size_t equals = entry.find(L'=');
            if (equals != std::wstring::npos) {
                std::wstring candidateId = entry.substr(0, equals);
                std::wstring candidatePath = entry.substr(equals + 1);
                TrimOrnamentText(candidateId);
                TrimOrnamentText(candidatePath);
                if (!candidatePath.empty() && !LooksLikeOrnamentPath(candidateId)) {
                    entry = std::move(candidatePath);
                }
            }
            TrimOrnamentText(entry);
            path = std::move(entry);
        }
    }
    if (path.empty()) return;
    OrnamentImageSpec spec;
    spec.id = L"pendant";
    spec.path = std::move(path);
    g_ornamentImageSpecs.push_back(std::move(spec));
}

// The selected preset owns the anchor, physics, reaction impulses, and node
// layout. Nodes always exist, so an empty image still renders the circular
// fallback of the draw path.
static void ApplyOrnamentPreset(OrnamentPreset preset) {
    const OrnamentPresetDefinition& definition =
        kOrnamentPresets[static_cast<size_t>(preset)];
    g_ornamentAnchorOffsetX = definition.anchorOffsetX;
    g_ornamentAnchorOffsetY = definition.anchorOffsetY;
    g_ornamentGravity = definition.gravity;
    g_ornamentDamping = definition.damping;
    g_ornamentConstraintIterations = definition.constraintIterations;
    g_ornamentStretch = definition.stretch;
    g_ornamentOpacity = definition.opacity;
    g_ornamentClickImpulse = definition.clickImpulse;
    g_ornamentWheelImpulse = definition.wheelImpulse;
    g_ornamentRopeSegments = definition.ropeSegments;
    g_ornamentRopeLengthPx = definition.ropeLengthPx * g_ornamentRopeLengthScale;
    g_ornamentRopeStyle = definition.ropeStyle;
    g_ornamentRopeWidthPx = definition.ropeWidthPx;
    g_ornamentRopeOpacity = definition.ropeOpacity;

    g_ornamentNodeSpecs.clear();
    const int nodeCount = std::clamp(definition.nodeCount, 0, kOrnamentMaxPresetNodes);
    g_ornamentNodeSpecs.reserve(static_cast<size_t>(nodeCount));
    for (int i = 0; i < nodeCount; ++i) {
        const OrnamentPresetNode& presetNode = definition.nodes[i];
        OrnamentNodeSpec spec;
        if (!g_ornamentImageSpecs.empty()) {
            spec.imageId = g_ornamentImageSpecs.front().id;
        }
        spec.ropeT = presetNode.ropeT;
        spec.sizePx = presetNode.sizePx;
        spec.opacity = presetNode.opacity;
        spec.angleDegrees = presetNode.angleDegrees;
        g_ornamentNodeSpecs.push_back(std::move(spec));
    }
}

void LoadSettings() {
    g_triggerVelocity = static_cast<float>(std::clamp(Wh_GetIntSetting(L"Behavior.trigger_velocity"), 50, 20000));
    g_stopVelocity = static_cast<float>(std::clamp(Wh_GetIntSetting(L"Behavior.stop_velocity"), 50, 20000));
    if (g_stopVelocity >= g_triggerVelocity) g_stopVelocity = std::max(50.0f, g_triggerVelocity * 0.5f);
    g_tailOffsetX = std::clamp(Wh_GetIntSetting(L"Behavior.tail_offset_x"), -64, 64);
    g_tailOffsetY = std::clamp(Wh_GetIntSetting(L"Behavior.tail_offset_y"), -64, 64);
    g_tailLength = std::clamp(Wh_GetIntSetting(L"Behavior.tail_length"), 2, kMaxTailLength);
    g_speedScaling = std::clamp(Wh_GetIntSetting(L"Behavior.speed_scaling"), 0, 1);
    g_widthMin = static_cast<float>(std::clamp(Wh_GetIntSetting(L"Appearance.width_min"), 1, 40));
    g_widthMax = static_cast<float>(std::clamp(Wh_GetIntSetting(L"Appearance.width_max"), 1, 60));
    g_coreWidthMin = static_cast<float>(std::clamp(Wh_GetIntSetting(L"Appearance.core_width_min"), 1, 40));
    g_coreWidthMax = static_cast<float>(std::clamp(Wh_GetIntSetting(L"Appearance.core_width_max"), 1, 60));
    g_alphaMin = std::clamp(Wh_GetIntSetting(L"Appearance.alpha_min"), 0, 100) / 100.0f;
    g_alphaMax = std::clamp(Wh_GetIntSetting(L"Appearance.alpha_max"), 0, 100) / 100.0f;
    g_taperPower = std::clamp(Wh_GetIntSetting(L"Appearance.taper_power"), 5, 30) / 10.0f;
    if (g_widthMax < g_widthMin) std::swap(g_widthMin, g_widthMax);
    if (g_coreWidthMax < g_coreWidthMin) std::swap(g_coreWidthMin, g_coreWidthMax);
    g_smoothIterations = std::clamp(Wh_GetIntSetting(L"Appearance.smooth_iterations"), 0, 4);
    g_gradientEnabled = std::clamp(Wh_GetIntSetting(L"Effects.gradient_enabled"), 0, 1);
    {
        uint32_t parsed = 0;
        WindhawkUtils::StringSetting value =
            WindhawkUtils::StringSetting::make(L"Effects.gradient_tail_color");
        g_gradientTailRGB = ParseHexColor(value.get(), parsed) ? parsed : 0x00FF00FF;
    }
    g_glowEnabled = std::clamp(Wh_GetIntSetting(L"Effects.glow_enabled"), 0, 1);
    {
        uint32_t parsed = 0;
        WindhawkUtils::StringSetting value =
            WindhawkUtils::StringSetting::make(L"Effects.glow_color");
        g_glowRGB = ParseHexColor(value.get(), parsed) ? parsed : 0x00FFFFFF;
    }
    g_glowWidthFactor = std::clamp(Wh_GetIntSetting(L"Effects.glow_width_factor"), 10, 30) / 10.0f;
    g_glowAlpha = std::clamp(Wh_GetIntSetting(L"Effects.glow_alpha"), 0, 100) / 100.0f;
    g_fadeEnabled = std::clamp(Wh_GetIntSetting(L"Behavior.fade_enabled"), 0, 1);
    g_fadeDecay = std::clamp(Wh_GetIntSetting(L"Behavior.fade_decay"), 50, 99) / 100.0f;
    {
        WindhawkUtils::StringSetting value =
            WindhawkUtils::StringSetting::make(L"Color.trail_color_mode");
        g_trailColorMode = _wcsicmp(value.get(), L"auto") == 0 ? 1 : 0;
    }
    {
        uint32_t parsed = 0;
        WindhawkUtils::StringSetting value =
            WindhawkUtils::StringSetting::make(L"Color.trail_color_manual");
        g_manualColorRGB = ParseHexColor(value.get(), parsed) ? parsed : kFallbackCoreColor;
    }
    {
        WindhawkUtils::StringSetting value =
            WindhawkUtils::StringSetting::make(L"Color.outline_color_mode");
        g_outlineColorMode = _wcsicmp(value.get(), L"manual") == 0 ? 1 : 0;
    }
    {
        uint32_t parsed = 0;
        WindhawkUtils::StringSetting value =
            WindhawkUtils::StringSetting::make(L"Color.outline_color_manual");
        g_manualOutlineRGB = ParseHexColor(value.get(), parsed) ? parsed : kFallbackOuterColor;
    }
    g_autoResampleInterval = std::clamp(Wh_GetIntSetting(L"Color.auto_resample_interval"), 0, kAutoResampleIntervalMaxMs);
    g_clickFeedbackEnabled = std::clamp(Wh_GetIntSetting(L"Interaction.click_feedback_enabled"), 0, 1);
    g_wheelFeedbackEnabled = std::clamp(Wh_GetIntSetting(L"Interaction.wheel_feedback_enabled"), 0, 1);
    if (!g_clickFeedbackEnabled && !g_wheelFeedbackEnabled) ClearRipples();
    g_rippleRadius = static_cast<float>(std::clamp(Wh_GetIntSetting(L"Interaction.ripple_radius"), 10, 200));
    g_wheelArrowSize = static_cast<float>(std::clamp(Wh_GetIntSetting(L"Interaction.wheel_arrow_size"), 6, 40));
    g_wheelArrowDistance = static_cast<float>(std::clamp(Wh_GetIntSetting(L"Interaction.wheel_arrow_distance"), 8, 120));
    g_rippleDurationMs = std::clamp(Wh_GetIntSetting(L"Interaction.ripple_duration_ms"), 100, 2000);
    g_rippleStrokeWidth = static_cast<float>(std::clamp(Wh_GetIntSetting(L"Interaction.ripple_width"), 1, 10));
    g_rippleOpacity = std::clamp(Wh_GetIntSetting(L"Interaction.ripple_opacity"), 5, 100) / 100.0f;
    const auto readInteractionColor = [](const wchar_t* settingPath, uint32_t fallbackColor) {
        uint32_t parsedColor = 0;
        WindhawkUtils::StringSetting value = WindhawkUtils::StringSetting::make(settingPath);
        return ParseHexColor(value.get(), parsedColor) ? parsedColor : fallbackColor;
    };
    g_leftClickRGB = readInteractionColor(L"Interaction.left_click_color", 0x0000D7FF);
    g_rightClickRGB = readInteractionColor(L"Interaction.right_click_color", 0x00FF4C8B);
    g_middleClickRGB = readInteractionColor(L"Interaction.middle_click_color", 0x00FFD166);
    g_wheelUpRGB = readInteractionColor(L"Interaction.wheel_up_color", 0x007CFF6B);
    g_wheelDownRGB = readInteractionColor(L"Interaction.wheel_down_color", 0x00A78BFA);
    g_accelerationAccentEnabled = std::clamp(Wh_GetIntSetting(L"Motion.acceleration_accent_enabled"), 0, 1);
    g_turnAccentEnabled = std::clamp(Wh_GetIntSetting(L"Motion.turn_accent_enabled"), 0, 1);
    g_fastMoveInterpolationEnabled = std::clamp(Wh_GetIntSetting(L"Motion.fast_move_interpolation"), 0, 1);
    g_accelerationThreshold = std::clamp(Wh_GetIntSetting(L"Motion.acceleration_threshold"), 3000, 100000);
    g_motionAccentOpacity = std::clamp(Wh_GetIntSetting(L"Motion.motion_accent_opacity"), 10, 100) / 100.0f;
    g_turnAngleDegrees = std::clamp(Wh_GetIntSetting(L"Motion.turn_angle_degrees"), 45, 150);
    g_motionAccentDurationMs = std::clamp(Wh_GetIntSetting(L"Motion.motion_accent_duration_ms"), 80, 1000);
    g_motionAccentSize = static_cast<float>(std::clamp(Wh_GetIntSetting(L"Motion.motion_accent_size"), 4, 40));
    g_maxInterpolationSteps = std::clamp(Wh_GetIntSetting(L"Motion.max_interpolation_steps"), 1, 6);
    g_motionAccentRGB = readInteractionColor(L"Motion.motion_accent_color", 0x00FFD166);
    g_ornamentEnabled = std::clamp(Wh_GetIntSetting(L"Ornament.enabled"), 0, 1);
    OrnamentPreset ornamentPreset = OrnamentPreset::Classic;
    {
        WindhawkUtils::StringSetting value =
            WindhawkUtils::StringSetting::make(L"Ornament.preset");
        ornamentPreset = ParseOrnamentPreset(value.get());
    }
    // A settings blob written before this key existed reads as 0, so fall back
    // to the preset length instead of clamping to the shortest rope.
    const int ropeLengthScalePct =
        Wh_GetIntSetting(L"Ornament.rope_length_scale");
    g_ornamentRopeLengthScale =
        std::clamp(ropeLengthScalePct > 0 ? ropeLengthScalePct : 100, 25, 400) / 100.0f;
    LoadOrnamentImageSpec();
    ApplyOrnamentPreset(ornamentPreset);
    g_ornamentAssetsDirty.store(true, std::memory_order_release);
    ResetMotionCueTracking();
    ParseAppRules();
    g_cachedForegroundWindow = nullptr;
    g_cachedAppRule = 0;
    g_renderCache.ReserveForTailLength(g_tailLength);
    g_autoColorInitialized = false;
    g_lastAutoColorCursor = nullptr;
    g_lastAutoColorUpdate = 0;
    if (g_trailColorMode == 0) g_currentCoreRGB = g_manualColorRGB;
    g_currentOuterRGB = ResolveOutlineColor(g_currentCoreRGB);
    ApplyOverlayCaptureAffinity();
}


static void ClearRipples() {
    for (RippleEffect& ripple : g_ripples) {
        ripple.active = false;
    }
    g_nextRippleSlot = 0;
    g_interactionPending = false;
}

static void ClearMotionAccents() {
    for (MotionAccent& accent : g_motionAccents) accent.active = false;
    g_nextMotionAccentSlot = 0;
}

static void ResetMotionCueTracking() {
    ClearMotionAccents();
    g_lastMotionCuePoint = {};
    g_lastMotionCueQpc = 0;
    g_lastMotionCueSpeed = 0.0f;
    g_lastMotionDirectionX = 0.0f;
    g_lastMotionDirectionY = 0.0f;
    g_hasPreviousMotionCueSample = false;
    g_accelerationWindowPoint = {};
    g_accelerationWindowQpc = 0;
    g_accelerationWindowSpeed = 0.0f;
    g_hasAccelerationWindowSample = false;
    g_hasAccelerationWindowSpeed = false;
    g_lastAccelerationAccentQpc = 0;
    g_lastTurnAccentQpc = 0;
}

static void AddMotionAccent(
    const POINT& center,
    float directionX,
    float directionY,
    float intensity,
    MotionAccentKind kind,
    float previousDirectionX = 0.0f,
    float previousDirectionY = 0.0f) {
    int selectedSlot = -1;
    for (int offset = 0; offset < kMaxActiveMotionAccents; ++offset) {
        const int candidateSlot = (g_nextMotionAccentSlot + offset) % kMaxActiveMotionAccents;
        if (!g_motionAccents[candidateSlot].active) {
            selectedSlot = candidateSlot;
            break;
        }
    }
    if (selectedSlot < 0) selectedSlot = g_nextMotionAccentSlot;
    g_nextMotionAccentSlot = (selectedSlot + 1) % kMaxActiveMotionAccents;

    MotionAccent& accent = g_motionAccents[selectedSlot];
    accent.center = center;
    accent.directionX = directionX;
    accent.directionY = directionY;
    accent.previousDirectionX = previousDirectionX;
    accent.previousDirectionY = previousDirectionY;
    accent.intensity = std::clamp(intensity, 0.5f, 1.8f);
    accent.startQpc = QpcNow();
    accent.kind = kind;
    accent.active = true;
}

static int UpdateMotionAccentState(LONGLONG nowQpc) {
    const LONGLONG durationTicks = std::max<LONGLONG>(
        1, g_qpcFrequency * static_cast<LONGLONG>(g_motionAccentDurationMs) / 1000);
    int activeCount = 0;
    for (MotionAccent& accent : g_motionAccents) {
        if (!accent.active) continue;
        const LONGLONG elapsedTicks = nowQpc - accent.startQpc;
        if (elapsedTicks < 0 || elapsedTicks >= durationTicks) {
            accent.active = false;
            continue;
        }
        ++activeCount;
    }
    return activeCount;
}

static bool HasActiveMotionAccents(LONGLONG nowQpc) {
    return UpdateMotionAccentState(nowQpc) > 0;
}

static void UpdateMotionCueTracking(const POINT& point, LONGLONG nowQpc) {
    if (!g_accelerationAccentEnabled && !g_turnAccentEnabled) {
        g_lastMotionCuePoint = point;
        g_lastMotionCueQpc = nowQpc;
        g_lastMotionCueSpeed = 0.0f;
        g_lastMotionDirectionX = 0.0f;
        g_lastMotionDirectionY = 0.0f;
        g_hasPreviousMotionCueSample = false;
        g_accelerationWindowPoint = point;
        g_accelerationWindowQpc = nowQpc;
        g_accelerationWindowSpeed = 0.0f;
        g_hasAccelerationWindowSample = false;
        g_hasAccelerationWindowSpeed = false;
        return;
    }

    if (!g_hasPreviousMotionCueSample) {
        g_lastMotionCuePoint = point;
        g_lastMotionCueQpc = nowQpc;
        g_lastMotionCueSpeed = 0.0f;
        g_lastMotionDirectionX = 0.0f;
        g_lastMotionDirectionY = 0.0f;
        g_hasPreviousMotionCueSample = true;
        g_accelerationWindowPoint = point;
        g_accelerationWindowQpc = nowQpc;
        g_accelerationWindowSpeed = 0.0f;
        g_hasAccelerationWindowSample = true;
        g_hasAccelerationWindowSpeed = false;
        return;
    }

    const LONGLONG elapsedTicks = nowQpc - g_lastMotionCueQpc;
    const LONGLONG resumeGapTicks = std::max<LONGLONG>(
        1, g_qpcFrequency * static_cast<LONGLONG>(kMotionResumeGapMs) / 1000);
    if (elapsedTicks <= 0 || elapsedTicks > resumeGapTicks) {
        // Rebase after idle; do not interpret time spent idle as acceleration or a turn.
        g_lastMotionCuePoint = point;
        g_lastMotionCueQpc = nowQpc;
        g_lastMotionCueSpeed = 0.0f;
        g_lastMotionDirectionX = 0.0f;
        g_lastMotionDirectionY = 0.0f;
        g_hasPreviousMotionCueSample = true;
        g_accelerationWindowPoint = point;
        g_accelerationWindowQpc = nowQpc;
        g_accelerationWindowSpeed = 0.0f;
        g_hasAccelerationWindowSample = true;
        g_hasAccelerationWindowSpeed = false;
        return;
    }

    const float dx = static_cast<float>(point.x - g_lastMotionCuePoint.x);
    const float dy = static_cast<float>(point.y - g_lastMotionCuePoint.y);
    const float distance = sqrtf(dx * dx + dy * dy);
    const float elapsedSeconds = static_cast<float>(elapsedTicks)
        / static_cast<float>(std::max<LONGLONG>(1, g_qpcFrequency));
    const float speed = distance / std::max(elapsedSeconds, 0.001f);
    if (distance >= 2.0f) {
        const float directionX = dx / distance;
        const float directionY = dy / distance;
        const LONGLONG turnCooldown = std::max<LONGLONG>(
            1, g_qpcFrequency * static_cast<LONGLONG>(kTurnAccentCooldownMs) / 1000);

        const float previousDirectionLength = sqrtf(
            g_lastMotionDirectionX * g_lastMotionDirectionX
            + g_lastMotionDirectionY * g_lastMotionDirectionY);
        if (g_turnAccentEnabled
            && g_lastMotionCueSpeed >= g_triggerVelocity
            && speed >= g_triggerVelocity
            && previousDirectionLength > 0.5f
            && (g_lastTurnAccentQpc == 0
                || nowQpc - g_lastTurnAccentQpc >= turnCooldown)) {
            const float dot = std::clamp(
                g_lastMotionDirectionX * directionX + g_lastMotionDirectionY * directionY,
                -1.0f,
                1.0f);
            const float angleThreshold = cosf(
                static_cast<float>(g_turnAngleDegrees) * 3.14159265358979323846f / 180.0f);
            if (dot <= angleThreshold) {
                AddMotionAccent(
                    point,
                    directionX,
                    directionY,
                    std::clamp((1.0f - dot) * 0.9f, 0.5f, 1.8f),
                    MotionAccentKind::Turn,
                    g_lastMotionDirectionX,
                    g_lastMotionDirectionY);
                g_lastTurnAccentQpc = nowQpc;
            }
        }

        g_lastMotionDirectionX = directionX;
        g_lastMotionDirectionY = directionY;
    }

    // Compare average velocities over longer, bounded windows instead of differentiating
    // every raw-input wake, whose report batching can make adjacent speeds oscillate.
    const LONGLONG accelerationWindowTicks = std::max<LONGLONG>(
        1, g_qpcFrequency
            * static_cast<LONGLONG>(kAccelerationMeasurementWindowMs) / 1000);
    if (g_accelerationAccentEnabled
        && g_hasAccelerationWindowSample
        && nowQpc - g_accelerationWindowQpc >= accelerationWindowTicks) {
        const LONGLONG windowElapsedTicks = nowQpc - g_accelerationWindowQpc;
        const float windowElapsedSeconds = static_cast<float>(windowElapsedTicks)
            / static_cast<float>(std::max<LONGLONG>(1, g_qpcFrequency));
        const float windowDx = static_cast<float>(point.x - g_accelerationWindowPoint.x);
        const float windowDy = static_cast<float>(point.y - g_accelerationWindowPoint.y);
        const float windowDistance = sqrtf(windowDx * windowDx + windowDy * windowDy);
        const float windowSpeed = windowDistance / std::max(windowElapsedSeconds, 0.001f);

        if (g_hasAccelerationWindowSpeed && windowDistance >= 2.0f) {
            const float acceleration =
                (windowSpeed - g_accelerationWindowSpeed)
                / std::max(windowElapsedSeconds, 0.001f);
            const LONGLONG accelerationCooldown = std::max<LONGLONG>(
                1, g_qpcFrequency
                    * static_cast<LONGLONG>(kAccelerationAccentCooldownMs) / 1000);
            if (windowSpeed >= g_triggerVelocity
                && acceleration >= static_cast<float>(g_accelerationThreshold)
                && (g_lastAccelerationAccentQpc == 0
                    || nowQpc - g_lastAccelerationAccentQpc >= accelerationCooldown)) {
                const float directionX = windowDx / windowDistance;
                const float directionY = windowDy / windowDistance;
                AddMotionAccent(
                    point,
                    directionX,
                    directionY,
                    acceleration / static_cast<float>(g_accelerationThreshold),
                    MotionAccentKind::Acceleration);
                g_lastAccelerationAccentQpc = nowQpc;
            }
        }

        g_accelerationWindowPoint = point;
        g_accelerationWindowQpc = nowQpc;
        g_accelerationWindowSpeed = windowSpeed;
        g_hasAccelerationWindowSample = true;
        g_hasAccelerationWindowSpeed = true;
    }

    g_lastMotionCuePoint = point;
    g_lastMotionCueQpc = nowQpc;
    g_lastMotionCueSpeed = speed;
}

static int UpdateRippleState(LONGLONG nowQpc) {
    const LONGLONG durationTicks = std::max<LONGLONG>(
        1, g_qpcFrequency * static_cast<LONGLONG>(g_rippleDurationMs) / 1000);
    int activeCount = 0;
    for (RippleEffect& ripple : g_ripples) {
        if (!ripple.active) continue;
        const LONGLONG elapsedTicks = nowQpc - ripple.startQpc;
        if (elapsedTicks >= durationTicks || elapsedTicks < 0) {
            ripple.active = false;
            continue;
        }
        ++activeCount;
    }
    return activeCount;
}

static bool HasActiveRipples(LONGLONG nowQpc) {
    return UpdateRippleState(nowQpc) > 0;
}

static void AddRipple(const POINT& center, uint32_t color, RippleKind kind = RippleKind::Click) {
    int selectedSlot = -1;
    for (int offset = 0; offset < kMaxActiveRipples; ++offset) {
        const int candidateSlot = (g_nextRippleSlot + offset) % kMaxActiveRipples;
        if (!g_ripples[candidateSlot].active) {
            selectedSlot = candidateSlot;
            break;
        }
    }
    if (selectedSlot < 0) selectedSlot = g_nextRippleSlot;
    g_nextRippleSlot = (selectedSlot + 1) % kMaxActiveRipples;

    RippleEffect& ripple = g_ripples[selectedSlot];
    ripple.center = center;
    ripple.startQpc = QpcNow();
    ripple.color = color;
    ripple.kind = kind;
    ripple.active = true;
    g_interactionPending = true;
    if (g_inputWakeEvent) SetEvent(g_inputWakeEvent);
}

static bool RegisterMouseRawInput(HWND hwnd, bool enable) {
    RAWINPUTDEVICE mouse = {};
    mouse.usUsagePage = 0x01;  // HID_USAGE_PAGE_GENERIC
    mouse.usUsage = 0x02;      // HID_USAGE_GENERIC_MOUSE
    mouse.dwFlags = enable ? RIDEV_INPUTSINK : RIDEV_REMOVE;
    mouse.hwndTarget = enable ? hwnd : nullptr;

    if (!RegisterRawInputDevices(&mouse, 1, sizeof(mouse))) {
        Wh_Log(L"RegisterRawInputDevices(%d) failed: %lu", enable, GetLastError());
        return false;
    }
    return true;
}

static bool ReadTokenIntegrityRid(HANDLE tokenHandle, DWORD& integrityRid) {
    DWORD requiredSize = 0;
    if (GetTokenInformation(
            tokenHandle,
            TokenIntegrityLevel,
            nullptr,
            0,
            &requiredSize)
        || GetLastError() != ERROR_INSUFFICIENT_BUFFER
        || requiredSize < sizeof(TOKEN_MANDATORY_LABEL)) {
        return false;
    }

    std::vector<BYTE> tokenInfo(requiredSize);
    if (!GetTokenInformation(
            tokenHandle,
            TokenIntegrityLevel,
            tokenInfo.data(),
            requiredSize,
            &requiredSize)) {
        return false;
    }

    const auto* mandatoryLabel =
        reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(tokenInfo.data());
    PSID integritySid = mandatoryLabel->Label.Sid;
    if (!integritySid || !IsValidSid(integritySid)) return false;

    PUCHAR subAuthorityCount = GetSidSubAuthorityCount(integritySid);
    if (!subAuthorityCount || *subAuthorityCount == 0) return false;
    PDWORD ridValue = GetSidSubAuthority(
        integritySid,
        static_cast<DWORD>(*subAuthorityCount - 1));
    if (!ridValue) return false;

    integrityRid = *ridValue;
    return true;
}

static void CacheCurrentIntegrityLevel() {
    if (g_currentIntegrityLevelAttempted) return;
    g_currentIntegrityLevelAttempted = true;

    HANDLE tokenHandle = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle)) {
        Wh_Log(L"OpenProcessToken(current process) failed: %lu", GetLastError());
        return;
    }

    g_currentIntegrityLevelKnown =
        ReadTokenIntegrityRid(tokenHandle, g_currentIntegrityLevelRid);
    CloseHandle(tokenHandle);
    if (!g_currentIntegrityLevelKnown) {
        Wh_Log(L"Unable to read current process token integrity level");
    }
}

// Raw input is normally unavailable while a higher-integrity window has
// focus. Compare token integrity levels and use bounded sampling when the
// target integrity cannot be established safely.
static bool IsHigherRightsWindow(HWND hwnd) {
    CacheCurrentIntegrityLevel();
    if (!g_currentIntegrityLevelKnown) return true;
    if (!hwnd) return false;

    DWORD processId = 0;
    if (!GetWindowThreadProcessId(hwnd, &processId) || !processId) return false;

    HANDLE processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!processHandle) return true;

    HANDLE tokenHandle = nullptr;
    if (!OpenProcessToken(processHandle, TOKEN_QUERY, &tokenHandle)) {
        CloseHandle(processHandle);
        return true;
    }

    DWORD foregroundIntegrityRid = 0;
    const bool integrityKnown = ReadTokenIntegrityRid(tokenHandle, foregroundIntegrityRid);
    CloseHandle(tokenHandle);
    CloseHandle(processHandle);
    return !integrityKnown || foregroundIntegrityRid > g_currentIntegrityLevelRid;
}

static void CALLBACK ForegroundChanged(
    HWINEVENTHOOK,
    DWORD,
    HWND,
    LONG,
    LONG,
    DWORD,
    DWORD) {
    g_foregroundChangePending = true;
    if (g_inputWakeEvent) SetEvent(g_inputWakeEvent);
}

static void ResetFallbackPollTracking() {
    POINT point = {};
    g_fallbackCursorPositionValid = GetCursorPos(&point) != FALSE;
    if (g_fallbackCursorPositionValid) g_fallbackCursorPosition = point;
    g_fallbackLastMovementTick = GetTickCount() - kFallbackIdleDelayMs;
}

static LONGLONG GetFallbackPollIntervalTicks() {
    POINT point = {};
    const DWORD now = GetTickCount();
    if (GetCursorPos(&point)) {
        if (!g_fallbackCursorPositionValid) {
            g_fallbackCursorPosition = point;
            g_fallbackCursorPositionValid = true;
            g_fallbackLastMovementTick = now - kFallbackIdleDelayMs;
        } else if (point.x != g_fallbackCursorPosition.x
            || point.y != g_fallbackCursorPosition.y) {
            g_fallbackCursorPosition = point;
            g_fallbackLastMovementTick = now;
        }
    }

    const DWORD intervalMs = now - g_fallbackLastMovementTick < kFallbackIdleDelayMs
        ? kFallbackActivePollIntervalMs
        : kFallbackIdlePollIntervalMs;
    return std::max<LONGLONG>(
        1, g_qpcFrequency * static_cast<LONGLONG>(intervalMs) / 1000);
}

static void RefreshForegroundInputState() {
    const bool wasPollingFallback = g_pollingFallback;
    g_elevatedForeground = IsHigherRightsWindow(GetForegroundWindow());
    g_pollingFallback = !g_rawInputRegistered || !g_foregroundEventHook || g_elevatedForeground;
    if (g_pollingFallback && !wasPollingFallback) ResetFallbackPollTracking();
    g_foregroundChangePending = false;
}

static void ResetAutoColorSamplingCache();

static bool UpdateCursorMonitorDpi(const POINT& cursorPoint) {
    const HMONITOR monitor = MonitorFromPoint(cursorPoint, MONITOR_DEFAULTTONEAREST);
    if (!monitor || monitor == g_cursorMonitor) return false;

    UINT dpiX = 96;
    UINT dpiY = 96;
    const HRESULT dpiResult = GetDpiForMonitor(
        monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    if (FAILED(dpiResult) || dpiX == 0) {
        Wh_Log(L"GetDpiForMonitor failed: 0x%08X", static_cast<unsigned int>(dpiResult));
        dpiX = 96;
    }

    g_cursorMonitor = monitor;
    ResetAutoColorSamplingCache();
    const UINT newDpi = dpiX > 0 ? dpiX : 96;
    if (newDpi == g_cursorDpi) return false;

    g_cursorDpi = newDpi;
    g_currentDpiScale = std::clamp(static_cast<float>(g_cursorDpi) / 96.0f, 0.5f, 4.0f);
    HideOverlay();
    return true;
}

static bool IsDesktopShellWindow(HWND hwnd) {
    wchar_t className[64] = {};
    const int length = GetClassNameW(hwnd, className, ARRAYSIZE(className));
    if (length <= 0) return false;
    return _wcsicmp(className, L"Progman") == 0
        || _wcsicmp(className, L"WorkerW") == 0;
}

static bool CoversMonitor(HWND hwnd, HMONITOR cursorMonitor) {
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    if ((style & WS_CAPTION) != 0) return false;
    RECT windowRect = {};
    if (!GetWindowRect(hwnd, &windowRect)) return false;
    const HMONITOR windowMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!cursorMonitor || windowMonitor != cursorMonitor) return false;
    MONITORINFO monitorInfo = {sizeof(monitorInfo)};
    if (!GetMonitorInfoW(windowMonitor, &monitorInfo)) return false;
    return windowRect.left <= monitorInfo.rcMonitor.left + kFullscreenTolerancePx
        && windowRect.top <= monitorInfo.rcMonitor.top + kFullscreenTolerancePx
        && windowRect.right >= monitorInfo.rcMonitor.right - kFullscreenTolerancePx
        && windowRect.bottom >= monitorInfo.rcMonitor.bottom - kFullscreenTolerancePx;
}

// SHQueryUserNotificationState reports session-wide state, not per-window state.
static bool CheckStrongFullscreenSignal() {
    QUERY_USER_NOTIFICATION_STATE state = QUNS_NOT_PRESENT;
    return SUCCEEDED(SHQueryUserNotificationState(&state)) && state == QUNS_RUNNING_D3D_FULL_SCREEN;
}

static bool IsFullscreenCandidate(DWORD now, HWND hwnd, POINT cursorPoint) {
    if (!hwnd || hwnd == GetDesktopWindow() || hwnd == GetShellWindow()
        || IsDesktopShellWindow(hwnd) || IsIconic(hwnd) || !IsWindowVisible(hwnd)) {
        return false;
    }

    const HMONITOR cursorMonitor = MonitorFromPoint(cursorPoint, MONITOR_DEFAULTTONEAREST);
    const HMONITOR windowMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!cursorMonitor || windowMonitor != cursorMonitor) return false;

    if (hwnd != g_fullscreenForegroundWindow) {
        g_fullscreenForegroundWindow = hwnd;
        g_lastFullscreenStrongCheck = 0;
        g_fullscreenStrongSignal = false;
        g_fullscreenCoversMonitor = false;
        g_fullscreenFalseSince = 0;
    }

    // Window geometry and the shell's D3D state do not need to be queried on
    // every input report, even though fullscreen transitions are event-driven.
    if (now - g_lastFullscreenStrongCheck >= kFullscreenStrongCheckIntervalMs) {
        g_lastFullscreenStrongCheck = now;
        g_fullscreenStrongSignal = CheckStrongFullscreenSignal();
        g_fullscreenCoversMonitor = CoversMonitor(hwnd, cursorMonitor);
    }
    return g_fullscreenStrongSignal || g_fullscreenCoversMonitor;
}

static bool UpdateFullscreenState(DWORD now, HWND foreground, POINT cursorPoint) {
    const HMONITOR cursorMonitor = MonitorFromPoint(cursorPoint, MONITOR_DEFAULTTONEAREST);
    const HMONITOR foregroundMonitor = foreground
        ? MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST)
        : nullptr;
    const bool isDesktopShell = !foreground
        || foreground == GetDesktopWindow()
        || foreground == GetShellWindow()
        || IsDesktopShellWindow(foreground);
    const bool pointerLeftFullscreenMonitor =
        cursorMonitor && foregroundMonitor && cursorMonitor != foregroundMonitor;

    // The desktop shell is never a fullscreen app. Likewise, a fullscreen
    // window on another monitor must not suppress an overlay under the pointer.
    // These transitions are immediate; ordinary same-monitor fullscreen exit
    // still uses the 500 ms confirmation below to avoid flicker.
    if (isDesktopShell || pointerLeftFullscreenMonitor) {
        g_fullscreenSuppressed = false;
        g_fullscreenFalseSince = 0;
        return false;
    }

    const bool candidate = IsFullscreenCandidate(now, foreground, cursorPoint);
    if (candidate) {
        g_fullscreenFalseSince = 0;
        g_fullscreenSuppressed = true;
        return true;
    }

    if (g_fullscreenSuppressed) {
        if (g_fullscreenFalseSince == 0) g_fullscreenFalseSince = now;
        if (now - g_fullscreenFalseSince >= kFullscreenExitConfirmMs) {
            g_fullscreenSuppressed = false;
            g_fullscreenFalseSince = 0;
        }
        return g_fullscreenSuppressed;
    }

    g_fullscreenFalseSince = 0;
    return false;
}

static int GrowBackbufferDimension(int required, int current) {
    if (required <= current) return current;

    const int64_t rounded = (
        (static_cast<int64_t>(required) + kBackbufferGrowthGranularity - 1)
        / kBackbufferGrowthGranularity
    ) * kBackbufferGrowthGranularity;
    return rounded <= std::numeric_limits<int>::max()
        ? static_cast<int>(rounded)
        : required;
}

bool EnsureBackbuffer(int width, int height, HDC referenceDc) {
    if (width <= 0 || height <= 0 || !referenceDc) return false;
    if (g_backBufferDc && g_backBufferBitmap && width <= g_cachedWidth && height <= g_cachedHeight) return true;
    if (!g_backBufferDc) {
        g_backBufferDc = CreateCompatibleDC(referenceDc);
        if (!g_backBufferDc) return false;
    }
    const int newWidth = GrowBackbufferDimension(width, g_cachedWidth);
    const int newHeight = GrowBackbufferDimension(height, g_cachedHeight);
    ReleaseRenderResources();
    HBITMAP bitmap = Create32BitDIB(newWidth, newHeight);
    if (!bitmap) return false;
    const HGDIOBJ oldBitmap = SelectObject(g_backBufferDc, bitmap);
    if (!g_originalBitmap) g_originalBitmap = oldBitmap;
    else if (oldBitmap && oldBitmap != g_originalBitmap) DeleteObject(oldBitmap);
    g_backBufferBitmap = bitmap;
    g_cachedWidth = newWidth;
    g_cachedHeight = newHeight;
    g_backbufferIdleSince = 0;
    return true;
}

static void MaybeReleaseIdleBackbuffer(DWORD now) {
    if (g_isSmearing || g_historyCount > 0 || HasActiveRipples(QpcNow()) || OrnamentActive()) {
        g_backbufferIdleSince = 0;
        return;
    }
    if (!g_backBufferDc || !g_backBufferBitmap) {
        g_backbufferIdleSince = 0;
        return;
    }
    if (g_backbufferIdleSince == 0) {
        g_backbufferIdleSince = now;
        return;
    }
    if (now - g_backbufferIdleSince >= kBackbufferIdleReleaseDelayMs) {
        ReleaseBackbuffer();
    }
}

bool EnsureD2DResources() {
    if (!g_d2dFactory) return false;
    if (g_renderTarget) return true;
    const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    HRESULT hr = g_d2dFactory->CreateDCRenderTarget(&properties, &g_renderTarget);
    if (FAILED(hr) || !g_renderTarget) return false;
    hr = g_renderTarget->CreateSolidColorBrush(ToColorF(g_currentOuterRGB, 1.0f), &g_outerBrush);
    if (FAILED(hr)) { ReleaseRenderResources(); return false; }
    hr = g_renderTarget->CreateSolidColorBrush(ToColorF(g_currentCoreRGB, 1.0f), &g_coreBrush);
    if (FAILED(hr)) { ReleaseRenderResources(); return false; }
    hr = g_renderTarget->CreateSolidColorBrush(ToColorF(g_glowRGB, 1.0f), &g_glowBrush);
    if (FAILED(hr)) { ReleaseRenderResources(); return false; }
    hr = g_renderTarget->CreateLayer(nullptr, &g_trailOpacityLayer);
    if (FAILED(hr) || !g_trailOpacityLayer) { ReleaseRenderResources(); return false; }
    hr = g_renderTarget->CreateLayer(nullptr, &g_glowOpacityLayer);
    if (FAILED(hr) || !g_glowOpacityLayer) { ReleaseRenderResources(); return false; }
    hr = g_renderTarget->CreateSolidColorBrush(ToColorF(g_leftClickRGB, 1.0f), &g_rippleBrush);
    if (FAILED(hr)) { ReleaseRenderResources(); return false; }
    hr = g_renderTarget->CreateSolidColorBrush(ToColorF(g_currentCoreRGB, 1.0f), &g_ornamentBrush);
    if (FAILED(hr)) { ReleaseRenderResources(); return false; }
    D2D1_STROKE_STYLE_PROPERTIES strokeProperties = {};
    strokeProperties.startCap = D2D1_CAP_STYLE_ROUND;
    strokeProperties.endCap = D2D1_CAP_STYLE_ROUND;
    strokeProperties.lineJoin = D2D1_LINE_JOIN_ROUND;
    hr = g_d2dFactory->CreateStrokeStyle(strokeProperties, nullptr, 0, &g_strokeStyle);
    if (FAILED(hr)) { ReleaseRenderResources(); return false; }
    return true;
}

bool EnsureGradientBrush() {
    if (!g_gradientEnabled) {
        if (g_gradientBrush) { g_gradientBrush->Release(); g_gradientBrush = nullptr; }
        if (g_gradientStops) { g_gradientStops->Release(); g_gradientStops = nullptr; }
        g_gradientHeadCache = 0xFFFFFFFFu;
        g_gradientTailCache = 0xFFFFFFFFu;
        return false;
    }
    if (!g_renderTarget) return false;
    if (g_gradientBrush && g_gradientStops && g_gradientHeadCache == g_currentCoreRGB && g_gradientTailCache == g_gradientTailRGB) return true;
    if (g_gradientBrush) { g_gradientBrush->Release(); g_gradientBrush = nullptr; }
    if (g_gradientStops) { g_gradientStops->Release(); g_gradientStops = nullptr; }
    D2D1_GRADIENT_STOP stops[2] = {};
    stops[0].position = 0.0f;
    stops[0].color = ToColorF(g_currentCoreRGB, 1.0f);
    stops[1].position = 1.0f;
    stops[1].color = ToColorF(g_gradientTailRGB, 1.0f);
    HRESULT hr = g_renderTarget->CreateGradientStopCollection(stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &g_gradientStops);
    if (FAILED(hr)) return false;
    const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES properties = {};
    hr = g_renderTarget->CreateLinearGradientBrush(properties, g_gradientStops, &g_gradientBrush);
    if (FAILED(hr)) { g_gradientStops->Release(); g_gradientStops = nullptr; return false; }
    g_gradientHeadCache = g_currentCoreRGB;
    g_gradientTailCache = g_gradientTailRGB;
    return true;
}

void SmoothTrail(int iterations, int renderLength) {
    auto& current = g_renderCache.smoothed;
    auto& next = g_renderCache.subdivision;
    current.clear();
    next.clear();
    const int pointCount = std::min(g_historyCount, std::max(renderLength, 2));
    for (int i = 0; i < pointCount; ++i) {
        const POINT& point = HistoryAt(i);
        current.push_back(D2D1::Point2F(
            static_cast<float>(point.x) + static_cast<float>(g_tailOffsetX) * g_currentDpiScale,
            static_cast<float>(point.y) + static_cast<float>(g_tailOffsetY) * g_currentDpiScale));
    }
    for (int iteration = 0; iteration < iterations && current.size() >= 3; ++iteration) {
        next.clear();
        next.reserve(current.size() * 2);
        next.push_back(current.front());
        for (size_t i = 0; i + 1 < current.size(); ++i) {
            const auto& a = current[i];
            const auto& b = current[i + 1];
            next.push_back(D2D1::Point2F(0.75f * a.x + 0.25f * b.x, 0.75f * a.y + 0.25f * b.y));
            next.push_back(D2D1::Point2F(0.25f * a.x + 0.75f * b.x, 0.25f * a.y + 0.75f * b.y));
        }
        next.push_back(current.back());
        current.swap(next);
    }
    g_renderCache.PrepareTaper();
}

RECT CalculateTrailBounds(float maxHalfWidth) {
    if (g_renderCache.smoothed.empty()) return {0, 0, 0, 0};
    float minX = g_renderCache.smoothed.front().x;
    float minY = g_renderCache.smoothed.front().y;
    float maxX = minX;
    float maxY = minY;
    for (const auto& point : g_renderCache.smoothed) {
        minX = std::min(minX, point.x);
        minY = std::min(minY, point.y);
        maxX = std::max(maxX, point.x);
        maxY = std::max(maxY, point.y);
    }
    const float padding = maxHalfWidth + kRenderPadding;
    RECT result = {
        static_cast<LONG>(floorf(minX - padding)),
        static_cast<LONG>(floorf(minY - padding)),
        static_cast<LONG>(ceilf(maxX + padding)),
        static_cast<LONG>(ceilf(maxY + padding)),
    };
    if (result.right <= result.left) result.right = result.left + 1;
    if (result.bottom <= result.top) result.bottom = result.top + 1;
    return result;
}

static void DrawTrailStroke(ID2D1RenderTarget* target, ID2D1Brush* brush, float halfWidth, bool drawHead) {
    const auto& points = g_renderCache.smoothed;
    const auto& taper = g_renderCache.taper;
    if (points.size() < 2 || taper.size() < points.size() || !brush) return;
    for (size_t i = 0; i + 1 < points.size(); ++i) {
        const float width = std::max(0.1f, halfWidth * taper[i] * 2.0f);
        target->DrawLine(points[i], points[i + 1], brush, width, g_strokeStyle);
    }
    if (drawHead) target->FillEllipse(D2D1::Ellipse(points.front(), halfWidth, halfWidth), brush);
}

static int GetEffectiveTailLength(float speedNorm) {
    if (!g_speedScaling) return g_tailLength;
    return std::max(2, static_cast<int>(g_tailLength * (0.55f + 0.45f * std::clamp(speedNorm, 0.0f, 1.0f))));
}

void UpdateTrailState(const POINT& previousPoint, const POINT& point, float velocity) {
    const bool startedSmearing = velocity > g_triggerVelocity && !g_isSmearing;
    if (startedSmearing) {
        g_isSmearing = true;
        g_lowVelocityFrames = 0;
        g_fadeAlpha = 1.0f;
        g_smoothedSpeedNorm = 0.0f;
    } else if (velocity < g_stopVelocity && g_isSmearing) {
        if (++g_lowVelocityFrames > 2) {
            g_isSmearing = false;
            g_frozenSpeedNorm = g_smoothedSpeedNorm;
        }
    } else if (g_isSmearing) {
        g_lowVelocityFrames = 0;
    }
    if (g_isSmearing) {
        if (startedSmearing) {
            // A fading tail belongs to the previous motion segment. Do not connect
            // it to a new segment after a discontinuous cursor repositioning.
            HistoryClear();
        }
        const float target = std::clamp((velocity - g_stopVelocity) / (g_triggerVelocity * 2.0f), 0.0f, 1.0f);
        g_smoothedSpeedNorm = g_smoothedSpeedNorm * 0.55f + target * 0.45f;
        const float dx = static_cast<float>(point.x - previousPoint.x);
        const float dy = static_cast<float>(point.y - previousPoint.y);
        const float distance = sqrtf(dx * dx + dy * dy);
        int interpolationSteps = 1;
        // Start from the current point only. A discontinuous cursor repositioning
        // must not be turned into a streak when the velocity gate first opens.
        if (!startedSmearing
            && g_fastMoveInterpolationEnabled
            && distance > kFastMoveInterpolationSpacingPx) {
            const int requiredSteps = static_cast<int>(ceilf(distance / kFastMoveInterpolationSpacingPx));
            interpolationSteps = std::clamp(requiredSteps, 1, g_maxInterpolationSteps + 1);
        }
        // History is stored newest-first, so push the interpolated path toward
        // the current endpoint in ascending order. Work is capped at six points.
        for (int step = 1; step <= interpolationSteps; ++step) {
            const float progress = static_cast<float>(step) / interpolationSteps;
            const POINT sample = {
                static_cast<LONG>(std::lround(previousPoint.x + dx * progress)),
                static_cast<LONG>(std::lround(previousPoint.y + dy * progress)),
            };
            HistoryPushFront(sample);
            while (g_historyCount > g_tailLength) HistoryPopBack();
        }
    } else if (g_fadeEnabled) {
        g_fadeAlpha *= g_fadeDecay;
        if (g_fadeAlpha < kFadeCutoff || g_historyCount < 2) {
            HistoryClear();
            g_fadeAlpha = 0.0f;
        }
    } else {
        if (g_historyCount > 0) HistoryPopBack();
        g_fadeAlpha = 1.0f;
    }
}

struct ColorCandidate { uint32_t rgb; int count; };

static HCURSOR g_cachedCursorCandidateHandle = nullptr;
static std::vector<ColorCandidate> g_cachedCursorColorCandidates;
static bool g_cachedCursorCandidatesReady = false;
constexpr float kAutoColorLuminanceChangeThreshold = 0.04f;

static bool ExtractCursorColorCandidates(HCURSOR cursorHandle, std::vector<ColorCandidate>& out) {
    out.clear();
    if (!cursorHandle) return false;
    ICONINFO iconInfo = {};
    if (!GetIconInfo(cursorHandle, &iconInfo)) return false;
    const auto cleanup = [&] {
        if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
        if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
    };
    if (iconInfo.hbmColor) {
        BITMAP bitmap = {};
        if (GetObject(iconInfo.hbmColor, sizeof(bitmap), &bitmap) && bitmap.bmWidth > 0 && bitmap.bmHeight > 0 && bitmap.bmWidth <= 256 && bitmap.bmHeight <= 256) {
            const int width = bitmap.bmWidth;
            const int height = bitmap.bmHeight;
            BITMAPINFO info = {};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = width;
            info.bmiHeader.biHeight = -height;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            std::vector<uint32_t> pixels(static_cast<size_t>(width) * height);
            HDC screen = GetScreenDC();
            if (screen && GetDIBits(screen, iconInfo.hbmColor, 0, height, pixels.data(), &info, DIB_RGB_COLORS) == height) {
                std::unordered_map<uint32_t, int> histogram;
                histogram.reserve(64);
                for (uint32_t pixel : pixels) if (((pixel >> 24) & 0xFF) >= kCursorAlphaThreshold) ++histogram[pixel & 0x00FFFFFF];
                if (!histogram.empty()) {
                    out.reserve(histogram.size());
                    for (const auto& entry : histogram) out.push_back({entry.first, entry.second});
                    std::sort(out.begin(), out.end(), [](const ColorCandidate& a, const ColorCandidate& b) { return a.count > b.count; });
                    cleanup();
                    return true;
                }

                if (iconInfo.hbmMask) {
                    BITMAP maskBitmap = {};
                    if (GetObject(iconInfo.hbmMask, sizeof(maskBitmap), &maskBitmap) && maskBitmap.bmWidth >= width && maskBitmap.bmHeight >= height) {
                        BITMAPINFO maskInfo = {};
                        maskInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                        maskInfo.bmiHeader.biWidth = width;
                        maskInfo.bmiHeader.biHeight = -height;
                        maskInfo.bmiHeader.biPlanes = 1;
                        maskInfo.bmiHeader.biBitCount = 1;
                        maskInfo.bmiHeader.biCompression = BI_RGB;
                        maskInfo.bmiHeader.biClrUsed = 2;
                        const size_t maskStride = ((static_cast<size_t>(width) + 31u) / 32u) * 4u;
                        std::vector<uint8_t> maskBits(maskStride * static_cast<size_t>(height));
                        if (GetDIBits(screen, iconInfo.hbmMask, 0, height, maskBits.data(), &maskInfo, DIB_RGB_COLORS) == height) {
                            std::unordered_map<uint32_t, int> maskHistogram;
                            maskHistogram.reserve(64);
                            for (int y = 0; y < height; ++y) {
                                for (int x = 0; x < width; ++x) {
                                    const uint8_t rowByte = maskBits[static_cast<size_t>(y) * maskStride + static_cast<size_t>(x >> 3)];
                                    const bool transparent = (rowByte & (0x80u >> (x & 7))) != 0;
                                    if (!transparent) {
                                        const uint32_t rgb = pixels[static_cast<size_t>(y) * width + x] & 0x00FFFFFF;
                                        ++maskHistogram[rgb];
                                    }
                                }
                            }
                            out.reserve(maskHistogram.size());
                            for (const auto& entry : maskHistogram) out.push_back({entry.first, entry.second});
                            std::sort(out.begin(), out.end(), [](const ColorCandidate& a, const ColorCandidate& b) { return a.count > b.count; });
                            cleanup();
                            return !out.empty();
                        }
                    }
                }
            }
        }
    }
    cleanup();
    return false;
}

HDC g_backgroundSamplerDc = nullptr;
HBITMAP g_backgroundSamplerBitmap = nullptr;
HGDIOBJ g_backgroundSamplerOriginal = nullptr;
uint32_t* g_backgroundSamplerPixels = nullptr;
int g_backgroundSamplerSize = 0;
bool g_backgroundLuminanceValid = false;
POINT g_backgroundLuminanceCenter = {0, 0};
DWORD g_backgroundLuminanceUpdated = 0;
float g_backgroundLuminance = 0.5f;

static void ResetAutoColorSamplingCache() {
    g_backgroundLuminanceValid = false;
    g_backgroundLuminanceCenter = {0, 0};
    g_backgroundLuminanceUpdated = 0;
    g_autoColorPickLuminanceValid = false;
}

static void ReleaseBackgroundSampler() {
    if (g_backgroundSamplerDc) {
        if (g_backgroundSamplerOriginal) SelectObject(g_backgroundSamplerDc, g_backgroundSamplerOriginal);
        DeleteDC(g_backgroundSamplerDc);
        g_backgroundSamplerDc = nullptr;
    }
    if (g_backgroundSamplerBitmap) {
        DeleteObject(g_backgroundSamplerBitmap);
        g_backgroundSamplerBitmap = nullptr;
    }
    g_backgroundSamplerOriginal = nullptr;
    g_backgroundSamplerPixels = nullptr;
    g_backgroundSamplerSize = 0;
    g_backgroundLuminanceValid = false;
}

static float SampleBackgroundLuminance(DWORD now, POINT center) {
    if (g_backgroundLuminanceValid) {
        const int64_t dx = static_cast<int64_t>(center.x) -
                           static_cast<int64_t>(g_backgroundLuminanceCenter.x);
        const int64_t dy = static_cast<int64_t>(center.y) -
                           static_cast<int64_t>(g_backgroundLuminanceCenter.y);
        const int64_t distanceThreshold = kBackgroundResampleDistancePx;
        // Short-circuit large coordinate deltas before squaring them; the final
        // squared-distance comparison only runs when both deltas are below 20.
        // A time limit is mandatory, while cursor travel is an additional gate
        // so high-speed movement cannot trigger a capture on every sample.
        const bool movedFarEnough =
            dx <= -distanceThreshold || dx >= distanceThreshold ||
            dy <= -distanceThreshold || dy >= distanceThreshold ||
            dx * dx + dy * dy >= distanceThreshold * distanceThreshold;
        if (now - g_backgroundLuminanceUpdated < kBackgroundResampleIntervalMs
            || !movedFarEnough) {
            return g_backgroundLuminance;
        }
    }

    const int size = kBackgroundSampleRadius * 2 + 1;
    HDC screen = GetScreenDC();
    if (!screen) return g_backgroundLuminanceValid ? g_backgroundLuminance : 0.5f;
    if (g_backgroundSamplerSize != size || !g_backgroundSamplerDc || !g_backgroundSamplerBitmap || !g_backgroundSamplerPixels) {
        ReleaseBackgroundSampler();
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = size;
        info.bmiHeader.biHeight = -size;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        g_backgroundSamplerBitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!g_backgroundSamplerBitmap || !bits) { ReleaseBackgroundSampler(); return g_backgroundLuminanceValid ? g_backgroundLuminance : 0.5f; }
        g_backgroundSamplerPixels = static_cast<uint32_t*>(bits);
        g_backgroundSamplerDc = CreateCompatibleDC(screen);
        if (!g_backgroundSamplerDc) { ReleaseBackgroundSampler(); return g_backgroundLuminanceValid ? g_backgroundLuminance : 0.5f; }
        g_backgroundSamplerOriginal = SelectObject(g_backgroundSamplerDc, g_backgroundSamplerBitmap);
        g_backgroundSamplerSize = size;
    }
    if (!BitBlt(g_backgroundSamplerDc, 0, 0, size, size, screen, center.x - kBackgroundSampleRadius, center.y - kBackgroundSampleRadius, SRCCOPY)) return g_backgroundLuminanceValid ? g_backgroundLuminance : 0.5f;
    // GDI batches drawing calls, so the blit has to be flushed before the DIB
    // section bits are read directly.
    GdiFlush();
    double sum = 0.0;
    int count = 0;
    for (int yIndex = 0; yIndex < kBackgroundSampleGrid; ++yIndex) {
        const int y = (size - 1) * yIndex / (kBackgroundSampleGrid - 1);
        for (int xIndex = 0; xIndex < kBackgroundSampleGrid; ++xIndex) {
            const int x = (size - 1) * xIndex / (kBackgroundSampleGrid - 1);
            const uint32_t pixel = g_backgroundSamplerPixels[y * size + x];
            sum += RgbLuminance(static_cast<uint8_t>((pixel >> 16) & 0xFF), static_cast<uint8_t>((pixel >> 8) & 0xFF), static_cast<uint8_t>(pixel & 0xFF));
            ++count;
        }
    }
    const float luminance = count ? static_cast<float>(sum / count) : 0.5f;
    g_backgroundLuminance = luminance;
    g_backgroundLuminanceCenter = center;
    g_backgroundLuminanceUpdated = now;
    g_backgroundLuminanceValid = true;
    return luminance;
}

static uint32_t ResolveOutlineColor(uint32_t core) {
    if (g_outlineColorMode == 1) return g_manualOutlineRGB;
    return RgbLuminance(core) > 0.5f ? kFallbackOuterColor : kFallbackCoreColor;
}

static void UpdateTrailColorIfNeeded(DWORD now) {
    if (g_trailColorMode == 0) {
        g_currentCoreRGB = g_manualColorRGB;
        g_currentOuterRGB = ResolveOutlineColor(g_currentCoreRGB);
        return;
    }

    CURSORINFO cursorInfo = {sizeof(cursorInfo)};
    if (!GetCursorInfo(&cursorInfo) || !(cursorInfo.flags & CURSOR_SHOWING) || !cursorInfo.hCursor) {
        g_lastAutoColorCursor = nullptr;
        g_autoColorInitialized = true;
        g_autoColorPickLuminanceValid = false;
        g_cachedCursorCandidateHandle = nullptr;
        g_cachedCursorColorCandidates.clear();
        g_cachedCursorCandidatesReady = false;
        g_currentCoreRGB = kFallbackCoreColor;
        g_currentOuterRGB = ResolveOutlineColor(g_currentCoreRGB);
        return;
    }

    const HCURSOR cursorHandle = cursorInfo.hCursor;
    const bool cursorChanged = !g_autoColorInitialized || cursorHandle != g_lastAutoColorCursor;
    const bool intervalElapsed = g_autoResampleInterval > 0
        && now - g_lastAutoColorUpdate >= static_cast<DWORD>(g_autoResampleInterval);
    const bool refreshCandidates = cursorChanged || intervalElapsed
        || !g_cachedCursorCandidatesReady
        || g_cachedCursorCandidateHandle != cursorHandle;

    POINT point = {};
    if (!GetCursorPos(&point)) return;

    // SampleBackgroundLuminance already throttles screen reads by both time and
    // cursor travel. Use its cached value to adapt contrast without re-reading
    // the cursor bitmap when only the content beneath the pointer changed.
    const float background = SampleBackgroundLuminance(now, point);

    if (refreshCandidates) {
        g_cachedCursorColorCandidates.clear();
        g_cachedCursorCandidatesReady = true;
        g_cachedCursorCandidateHandle = cursorHandle;
        ExtractCursorColorCandidates(cursorHandle, g_cachedCursorColorCandidates);
        g_lastAutoColorCursor = cursorHandle;
        g_lastAutoColorUpdate = now;
        g_autoColorInitialized = true;
    }

    if (g_cachedCursorColorCandidates.empty()) {
        g_currentCoreRGB = kFallbackCoreColor;
        g_currentOuterRGB = ResolveOutlineColor(g_currentCoreRGB);
        g_autoColorPickLuminanceValid = false;
        return;
    }

    const bool backgroundChanged = !g_autoColorPickLuminanceValid
        || fabsf(background - g_lastAutoColorPickLuminance) >= kAutoColorLuminanceChangeThreshold;
    if (!refreshCandidates && !backgroundChanged) return;

    // Keep the existing color when it still has acceptable contrast. Using a
    // lower hold threshold than the selection threshold prevents color flapping.
    if (!refreshCandidates
        && ContrastRatio(RgbLuminance(g_currentCoreRGB), background)
            >= kAutoColorHysteresisContrast) {
        g_lastAutoColorPickLuminance = background;
        g_autoColorPickLuminanceValid = true;
        return;
    }

    // In Auto mode the overlay is excluded from screen capture, so the sample
    // can't pick up the trail's own pixels and the window need not be hidden.
    for (const ColorCandidate& candidate : g_cachedCursorColorCandidates) {
        if (ContrastRatio(RgbLuminance(candidate.rgb), background) >= kMinColorContrast) {
            g_currentCoreRGB = candidate.rgb;
            g_currentOuterRGB = ResolveOutlineColor(candidate.rgb);
            g_lastAutoColorPickLuminance = background;
            g_autoColorPickLuminanceValid = true;
            return;
        }
    }

    g_currentCoreRGB = kFallbackCoreColor;
    g_currentOuterRGB = ResolveOutlineColor(g_currentCoreRGB);
    g_lastAutoColorPickLuminance = background;
    g_autoColorPickLuminanceValid = true;
}

static D2D1_POINT_2F OrnamentAnchorPoint() {
    return D2D1::Point2F(
        static_cast<float>(g_lastPos.x) + g_ornamentAnchorOffsetX * g_currentDpiScale,
        static_cast<float>(g_lastPos.y) + g_ornamentAnchorOffsetY * g_currentDpiScale);
}

static void ReleaseOrnamentBitmaps() {
    for (OrnamentImage& image : g_ornamentImages) {
        if (image.bitmap) {
            image.bitmap->Release();
            image.bitmap = nullptr;
        }
    }
}

static void ReleaseOrnamentAssets() {
    ReleaseOrnamentBitmaps();
    g_ornamentImages.clear();
    g_ornamentNodes.clear();
    if (g_wicFactory) {
        g_wicFactory->Release();
        g_wicFactory = nullptr;
    }
    g_ornamentAssetsReady = false;
    g_ornamentPhysicsInitialized = false;
    g_ornamentRedrawPending = false;
    g_ornamentSmoothedSpeedNorm = 0.0f;
    g_ornamentSmoothedSpeedPx = 0.0f;
}

static int FindOrnamentImageIndexById(const std::wstring& id) {
    if (id.empty()) return -1;
    for (size_t i = 0; i < g_ornamentImages.size(); ++i) {
        if (_wcsicmp(g_ornamentImages[i].id.c_str(), id.c_str()) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

static bool DecodeOrnamentImage(
    const OrnamentImageSpec& spec,
    OrnamentImage& output,
    uint64_t& decodedBytes) {
    output.id = spec.id;
    output.path = spec.path;
    if (!g_wicFactory) return false;

    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    const auto releaseInterfaces = [&] {
        if (converter) { converter->Release(); converter = nullptr; }
        if (frame) { frame->Release(); frame = nullptr; }
        if (decoder) { decoder->Release(); decoder = nullptr; }
    };

    HRESULT hr = g_wicFactory->CreateDecoderFromFilename(
        spec.path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(hr) || !decoder) {
        Wh_Log(L"Failed to open ornament image: %s", spec.path.c_str());
        releaseInterfaces();
        return false;
    }
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr) || !frame) {
        Wh_Log(L"Failed to read first frame of ornament image: %s", spec.path.c_str());
        releaseInterfaces();
        return false;
    }
    UINT width = 0;
    UINT height = 0;
    hr = frame->GetSize(&width, &height);
    if (FAILED(hr)
        || width == 0
        || height == 0
        || width > static_cast<UINT>(kOrnamentMaxImageDimension)
        || height > static_cast<UINT>(kOrnamentMaxImageDimension)) {
        Wh_Log(L"Unsupported ornament image dimensions: %s", spec.path.c_str());
        releaseInterfaces();
        return false;
    }
    const uint64_t imageBytes = static_cast<uint64_t>(width) * height * 4ull;
    if (imageBytes > kOrnamentMaxDecodedBytes
        || decodedBytes + imageBytes > kOrnamentMaxDecodedBytes) {
        Wh_Log(L"Ornament image memory limit reached: %s", spec.path.c_str());
        releaseInterfaces();
        return false;
    }
    hr = g_wicFactory->CreateFormatConverter(&converter);
    if (FAILED(hr) || !converter) {
        releaseInterfaces();
        return false;
    }
    hr = converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        Wh_Log(L"Failed to convert ornament image pixels: %s", spec.path.c_str());
        releaseInterfaces();
        return false;
    }
    const UINT stride = width * 4u;
    const UINT bufferSize = stride * height;
    output.pixels.resize(bufferSize);
    hr = converter->CopyPixels(nullptr, stride, bufferSize, output.pixels.data());
    if (FAILED(hr)) {
        output.pixels.clear();
        Wh_Log(L"Failed to copy ornament image pixels: %s", spec.path.c_str());
        releaseInterfaces();
        return false;
    }
    output.width = width;
    output.height = height;
    decodedBytes += imageBytes;
    releaseInterfaces();
    return true;
}

static void BuildOrnamentImages() {
    g_ornamentImages.clear();
    g_ornamentImages.reserve(g_ornamentImageSpecs.size());
    if (!g_ornamentImageSpecs.empty()) {
        const HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&g_wicFactory));
        if (FAILED(hr) || !g_wicFactory) {
            Wh_Log(L"Failed to create WIC imaging factory for ornaments");
        }
    }
    uint64_t decodedBytes = 0;
    for (const OrnamentImageSpec& spec : g_ornamentImageSpecs) {
        OrnamentImage image;
        DecodeOrnamentImage(spec, image, decodedBytes);
        g_ornamentImages.push_back(std::move(image));
    }
}

static void BuildOrnamentNodes() {
    g_ornamentNodes.clear();
    g_ornamentNodes.reserve(g_ornamentNodeSpecs.size());
    for (const OrnamentNodeSpec& spec : g_ornamentNodeSpecs) {
        OrnamentNode node;
        // A node without a decoded image still renders the circular fallback,
        // so the index stays -1 and the preset remains usable with no image.
        node.imageIndex = FindOrnamentImageIndexById(spec.imageId);
        node.ropeT = spec.ropeT;
        node.sizePx = spec.sizePx;
        node.angleDegrees = spec.angleDegrees;
        node.pivotX = spec.pivotX;
        node.pivotY = spec.pivotY;
        node.opacity = spec.opacity;
        g_ornamentNodes.push_back(node);
    }
}

static void ResetOrnamentPhysics() {
    const int segments = std::clamp(g_ornamentRopeSegments, 1, kOrnamentMaxRopeSegments);
    g_ornamentRopePointCount = segments + 1;
    const float length = std::max(1.0f, g_ornamentRopeLengthPx * g_currentDpiScale);
    g_ornamentRopeSegmentPx = length / static_cast<float>(segments);
    D2D1_POINT_2F anchor = OrnamentAnchorPoint();
    for (int i = 0; i < g_ornamentRopePointCount; ++i) {
        const D2D1_POINT_2F point = D2D1::Point2F(
            anchor.x,
            anchor.y + g_ornamentRopeSegmentPx * static_cast<float>(i));
        g_ornamentRopePoints[i] = point;
        g_ornamentRopePrevious[i] = point;
    }
    g_ornamentLastAnchor = anchor;
    g_ornamentSmoothedSpeedNorm = 0.0f;
    g_ornamentSmoothedSpeedPx = 0.0f;
    g_ornamentPhysicsInitialized = true;
    g_ornamentRedrawPending = true;
}

static void EnsureOrnamentAssetsLoaded() {
    if (!g_ornamentEnabled) {
        if (g_ornamentAssetsReady || !g_ornamentImages.empty()) ReleaseOrnamentAssets();
        return;
    }
    const bool dirty = g_ornamentAssetsDirty.exchange(false, std::memory_order_acq_rel);
    if (!dirty && g_ornamentAssetsReady) return;
    ReleaseOrnamentAssets();
    BuildOrnamentImages();
    BuildOrnamentNodes();
    g_ornamentAssetsReady = true;
    g_ornamentPhysicsInitialized = false;
    g_ornamentRedrawPending = true;
}

static void EnsureOrnamentBitmaps() {
    if (!g_renderTarget || !g_ornamentAssetsReady) return;
    for (OrnamentImage& image : g_ornamentImages) {
        if (image.bitmap || image.width == 0 || image.height == 0 || image.pixels.empty()) continue;
        const D2D1_BITMAP_PROPERTIES properties = D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0f,
            96.0f);
        const D2D1_SIZE_U size = D2D1::SizeU(image.width, image.height);
        const UINT stride = image.width * 4u;
        const HRESULT hr = g_renderTarget->CreateBitmap(
            size,
            image.pixels.data(),
            stride,
            properties,
            &image.bitmap);
        if (FAILED(hr)) {
            image.bitmap = nullptr;
            Wh_Log(L"Failed to upload ornament image: %s", image.path.c_str());
        }
    }
}

static void UpdateOrnamentNodeGeometry(OrnamentNode& node) {
    float aspect = 1.0f;
    if (node.imageIndex >= 0
        && node.imageIndex < static_cast<int>(g_ornamentImages.size())) {
        const OrnamentImage& image = g_ornamentImages[static_cast<size_t>(node.imageIndex)];
        if (image.width > 0 && image.height > 0) {
            aspect = static_cast<float>(image.width) / static_cast<float>(image.height);
        }
    }
    const float size = std::max(1.0f, node.sizePx * g_currentDpiScale);
    const float width = aspect >= 1.0f ? size : size * aspect;
    const float height = aspect >= 1.0f ? size / aspect : size;
    node.localBounds = D2D1::RectF(
        -node.pivotX * width,
        -node.pivotY * height,
        (1.0f - node.pivotX) * width,
        (1.0f - node.pivotY) * height);
}

static D2D1_MATRIX_3X2_F OrnamentNodeMatrix(const OrnamentNode& node) {
    return D2D1::Matrix3x2F::Scale(node.scaleX, node.scaleY)
        * D2D1::Matrix3x2F::Rotation(node.drawAngleDegrees)
        * D2D1::Matrix3x2F::Translation(node.position.x, node.position.y);
}

static D2D1_POINT_2F OrnamentRopePointAt(float t) {
    const int count = g_ornamentRopePointCount;
    if (count <= 0) return D2D1::Point2F(0.0f, 0.0f);
    if (count == 1) return g_ornamentRopePoints[0];
    const float location =
        std::clamp(t, 0.0f, 1.0f) * static_cast<float>(count - 1);
    const int index = std::min(static_cast<int>(location), count - 2);
    const float fraction = location - static_cast<float>(index);
    const D2D1_POINT_2F& a = g_ornamentRopePoints[index];
    const D2D1_POINT_2F& b = g_ornamentRopePoints[index + 1];
    return D2D1::Point2F(
        a.x + (b.x - a.x) * fraction,
        a.y + (b.y - a.y) * fraction);
}

static void UpdateOrnamentDrawStates() {
    if (g_ornamentNodes.empty()) return;
    if (g_ornamentRopePointCount < 2) return;
    const float speed = std::clamp(g_ornamentSmoothedSpeedNorm, 0.0f, 1.0f);
    const float stretchScale = 1.0f + g_ornamentStretch * speed;
    const float compensatingScale = 1.0f / sqrtf(std::max(0.01f, stretchScale));
    for (OrnamentNode& node : g_ornamentNodes) {
        const float t = std::clamp(node.ropeT, 0.0f, 1.0f);
        node.position = OrnamentRopePointAt(t);
        const D2D1_POINT_2F before = OrnamentRopePointAt(t - 0.02f);
        const D2D1_POINT_2F after = OrnamentRopePointAt(t + 0.02f);
        const float dx = after.x - before.x;
        const float dy = after.y - before.y;
        if (dx * dx + dy * dy > kOrnamentConstraintEpsilonPx) {
            node.drawAngleDegrees =
                atan2f(-dx, dy) * 180.0f / kOrnamentPi + node.angleDegrees;
        } else {
            node.drawAngleDegrees = node.angleDegrees;
        }
        node.scaleY = stretchScale;
        node.scaleX = compensatingScale;
        UpdateOrnamentNodeGeometry(node);
    }
}

static void ConstrainOrnamentDistance(
    D2D1_POINT_2F& moving,
    D2D1_POINT_2F& fixed,
    float targetDistance,
    bool moveBoth) {
    float dx = moving.x - fixed.x;
    float dy = moving.y - fixed.y;
    float distance = sqrtf(dx * dx + dy * dy);
    if (distance < kOrnamentConstraintEpsilonPx) {
        moving = D2D1::Point2F(fixed.x, fixed.y + targetDistance);
        return;
    }
    const float correction = (distance - targetDistance) / distance;
    if (moveBoth) {
        const float half = correction * 0.5f;
        moving.x -= dx * half;
        moving.y -= dy * half;
        fixed.x += dx * half;
        fixed.y += dy * half;
    } else {
        moving.x -= dx * correction;
        moving.y -= dy * correction;
    }
}

static void StepOrnamentPhysics(
    float dtSeconds,
    float anchorStepX,
    float anchorStepY) {
    const int count = g_ornamentRopePointCount;
    if (count < 2) return;
    const float retain = powf(std::clamp(1.0f - g_ornamentDamping / 100.0f, 0.5f, 1.0f), dtSeconds * kOrnamentSimulationHz);
    const float gravityStep = g_ornamentGravity * g_currentDpiScale * dtSeconds * dtSeconds;
    for (int i = 1; i < count; ++i) {
        D2D1_POINT_2F& point = g_ornamentRopePoints[i];
        // Verlet velocity is world-space. Damping must act on the velocity
        // relative to the moving anchor; otherwise dragging the cursor at a
        // constant speed looks like wind and tilts the rope by a speed-dependent
        // angle. Subtract the frame displacement, then add it back to position.
        const float vx = point.x - g_ornamentRopePrevious[i].x - anchorStepX;
        const float vy = point.y - g_ornamentRopePrevious[i].y - anchorStepY;
        g_ornamentRopePrevious[i] = point;
        point.x += vx * retain + anchorStepX;
        point.y += vy * retain + anchorStepY + gravityStep;
    }

    D2D1_POINT_2F anchor = OrnamentAnchorPoint();
    for (int iteration = 0; iteration < g_ornamentConstraintIterations; ++iteration) {
        g_ornamentRopePoints[0] = anchor;
        ConstrainOrnamentDistance(
            g_ornamentRopePoints[1],
            anchor,
            g_ornamentRopeSegmentPx,
            false);
        for (int i = 1; i < count - 1; ++i) {
            ConstrainOrnamentDistance(
                g_ornamentRopePoints[i + 1],
                g_ornamentRopePoints[i],
                g_ornamentRopeSegmentPx,
                true);
        }
    }
    g_ornamentRopePoints[0] = anchor;
    for (int i = 0; i < count; ++i) {
        if (!std::isfinite(g_ornamentRopePoints[i].x)
            || !std::isfinite(g_ornamentRopePoints[i].y)) {
            ResetOrnamentPhysics();
            return;
        }
    }
}

static bool OrnamentAnimating() {
    for (int i = 1; i < g_ornamentRopePointCount; ++i) {
        const float dx = g_ornamentRopePoints[i].x - g_ornamentRopePrevious[i].x;
        const float dy = g_ornamentRopePoints[i].y - g_ornamentRopePrevious[i].y;
        if (dx * dx + dy * dy > kOrnamentRestVelocityPx * kOrnamentRestVelocityPx) return true;
    }
    return false;
}

static void UpdateOrnamentPhysics(float velocity, LONGLONG nowQpc) {
    const bool visible = g_ornamentEnabled && g_ornamentAssetsReady && !g_ornamentNodes.empty();
    if (!visible) {
        g_ornamentPhysicsInitialized = false;
        g_ornamentRedrawPending = false;
        g_ornamentSmoothedSpeedPx = 0.0f;
        g_ornamentSmoothedSpeedNorm = 0.0f;
        return;
    }

    const float targetSpeedPx = std::max(0.0f, velocity);
    g_ornamentSmoothedSpeedPx += (targetSpeedPx - g_ornamentSmoothedSpeedPx)
        * kOrnamentSpeedSmoothing;
    g_ornamentSmoothedSpeedNorm = std::clamp(
        g_ornamentSmoothedSpeedPx / kOrnamentSpeedReferencePxPerSecond,
        0.0f,
        1.0f);

    if (!g_ornamentPhysicsInitialized) {
        ResetOrnamentPhysics();
        g_ornamentLastStepQpc = nowQpc;
    }

    const D2D1_POINT_2F anchor = OrnamentAnchorPoint();
    const float anchorStepX = anchor.x - g_ornamentLastAnchor.x;
    const float anchorStepY = anchor.y - g_ornamentLastAnchor.y;

    float elapsed = 0.0f;
    if (g_ornamentLastStepQpc > 0 && nowQpc > g_ornamentLastStepQpc && g_qpcFrequency > 0) {
        elapsed = static_cast<float>(nowQpc - g_ornamentLastStepQpc)
            / static_cast<float>(g_qpcFrequency);
    }
    elapsed = std::clamp(elapsed, 0.0f, kOrnamentMaxCatchUpSeconds);
    g_ornamentLastStepQpc = nowQpc;
    if (elapsed > 0.0f) {
        const float fixedStep = 1.0f / kOrnamentSimulationHz;
        int steps = std::max(1, static_cast<int>(ceilf(elapsed / fixedStep)));
        steps = std::min(steps, kOrnamentMaxSubSteps);
        const float step = elapsed / static_cast<float>(steps);
        const float stepX = anchorStepX / static_cast<float>(steps);
        const float stepY = anchorStepY / static_cast<float>(steps);
        for (int i = 0; i < steps; ++i) StepOrnamentPhysics(step, stepX, stepY);
        g_ornamentLastAnchor = anchor;
    }

    if (OrnamentAnimating()) {
        g_ornamentRedrawPending = true;
    }
}

static bool OrnamentVisible() {
    return g_ornamentEnabled
        && g_ornamentAssetsReady
        && !g_ornamentNodes.empty()
        && g_ornamentOpacity > 0.0f;
}

static bool OrnamentActive() {
    return OrnamentVisible()
        && (g_ornamentRedrawPending || OrnamentAnimating());
}

// A click or wheel impulse is a one-shot velocity applied to the whole chain.
static void QueueOrnamentImpulse(float impulseY, LONGLONG nowQpc) {
    if (!OrnamentVisible()) return;
    if (!g_ornamentPhysicsInitialized) {
        ResetOrnamentPhysics();
    }
    // Rebase the simulation clock so the impulse is not consumed by a catch-up
    // step covering the idle interval before the input.
    g_ornamentLastStepQpc = nowQpc;
    // The impulse is a velocity in pixels per second applied once. Verlet keeps
    // velocity in position deltas, so convert it with the fixed simulation step.
    const float stepSeconds = 1.0f / kOrnamentSimulationHz;
    const float deltaY = impulseY * stepSeconds;
    for (int i = 1; i < g_ornamentRopePointCount; ++i) {
        g_ornamentRopePrevious[i].y += deltaY;
    }
    g_ornamentRedrawPending = true;
    g_interactionPending = true;
    if (g_inputWakeEvent) SetEvent(g_inputWakeEvent);
}

static void SuppressOrnament(LONGLONG nowQpc) {
    g_ornamentPhysicsInitialized = false;
    g_ornamentLastStepQpc = nowQpc;
    g_ornamentRedrawPending = false;
    g_ornamentSmoothedSpeedPx = 0.0f;
    g_ornamentSmoothedSpeedNorm = 0.0f;
}

static RECT CalculateOrnamentBounds() {
    RECT bounds = {};
    bool hasBounds = false;
    const float padding = 2.0f * g_currentDpiScale;
    const float ropeOpacity =
        std::clamp(g_ornamentRopeOpacity * g_ornamentOpacity, 0.0f, 1.0f);
    if (g_ornamentRopePointCount >= 2 && ropeOpacity > kOrnamentMinDrawOpacity) {
        const float ropePadding =
            padding + g_ornamentRopeWidthPx * g_currentDpiScale * 4.0f;
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        for (int i = 0; i < g_ornamentRopePointCount; ++i) {
            minX = std::min(minX, g_ornamentRopePoints[i].x);
            minY = std::min(minY, g_ornamentRopePoints[i].y);
            maxX = std::max(maxX, g_ornamentRopePoints[i].x);
            maxY = std::max(maxY, g_ornamentRopePoints[i].y);
        }
        bounds.left = static_cast<LONG>(floorf(minX - ropePadding));
        bounds.top = static_cast<LONG>(floorf(minY - ropePadding));
        bounds.right = static_cast<LONG>(ceilf(maxX + ropePadding));
        bounds.bottom = static_cast<LONG>(ceilf(maxY + ropePadding));
        hasBounds = true;
    }
    for (const OrnamentNode& node : g_ornamentNodes) {
        if (node.opacity * g_ornamentOpacity <= kOrnamentMinDrawOpacity) continue;
        const D2D1_MATRIX_3X2_F matrix = OrnamentNodeMatrix(node);
        const D2D1_POINT_2F corners[4] = {
            D2D1::Point2F(node.localBounds.left, node.localBounds.top),
            D2D1::Point2F(node.localBounds.right, node.localBounds.top),
            D2D1::Point2F(node.localBounds.right, node.localBounds.bottom),
            D2D1::Point2F(node.localBounds.left, node.localBounds.bottom),
        };
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        for (const D2D1_POINT_2F& corner : corners) {
            const D2D1_POINT_2F point = D2D1::Point2F(
                corner.x * matrix._11 + corner.y * matrix._21 + matrix._31,
                corner.x * matrix._12 + corner.y * matrix._22 + matrix._32);
            minX = std::min(minX, point.x);
            minY = std::min(minY, point.y);
            maxX = std::max(maxX, point.x);
            maxY = std::max(maxY, point.y);
        }
        const RECT candidate = {
            static_cast<LONG>(floorf(minX - padding)),
            static_cast<LONG>(floorf(minY - padding)),
            static_cast<LONG>(ceilf(maxX + padding)),
            static_cast<LONG>(ceilf(maxY + padding)),
        };
        if (!hasBounds) {
            bounds = candidate;
            hasBounds = true;
        } else {
            bounds.left = std::min(bounds.left, candidate.left);
            bounds.top = std::min(bounds.top, candidate.top);
            bounds.right = std::max(bounds.right, candidate.right);
            bounds.bottom = std::max(bounds.bottom, candidate.bottom);
        }
    }
    if (hasBounds) {
        if (bounds.right <= bounds.left) bounds.right = bounds.left + 1;
        if (bounds.bottom <= bounds.top) bounds.bottom = bounds.top + 1;
    }
    return bounds;
}

static int SmoothOrnamentRope(
    const D2D1_POINT_2F* input,
    int count,
    D2D1_POINT_2F* output,
    int capacity) {
    if (count < 2 || capacity < 3) return 0;
    int written = 0;
    output[written++] = input[0];
    for (int i = 0; i + 1 < count && written + 2 < capacity; ++i) {
        const D2D1_POINT_2F& a = input[i];
        const D2D1_POINT_2F& b = input[i + 1];
        output[written++] = D2D1::Point2F(
            a.x * 0.75f + b.x * 0.25f,
            a.y * 0.75f + b.y * 0.25f);
        output[written++] = D2D1::Point2F(
            a.x * 0.25f + b.x * 0.75f,
            a.y * 0.25f + b.y * 0.75f);
    }
    output[written++] = input[count - 1];
    return written;
}

static void StrokeOrnamentRope(
    const D2D1_POINT_2F* points,
    int count,
    float width,
    float alpha,
    uint32_t rgb,
    float taper) {
    if (!g_ornamentBrush || count < 2) return;
    for (int i = 0; i + 1 < count; ++i) {
        const float t =
            (static_cast<float>(i) + 0.5f) / static_cast<float>(count - 1);
        const float segmentAlpha =
            std::clamp(alpha * (0.5f + 0.5f * t), 0.0f, 1.0f);
        g_ornamentBrush->SetColor(ToColorF(rgb, segmentAlpha));
        g_renderTarget->DrawLine(
            points[i],
            points[i + 1],
            g_ornamentBrush,
            width * (1.0f + taper * t),
            g_strokeStyle);
    }
}

static void DrawOrnamentRope() {
    if (!g_ornamentBrush || g_ornamentRopePointCount < 2) return;
    const float alpha =
        std::clamp(g_ornamentRopeOpacity * g_ornamentOpacity, 0.0f, 1.0f);
    if (alpha <= kOrnamentMinDrawOpacity) return;

    const D2D1_POINT_2F* points = g_ornamentRopePoints;
    int count = g_ornamentRopePointCount;
    const int first = SmoothOrnamentRope(
        points,
        count,
        g_ornamentRopeSmoothA,
        kOrnamentMaxRopeDrawPoints);
    if (first >= 2) {
        const int second = SmoothOrnamentRope(
            g_ornamentRopeSmoothA,
            first,
            g_ornamentRopeSmoothB,
            kOrnamentMaxRopeDrawPoints);
        if (second >= 2) {
            points = g_ornamentRopeSmoothB;
            count = second;
        }
    }

    const float width = std::max(1.0f, g_ornamentRopeWidthPx * g_currentDpiScale);
    switch (g_ornamentRopeStyle) {
        case OrnamentRopeStyle::Silk:
            StrokeOrnamentRope(
                points, count, width, alpha, g_currentCoreRGB, -0.4f);
            break;
        case OrnamentRopeStyle::Neon:
            StrokeOrnamentRope(
                points, count, width * 3.6f, alpha * 0.16f, g_currentCoreRGB, 0.0f);
            StrokeOrnamentRope(
                points, count, width * 2.0f, alpha * 0.28f, g_currentCoreRGB, 0.0f);
            StrokeOrnamentRope(
                points, count, width, alpha, g_currentCoreRGB, 0.0f);
            break;
        case OrnamentRopeStyle::Chain:
            StrokeOrnamentRope(
                points, count, width * 0.55f, alpha * 0.6f, g_currentCoreRGB, 0.0f);
            for (int i = 1; i < g_ornamentRopePointCount; ++i) {
                const float t = static_cast<float>(i)
                    / static_cast<float>(g_ornamentRopePointCount - 1);
                g_ornamentBrush->SetColor(
                    ToColorF(g_currentCoreRGB, alpha * (0.45f + 0.55f * t)));
                const float radius = width * 1.6f;
                g_renderTarget->FillEllipse(
                    D2D1::Ellipse(g_ornamentRopePoints[i], radius, radius),
                    g_ornamentBrush);
            }
            break;
        case OrnamentRopeStyle::Ribbon:
            StrokeOrnamentRope(
                points, count, width * 3.2f, alpha * 0.3f, g_currentOuterRGB, 0.0f);
            StrokeOrnamentRope(
                points, count, width * 1.1f, alpha, g_currentCoreRGB, 0.0f);
            break;
    }
}

static void DrawOrnament(const RECT& renderBounds) {
    if (!g_ornamentAssetsReady || g_ornamentNodes.empty()) return;
    const D2D1_MATRIX_3X2_F baseTransform = D2D1::Matrix3x2F::Translation(
        -static_cast<float>(renderBounds.left),
        -static_cast<float>(renderBounds.top));
    g_renderTarget->SetTransform(baseTransform);
    DrawOrnamentRope();
    for (const OrnamentNode& node : g_ornamentNodes) {
        const float alpha = std::clamp(node.opacity * g_ornamentOpacity, 0.0f, 1.0f);
        if (alpha <= kOrnamentMinDrawOpacity) continue;
        g_renderTarget->SetTransform(OrnamentNodeMatrix(node) * baseTransform);
        const OrnamentImage* image = node.imageIndex >= 0
            && node.imageIndex < static_cast<int>(g_ornamentImages.size())
            ? &g_ornamentImages[static_cast<size_t>(node.imageIndex)]
            : nullptr;
        if (image && image->bitmap) {
            g_renderTarget->DrawBitmap(
                image->bitmap,
                &node.localBounds,
                alpha,
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                nullptr);
        } else if (g_ornamentBrush) {
            const float width = node.localBounds.right - node.localBounds.left;
            const float height = node.localBounds.bottom - node.localBounds.top;
            if (width <= 0.0f || height <= 0.0f) continue;
            g_ornamentBrush->SetColor(ToColorF(g_currentCoreRGB, alpha * 0.72f));
            g_renderTarget->FillEllipse(
                D2D1::Ellipse(
                    D2D1::Point2F(
                        (node.localBounds.left + node.localBounds.right) * 0.5f,
                        (node.localBounds.top + node.localBounds.bottom) * 0.5f),
                    width * 0.5f,
                    height * 0.5f),
                g_ornamentBrush);
        }
    }
    g_renderTarget->SetTransform(baseTransform);
}

bool RenderOverlay(HWND hwnd, LONGLONG nowQpc) {
    UpdateOrnamentDrawStates();
    const bool ornamentVisible = OrnamentVisible();
    bool drawTrail = g_historyCount >= 2;
    const float speed = g_speedScaling ? (g_isSmearing ? g_smoothedSpeedNorm : g_frozenSpeedNorm) : 1.0f;
    const int renderLength = GetEffectiveTailLength(speed);
    const float outerHalf = (g_widthMin + (g_widthMax - g_widthMin) * speed) * g_currentDpiScale;
    const float coreHalf = (g_coreWidthMin + (g_coreWidthMax - g_coreWidthMin) * speed) * g_currentDpiScale;
    const float alphaScale = g_alphaMin + (g_alphaMax - g_alphaMin) * speed;
    const float trailAlpha = alphaScale * g_fadeAlpha;

    if (drawTrail) {
        SmoothTrail(g_smoothIterations, renderLength);
        drawTrail = g_renderCache.smoothed.size() >= 2 && trailAlpha >= 0.02f;
    }

    const int activeRippleCount = UpdateRippleState(nowQpc);
    const int activeMotionAccentCount = UpdateMotionAccentState(nowQpc);
    if (!drawTrail
        && activeRippleCount == 0
        && activeMotionAccentCount == 0
        && !ornamentVisible) {
        return false;
    }

    RECT bounds = {};
    bool hasBounds = false;
    const auto includeBounds = [&bounds, &hasBounds](const RECT& candidate) {
        if (!hasBounds) {
            bounds = candidate;
            hasBounds = true;
            return;
        }
        bounds.left = std::min(bounds.left, candidate.left);
        bounds.top = std::min(bounds.top, candidate.top);
        bounds.right = std::max(bounds.right, candidate.right);
        bounds.bottom = std::max(bounds.bottom, candidate.bottom);
    };

    if (drawTrail) {
        const float boundsHalf = g_glowEnabled
            ? outerHalf * std::max(1.0f, g_glowWidthFactor)
            : outerHalf;
        includeBounds(CalculateTrailBounds(boundsHalf));
    }

    // These values originate from integer settings, so no floating-point round-up is needed.
    const float scaledRippleRadius = g_rippleRadius * g_currentDpiScale;
    const float scaledRippleStrokeWidth = g_rippleStrokeWidth * g_currentDpiScale;
    const float scaledWheelArrowSize = g_wheelArrowSize * g_currentDpiScale;
    const float scaledWheelArrowDistance = g_wheelArrowDistance * g_currentDpiScale;
    const LONG ripplePadding = static_cast<LONG>(
        scaledRippleRadius + scaledRippleStrokeWidth + kRenderPadding * g_currentDpiScale + 1.0f);
    const LONG wheelArrowPadding = static_cast<LONG>(ceilf(
        scaledWheelArrowDistance * 0.5f
        + scaledWheelArrowSize * 0.65f
        + scaledRippleStrokeWidth
        + kRenderPadding * g_currentDpiScale
        + 2.0f));
    const MotionAccent* newestMotionAccent = nullptr;
    for (const MotionAccent& accent : g_motionAccents) {
        if (accent.active && (!newestMotionAccent || accent.startQpc > newestMotionAccent->startQpc)) {
            newestMotionAccent = &accent;
        }
    }
    const float scaledMotionAccentSize = g_motionAccentSize * g_currentDpiScale;
    const LONG motionAccentPadding = static_cast<LONG>(
        ceilf(scaledMotionAccentSize * 3.8f + kRenderPadding * g_currentDpiScale + 3.0f));
    for (const MotionAccent& accent : g_motionAccents) {
        if (!accent.active) continue;
        if (newestMotionAccent) {
            const int64_t deltaX = static_cast<int64_t>(accent.center.x) - newestMotionAccent->center.x;
            const int64_t deltaY = static_cast<int64_t>(accent.center.y) - newestMotionAccent->center.y;
            if (deltaX < -kMaxRippleClusterRadiusPx || deltaX > kMaxRippleClusterRadiusPx
                || deltaY < -kMaxRippleClusterRadiusPx || deltaY > kMaxRippleClusterRadiusPx) {
                continue;
            }
        }
        const RECT accentBounds = {
            accent.center.x - motionAccentPadding,
            accent.center.y - motionAccentPadding,
            accent.center.x + motionAccentPadding,
            accent.center.y + motionAccentPadding,
        };
        includeBounds(accentBounds);
    }

    const RippleEffect* newestRipple = nullptr;
    for (const RippleEffect& ripple : g_ripples) {
        if (ripple.active && (!newestRipple || ripple.startQpc > newestRipple->startQpc)) {
            newestRipple = &ripple;
        }
    }
    for (const RippleEffect& ripple : g_ripples) {
        if (!ripple.active) continue;
        // A single layered window must submit one rectangular bitmap. Avoid a
        // huge allocation if interactions occur on distant monitors in quick
        // succession; keep rendering the most recent local cluster instead.
        if (newestRipple) {
            const int64_t deltaX = static_cast<int64_t>(ripple.center.x) - newestRipple->center.x;
            const int64_t deltaY = static_cast<int64_t>(ripple.center.y) - newestRipple->center.y;
            if (deltaX < -kMaxRippleClusterRadiusPx || deltaX > kMaxRippleClusterRadiusPx
                || deltaY < -kMaxRippleClusterRadiusPx || deltaY > kMaxRippleClusterRadiusPx) {
                continue;
            }
        }
        const LONG effectPadding = ripple.kind == RippleKind::Click
            ? ripplePadding
            : wheelArrowPadding;
        const RECT rippleBounds = {
            ripple.center.x - effectPadding,
            ripple.center.y - effectPadding,
            ripple.center.x + effectPadding,
            ripple.center.y + effectPadding,
        };
        includeBounds(rippleBounds);
    }
    if (ornamentVisible) includeBounds(CalculateOrnamentBounds());
    if (!hasBounds) return false;

    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    HDC screen = GetScreenDC();
    if (!screen || !EnsureBackbuffer(width, height, screen) || !EnsureD2DResources()) return false;
    if (!g_dcBound) {
        const RECT bindRect = {0, 0, g_cachedWidth, g_cachedHeight};
        if (FAILED(g_renderTarget->BindDC(g_backBufferDc, &bindRect))) return false;
        g_dcBound = true;
    }

    if (ornamentVisible) EnsureOrnamentBitmaps();

    g_renderTarget->BeginDraw();
    g_renderTarget->SetTransform(D2D1::Matrix3x2F::Translation(
        -static_cast<float>(bounds.left), -static_cast<float>(bounds.top)));
    const D2D1_RECT_F clipBounds = D2D1::RectF(
        static_cast<float>(bounds.left),
        static_cast<float>(bounds.top),
        static_cast<float>(bounds.right),
        static_cast<float>(bounds.bottom));
    g_renderTarget->PushAxisAlignedClip(clipBounds, D2D1_ANTIALIAS_MODE_ALIASED);
    g_renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

    if (drawTrail) {
        const D2D1_LAYER_PARAMETERS trailLayerParameters = D2D1::LayerParameters(
            clipBounds,
            nullptr,
            D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
            D2D1::IdentityMatrix(),
            std::clamp(trailAlpha, 0.0f, 1.0f));
        g_renderTarget->PushLayer(&trailLayerParameters, g_trailOpacityLayer);

        if (g_glowEnabled) {
            const D2D1_LAYER_PARAMETERS glowLayerParameters = D2D1::LayerParameters(
                clipBounds,
                nullptr,
                D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                D2D1::IdentityMatrix(),
                std::clamp(g_glowAlpha, 0.0f, 1.0f));
            g_renderTarget->PushLayer(&glowLayerParameters, g_glowOpacityLayer);
            g_glowBrush->SetColor(ToColorF(g_glowRGB, 1.0f));
            DrawTrailStroke(
                g_renderTarget,
                g_glowBrush,
                outerHalf * std::max(1.0f, g_glowWidthFactor),
                true);
            g_renderTarget->PopLayer();
        }

        g_outerBrush->SetColor(ToColorF(g_currentOuterRGB, 1.0f));
        g_coreBrush->SetColor(ToColorF(g_currentCoreRGB, 1.0f));
        DrawTrailStroke(g_renderTarget, g_outerBrush, outerHalf, true);

        ID2D1Brush* coreBrush = g_coreBrush;
        if (g_gradientEnabled && EnsureGradientBrush()) {
            g_gradientBrush->SetStartPoint(g_renderCache.smoothed.front());
            g_gradientBrush->SetEndPoint(g_renderCache.smoothed.back());
            g_gradientBrush->SetOpacity(1.0f);
            coreBrush = g_gradientBrush;
        }
        DrawTrailStroke(g_renderTarget, coreBrush, coreHalf, true);
        g_renderTarget->PopLayer();
    }

    const LONGLONG accentDurationTicks = std::max<LONGLONG>(
        1, g_qpcFrequency * static_cast<LONGLONG>(g_motionAccentDurationMs) / 1000);
    for (const MotionAccent& accent : g_motionAccents) {
        if (!accent.active) continue;
        const LONGLONG deltaX = newestMotionAccent
            ? static_cast<LONGLONG>(accent.center.x) - newestMotionAccent->center.x
            : 0;
        const LONGLONG deltaY = newestMotionAccent
            ? static_cast<LONGLONG>(accent.center.y) - newestMotionAccent->center.y
            : 0;
        if (newestMotionAccent
            && (deltaX < -kMaxRippleClusterRadiusPx || deltaX > kMaxRippleClusterRadiusPx
                || deltaY < -kMaxRippleClusterRadiusPx || deltaY > kMaxRippleClusterRadiusPx)) {
            continue;
        }
        const LONGLONG elapsedTicks = nowQpc - accent.startQpc;
        if (elapsedTicks < 0 || elapsedTicks >= accentDurationTicks) continue;
        const float progress = std::clamp(
            static_cast<float>(elapsedTicks) / static_cast<float>(accentDurationTicks),
            0.0f,
            1.0f);
        const float remaining = 1.0f - progress;
        const float alpha = g_motionAccentOpacity * accent.intensity * remaining * remaining;
        if (alpha < 0.005f) continue;

        const float directionX = accent.directionX;
        const float directionY = accent.directionY;
        const float normalX = -directionY;
        const float normalY = directionX;
        const D2D1_POINT_2F center = D2D1::Point2F(
            static_cast<float>(accent.center.x),
            static_cast<float>(accent.center.y));
        const auto pointAlong = [&](float along, float normal) {
            return D2D1::Point2F(
                center.x + directionX * along + normalX * normal,
                center.y + directionY * along + normalY * normal);
        };
        const float size = scaledMotionAccentSize
            * (0.82f + 0.18f * remaining)
            * std::clamp(accent.intensity, 0.5f, 1.8f);

        // Each cue uses a heavy contrast silhouette with a saturated color core.
        // This keeps the shape readable without sampling screen pixels every frame.
        const float stroke = std::max(1.5f, g_currentDpiScale * (2.2f - 0.6f * progress));
        const float outlineExtra = std::max(1.5f, g_currentDpiScale * 1.5f);
        const float visibleAlpha = std::clamp(alpha, 0.0f, 1.0f);
        const float accentLuminance =
            0.2126f * static_cast<float>((g_motionAccentRGB >> 16) & 0xFF)
            + 0.7152f * static_cast<float>((g_motionAccentRGB >> 8) & 0xFF)
            + 0.0722f * static_cast<float>(g_motionAccentRGB & 0xFF);
        const uint32_t outlineRGB = accentLuminance >= 128.0f ? 0x00000000 : 0x00FFFFFF;
        const auto drawAccentLine = [&](const D2D1_POINT_2F& start,
                                        const D2D1_POINT_2F& end,
                                        float lineWidth) {
            g_rippleBrush->SetColor(ToColorF(outlineRGB, visibleAlpha));
            g_renderTarget->DrawLine(
                start, end, g_rippleBrush, lineWidth + outlineExtra, g_strokeStyle);
            g_rippleBrush->SetColor(ToColorF(g_motionAccentRGB, visibleAlpha));
            g_renderTarget->DrawLine(
                start, end, g_rippleBrush, lineWidth, g_strokeStyle);
        };

        if (accent.kind == MotionAccentKind::Acceleration) {
            // The forward arrow communicates travel direction; the three
            // trailing lines expand backward so acceleration reads as a burst.
            const float tailExpansion = size * (0.16f + 0.58f * progress);
            for (int streak = -1; streak <= 1; ++streak) {
                const float lateral = static_cast<float>(streak)
                    * size * (0.34f + 0.05f * progress);
                const float rearOffset = static_cast<float>(streak == 0 ? 0 : 1);
                const float startAlong = -size * (0.10f + 0.12f * rearOffset)
                    - size * 0.12f * progress;
                const float endAlong = -size * (0.78f + 0.24f * rearOffset) - tailExpansion;
                const float lineWidth = stroke * (streak == 0 ? 1.2f : 0.88f);
                drawAccentLine(
                    pointAlong(startAlong, lateral),
                    pointAlong(endAlong, lateral),
                    lineWidth);
            }

            const D2D1_POINT_2F arrowTip = pointAlong(size * 0.42f, 0.0f);
            const D2D1_POINT_2F arrowBase = pointAlong(size * 0.08f, 0.0f);
            const D2D1_POINT_2F arrowWingA = pointAlong(size * 0.06f, size * 0.30f);
            const D2D1_POINT_2F arrowWingB = pointAlong(size * 0.06f, -size * 0.30f);
            drawAccentLine(arrowBase, arrowTip, stroke * 1.12f);
            drawAccentLine(arrowTip, arrowWingA, stroke * 1.12f);
            drawAccentLine(arrowTip, arrowWingB, stroke * 1.12f);
        } else {
            // A turn is drawn as the actual change of path, not a generic
            // chevron: the incoming leg arrives at the pivot and the outgoing
            // leg terminates in an arrowhead pointing along the new direction.
            const float previousLength = sqrtf(
                accent.previousDirectionX * accent.previousDirectionX
                + accent.previousDirectionY * accent.previousDirectionY);
            const float incomingExtent = size * (0.34f + 0.36f * progress);
            const float outgoingExtent = size * (0.30f + 0.38f * progress);
            if (previousLength > 0.5f) {
                const D2D1_POINT_2F incomingStart = D2D1::Point2F(
                    center.x - accent.previousDirectionX / previousLength * incomingExtent,
                    center.y - accent.previousDirectionY / previousLength * incomingExtent);
                drawAccentLine(incomingStart, center, stroke * 1.18f);
            } else {
                drawAccentLine(pointAlong(-incomingExtent, 0.0f), center, stroke * 1.18f);
            }

            const D2D1_POINT_2F arrowTip = pointAlong(outgoingExtent, 0.0f);
            const D2D1_POINT_2F arrowBase = pointAlong(outgoingExtent - size * 0.28f, 0.0f);
            const D2D1_POINT_2F arrowWingA = pointAlong(
                outgoingExtent - size * 0.30f, size * 0.22f);
            const D2D1_POINT_2F arrowWingB = pointAlong(
                outgoingExtent - size * 0.30f, -size * 0.22f);
            drawAccentLine(center, arrowBase, stroke * 1.18f);
            drawAccentLine(arrowBase, arrowTip, stroke * 1.18f);
            drawAccentLine(arrowTip, arrowWingA, stroke * 1.18f);
            drawAccentLine(arrowTip, arrowWingB, stroke * 1.18f);
        }
    }

    const LONGLONG durationTicks = std::max<LONGLONG>(
        1, g_qpcFrequency * static_cast<LONGLONG>(g_rippleDurationMs) / 1000);
    for (const RippleEffect& ripple : g_ripples) {
        if (!ripple.active) continue;
        const LONGLONG elapsedTicks = nowQpc - ripple.startQpc;
        if (elapsedTicks < 0 || elapsedTicks >= durationTicks) continue;
        const float progress = std::clamp(
            static_cast<float>(elapsedTicks) / static_cast<float>(durationTicks),
            0.0f,
            1.0f);
        const float easedProgress = progress * (2.0f - progress);
        const float radius = (kRippleInitialRadius
            + (g_rippleRadius - kRippleInitialRadius) * easedProgress) * g_currentDpiScale;
        const float remaining = 1.0f - progress;
        const float alpha = g_rippleOpacity * remaining * remaining;
        if (alpha < 0.005f) continue;

        if (ripple.kind != RippleKind::Click) {
            const float directionX = ripple.kind == RippleKind::WheelLeft
                ? -1.0f
                : ripple.kind == RippleKind::WheelRight ? 1.0f : 0.0f;
            const float directionY = ripple.kind == RippleKind::WheelUp
                ? -1.0f
                : ripple.kind == RippleKind::WheelDown ? 1.0f : 0.0f;
            const float arrowCenterX = static_cast<float>(ripple.center.x)
                + directionX * scaledWheelArrowDistance * (easedProgress - 0.5f);
            const float arrowCenterY = static_cast<float>(ripple.center.y)
                + directionY * scaledWheelArrowDistance * (easedProgress - 0.5f);
            const float arrowSize = scaledWheelArrowSize * (0.88f + 0.12f * remaining);
            const D2D1_POINT_2F arrowTip = D2D1::Point2F(
                arrowCenterX + directionX * arrowSize * 0.5f,
                arrowCenterY + directionY * arrowSize * 0.5f);
            const float perpendicularX = -directionY;
            const float perpendicularY = directionX;
            const D2D1_POINT_2F arrowBase = D2D1::Point2F(
                arrowTip.x - directionX * arrowSize * 0.29f,
                arrowTip.y - directionY * arrowSize * 0.29f);
            const D2D1_POINT_2F arrowLeft = D2D1::Point2F(
                arrowBase.x - perpendicularX * arrowSize * 0.31f,
                arrowBase.y - perpendicularY * arrowSize * 0.31f);
            const D2D1_POINT_2F arrowRight = D2D1::Point2F(
                arrowBase.x + perpendicularX * arrowSize * 0.31f,
                arrowBase.y + perpendicularY * arrowSize * 0.31f);
            const D2D1_POINT_2F shaftStart = D2D1::Point2F(
                arrowCenterX - directionX * arrowSize * 0.42f,
                arrowCenterY - directionY * arrowSize * 0.42f);
            const D2D1_POINT_2F shaftEnd = D2D1::Point2F(
                arrowCenterX + directionX * arrowSize * 0.20f,
                arrowCenterY + directionY * arrowSize * 0.20f);

            g_rippleBrush->SetColor(ToColorF(ripple.color, alpha));
            g_renderTarget->DrawLine(
                shaftStart, shaftEnd, g_rippleBrush, scaledRippleStrokeWidth, g_strokeStyle);
            g_renderTarget->DrawLine(
                arrowTip, arrowLeft, g_rippleBrush, scaledRippleStrokeWidth, g_strokeStyle);
            g_renderTarget->DrawLine(
                arrowTip, arrowRight, g_rippleBrush, scaledRippleStrokeWidth, g_strokeStyle);
            continue;
        }

        g_rippleBrush->SetColor(ToColorF(ripple.color, alpha));
        g_renderTarget->DrawEllipse(
            D2D1::Ellipse(
                D2D1::Point2F(
                    static_cast<float>(ripple.center.x),
                    static_cast<float>(ripple.center.y)),
                radius,
                radius),
            g_rippleBrush,
            scaledRippleStrokeWidth,
            g_strokeStyle);
        g_rippleBrush->SetColor(ToColorF(ripple.color, alpha * 0.58f));
        const float innerRadius = radius * 0.58f;
        g_renderTarget->DrawEllipse(
            D2D1::Ellipse(
                D2D1::Point2F(
                    static_cast<float>(ripple.center.x),
                    static_cast<float>(ripple.center.y)),
                innerRadius,
                innerRadius),
            g_rippleBrush,
            std::max(g_currentDpiScale * 1.5f, scaledRippleStrokeWidth * 0.78f),
            g_strokeStyle);
    }

    if (ornamentVisible) DrawOrnament(bounds);

    g_renderTarget->PopAxisAlignedClip();
    g_renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    const HRESULT drawResult = g_renderTarget->EndDraw();
    if (drawResult == D2DERR_RECREATE_TARGET) {
        ReleaseRenderResources();
        return false;
    }
    if (FAILED(drawResult)) return false;

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    POINT position = {bounds.left, bounds.top};
    SIZE size = {width, height};
    POINT bufferOrigin = {0, 0};
    return UpdateLayeredWindow(
        hwnd,
        screen,
        &position,
        &size,
        g_backBufferDc,
        &bufferOrigin,
        0,
        &blend,
        ULW_ALPHA) != FALSE;
}

// Pointer speed in pixels per second, derived from the real elapsed time between
// samples. The worker is event-driven while idle and samples at up to 125 Hz while
// effects are active, so a pixels-per-sample threshold would vary with the cadence.
static float PointerSpeedPxPerSecond(const POINT& point, LONGLONG nowQpc) {
    const LONGLONG elapsedTicks = nowQpc - g_lastSampleQpc;
    g_lastSampleQpc = nowQpc;
    const float dx = static_cast<float>(point.x - g_lastPos.x);
    const float dy = static_cast<float>(point.y - g_lastPos.y);
    g_lastPos = point;
    const float dtSeconds = (g_qpcFrequency > 0 && elapsedTicks > 0)
        ? static_cast<float>(elapsedTicks) / static_cast<float>(g_qpcFrequency)
        : 0.0f;
    return sqrtf(dx * dx + dy * dy) / std::max(dtSeconds, 0.001f);
}

bool SmearFrame(HWND hwnd, DWORD now, bool renderFrame) {
    POINT point = {};
    if (!GetCursorPos(&point)) return false;
    const LONGLONG sampleQpc = QpcNow();
    if (UpdateCursorMonitorDpi(point)) {
        ResetTrailState();
        ResetMotionCueTracking();
        g_ornamentPhysicsInitialized = false;
        g_lastPos = point;
        g_lastSampleQpc = sampleQpc;
    }
    const HWND foreground = GetForegroundWindow();
    const int appRule = CheckAppRuleCached(foreground);
    // While the trail is suppressed the pointer still moves. Keep the previous
    // position and sample timestamp current so a later resume cannot interpret
    // the suppressed movement as one unusually fast cursor jump.
    const auto parkSample = [&] {
        g_lastPos = point;
        g_lastSampleQpc = sampleQpc;
    };

    if (appRule < 0) {
        ResetTrailState();
        ResetMotionCueTracking();
        ClearRipples();
        SuppressOrnament(sampleQpc);
        parkSample();
        HideOverlay();
        MaybeReleaseIdleBackbuffer(now);
        return false;
    }

    const bool fullscreenActive = UpdateFullscreenState(now, foreground, point);
    if (fullscreenActive && appRule == 0) {
        ResetTrailState();
        ResetMotionCueTracking();
        ClearRipples();
        SuppressOrnament(sampleQpc);
        parkSample();
        HideOverlay();
        MaybeReleaseIdleBackbuffer(now);
        return false;
    }

    const LONGLONG sampleElapsedTicks = sampleQpc - g_lastSampleQpc;
    const LONGLONG resumeGapTicks = std::max<LONGLONG>(
        1, g_qpcFrequency * static_cast<LONGLONG>(kMotionResumeGapMs) / 1000);
    if (g_lastSampleQpc <= 0
        || sampleElapsedTicks <= 0
        || sampleElapsedTicks > resumeGapTicks) {
        // Initialize or rebase instead of inferring speed across an invalid sample interval.
        ResetTrailState();
        ResetMotionCueTracking();
        g_lastPos = point;
        g_lastSampleQpc = sampleQpc;
    }

    const bool wasSmearing = g_isSmearing;
    const int previousHistoryCount = g_historyCount;
    const POINT previousPoint = g_lastPos;
    const float velocity = PointerSpeedPxPerSecond(point, sampleQpc);
    UpdateMotionCueTracking(point, sampleQpc);
    UpdateTrailState(previousPoint, point, velocity);
    UpdateOrnamentPhysics(velocity, sampleQpc);
    MaybeReleaseIdleBackbuffer(now);
    if (g_isSmearing || g_historyCount >= 2) UpdateTrailColorIfNeeded(now);

    const bool stateChanged = wasSmearing != g_isSmearing
        || (previousHistoryCount >= 2) != (g_historyCount >= 2);
    const bool hasRipples = HasActiveRipples(sampleQpc);
    const bool hasMotionAccents = HasActiveMotionAccents(sampleQpc);
    const bool ornamentVisible = OrnamentVisible();
    if (g_historyCount < 2 && !hasRipples && !hasMotionAccents && !ornamentVisible) {
        HideOverlay();
        return false;
    }
    const bool ornamentRedrawDue = ornamentVisible
        && g_ornamentRedrawPending
        && sampleQpc >= g_ornamentNextRenderQpc;
    if (!renderFrame && !stateChanged && !ornamentRedrawDue) return false;
    if (!RenderOverlay(hwnd, sampleQpc)) {
        HideOverlay();
        return false;
    }
    g_ornamentRedrawPending = false;
    if (g_qpcFrequency > 0) {
        g_ornamentNextRenderQpc = sampleQpc
            + std::max<LONGLONG>(1, g_qpcFrequency / kActiveRenderFrameRate);
    }
    ShowOverlay();
    return true;
}

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case kSettingsChangedMessage:
        g_settingsDirty.store(true, std::memory_order_release);
        return 0;
    case WM_INPUT: {
        RAWINPUT rawInput = {};
        UINT rawInputSize = sizeof(rawInput);
        const UINT readResult = GetRawInputData(
            reinterpret_cast<HRAWINPUT>(lParam),
            RID_INPUT,
            &rawInput,
            &rawInputSize,
            sizeof(RAWINPUTHEADER));
        if (readResult != static_cast<UINT>(-1)
            && rawInput.header.dwType == RIM_TYPEMOUSE) {
            const RAWMOUSE& mouse = rawInput.data.mouse;
            const USHORT buttons = mouse.usButtonFlags;
            const USHORT clickDownMask = RI_MOUSE_LEFT_BUTTON_DOWN
                | RI_MOUSE_RIGHT_BUTTON_DOWN
                | RI_MOUSE_MIDDLE_BUTTON_DOWN;
            const bool clickDown = (buttons & clickDownMask) != 0;
            const bool wheelDown = (buttons & (RI_MOUSE_WHEEL | RI_MOUSE_HWHEEL)) != 0;
            const bool clickEvent = g_clickFeedbackEnabled && clickDown;
            const bool wheelEvent = g_wheelFeedbackEnabled && wheelDown;
            if (clickEvent || wheelEvent) {
                POINT interactionPoint = {};
                if (GetCursorPos(&interactionPoint)) {
                    if (g_clickFeedbackEnabled) {
                        if (buttons & RI_MOUSE_LEFT_BUTTON_DOWN) {
                            AddRipple(interactionPoint, g_leftClickRGB);
                        }
                        if (buttons & RI_MOUSE_RIGHT_BUTTON_DOWN) {
                            AddRipple(interactionPoint, g_rightClickRGB);
                        }
                        if (buttons & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
                            AddRipple(interactionPoint, g_middleClickRGB);
                        }
                    }
                    if (wheelEvent) {
                        const SHORT wheelDelta = static_cast<SHORT>(mouse.usButtonData);
                        if (wheelDelta != 0) {
                            const bool horizontalWheel = (buttons & RI_MOUSE_HWHEEL) != 0;
                            const bool shiftPressed = !horizontalWheel
                                && (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                            const RippleKind kind = horizontalWheel
                                ? (wheelDelta > 0
                                    ? RippleKind::WheelRight
                                    : RippleKind::WheelLeft)
                                : shiftPressed
                                    ? (wheelDelta > 0
                                        ? RippleKind::WheelLeft
                                        : RippleKind::WheelRight)
                                    : (wheelDelta > 0
                                        ? RippleKind::WheelUp
                                        : RippleKind::WheelDown);
                            AddRipple(
                                interactionPoint,
                                wheelDelta > 0 ? g_wheelUpRGB : g_wheelDownRGB,
                                kind);
                        }
                    }
                }
            }

            // Pendant reactions do not depend on the ripple feedback settings.
            if (g_ornamentEnabled && (clickDown || wheelDown)) {
                const LONGLONG reactionQpc = QpcNow();
                if (clickDown) {
                    QueueOrnamentImpulse(-g_ornamentClickImpulse, reactionQpc);
                }
                if ((buttons & RI_MOUSE_WHEEL) != 0) {
                    const SHORT wheelDelta = static_cast<SHORT>(mouse.usButtonData);
                    if (wheelDelta != 0) {
                        QueueOrnamentImpulse(
                            wheelDelta > 0
                                ? -g_ornamentWheelImpulse
                                : g_ornamentWheelImpulse,
                            reactionQpc);
                    }
                }
            }

            const bool moved = mouse.lLastX != 0
                || mouse.lLastY != 0
                || (mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0;
            if (moved) {
                g_rawMouseMovementPending = true;
                const LONGLONG currentQpc = QpcNow();
                const LONGLONG wakeInterval = std::max<LONGLONG>(
                    1, g_qpcFrequency * kInputWakeThrottleMs / 1000);
                if (!g_rawInputWakePending
                    && (g_lastRawInputWakeQpc == 0
                        || currentQpc - g_lastRawInputWakeQpc >= wakeInterval)) {
                    g_lastRawInputWakeQpc = currentQpc;
                    g_rawInputWakePending = true;
                    if (g_inputWakeEvent) SetEvent(g_inputWakeEvent);
                }
            }
        }
        // Windows requires DefWindowProc for foreground RIM_INPUT cleanup.
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    case WM_DISPLAYCHANGE:
        ReleaseBackbuffer();
        ReleaseBackgroundSampler();
        ReleaseScreenDC();
        g_lastFullscreenStrongCheck = 0;
        g_nextSampleDeadline = 0;
        g_cursorMonitor = nullptr;
        g_cursorDpi = 96;
        g_currentDpiScale = 1.0f;
        g_ornamentPhysicsInitialized = false;
        ResetAutoColorSamplingCache();
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}


static bool ArmFrameTimer(HANDLE timer, LONGLONG deadline, LONGLONG frequency) {
    const LONGLONG now = QpcNow();
    LONGLONG ticks = deadline - now;
    if (ticks < 1) ticks = 1;
    const LONGLONG due100ns = std::max<LONGLONG>(1, ticks * 10000000LL / frequency);
    LARGE_INTEGER due = {};
    due.QuadPart = -due100ns;
    return SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) != FALSE;
}

static LONGLONG FrameIntervalTicks(LONGLONG frequency, int frameRate) {
    return std::max<LONGLONG>(1, frequency / frameRate);
}

DWORD WINAPI OverlayThreadProc(LPVOID) {
    const HRESULT coResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(coResult)) return 0;
    LARGE_INTEGER qpcFrequency = {};
    if (!QueryPerformanceFrequency(&qpcFrequency) || qpcFrequency.QuadPart <= 0) {
        CoUninitialize();
        return 0;
    }
    g_qpcFrequency = qpcFrequency.QuadPart;
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2dFactory);
    if (FAILED(hr) || !g_d2dFactory) {
        CoUninitialize();
        return 0;
    }

    HINSTANCE instance = GetModuleHandle(nullptr);
    const wchar_t className[] = L"CursorTailOverlayWindowClass";
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = OverlayWndProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    const ATOM atom = RegisterClassW(&windowClass);
    if (!atom) {
        g_d2dFactory->Release();
        g_d2dFactory = nullptr;
        CoUninitialize();
        return 0;
    }

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        className,
        L"CursorTailOverlay",
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (!hwnd) {
        UnregisterClassW(className, instance);
        g_d2dFactory->Release();
        g_d2dFactory = nullptr;
        CoUninitialize();
        return 0;
    }

    g_overlayHwnd.store(hwnd, std::memory_order_release);
    ApplyOverlayCaptureAffinity();
    HideOverlay();
    POINT initialCursorPosition = {};
    if (GetCursorPos(&initialCursorPosition)) {
        UpdateCursorMonitorDpi(initialCursorPosition);
        g_lastPos = initialCursorPosition;
    }
    g_lastSampleQpc = QpcNow();
    g_fullscreenForegroundWindow = nullptr;
    g_lastFullscreenStrongCheck = 0;
    g_fullscreenStrongSignal = false;
    g_fullscreenSuppressed = false;
    g_fullscreenFalseSince = 0;
    if (g_settingsDirty.exchange(false, std::memory_order_acq_rel)) LoadSettings();

    HANDLE frameTimer = CreateWaitableTimerExW(
        nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!frameTimer) frameTimer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    if (!frameTimer) {
        DestroyWindow(hwnd);
        g_overlayHwnd.store(nullptr, std::memory_order_release);
        UnregisterClassW(className, instance);
        g_d2dFactory->Release();
        g_d2dFactory = nullptr;
        CoUninitialize();
        return 0;
    }

    g_rawInputRegistered = RegisterMouseRawInput(hwnd, true);
    g_foregroundEventHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        nullptr,
        ForegroundChanged,
        0,
        0,
        WINEVENT_OUTOFCONTEXT);
    if (!g_foregroundEventHook) {
        Wh_Log(L"SetWinEventHook(EVENT_SYSTEM_FOREGROUND) failed: %lu", GetLastError());
    }
    RefreshForegroundInputState();
    EnsureOrnamentAssetsLoaded();
    SmearFrame(hwnd, GetTickCount(), true);

    const LONGLONG simulationTicks =
        FrameIntervalTicks(qpcFrequency.QuadPart, kTargetFrameRate);
    const LONGLONG activeRenderTicks =
        FrameIntervalTicks(qpcFrequency.QuadPart, kActiveRenderFrameRate);
    const LONGLONG fadeRenderTicks =
        FrameIntervalTicks(qpcFrequency.QuadPart, kFadeRenderFrameRate);
    LONGLONG nextRenderDeadline = QpcNow();
    MSG message = {};
    bool running = true;

    while (running) {
        if (g_settingsDirty.exchange(false, std::memory_order_acq_rel)) {
            LoadSettings();
            EnsureOrnamentAssetsLoaded();
            SmearFrame(hwnd, GetTickCount(), true);
        }

        const LONGLONG currentQpc = QpcNow();
        const bool trailActive = g_isSmearing || g_historyCount >= 2;
        const bool rippleActive = HasActiveRipples(currentQpc) || HasActiveMotionAccents(currentQpc);
        const bool ornamentActive = OrnamentActive();
        const LONGLONG inputWakeInterval = std::max<LONGLONG>(
            1,
            g_qpcFrequency * static_cast<LONGLONG>(kInputWakeThrottleMs) / 1000);
        const bool inputCatchUpNeeded =
            g_rawMouseMovementPending && !g_rawInputWakePending && !ornamentActive;
        const bool sampleTimerNeeded =
            trailActive || rippleActive || ornamentActive || g_pollingFallback || inputCatchUpNeeded;
        bool trimTimerNeeded = false;

        LONGLONG timerDeadline = 0;
        if (sampleTimerNeeded) {
            if (inputCatchUpNeeded && !trailActive && !rippleActive && !ornamentActive && !g_pollingFallback) {
                // If the input wake was coalesced by the throttle, schedule one
                // bounded wake rather than waiting indefinitely and losing the
                // final sub-interval of a short movement burst.
                g_nextSampleDeadline = std::max(
                    currentQpc + 1,
                    g_lastRawInputWakeQpc + inputWakeInterval);
            } else if (g_nextSampleDeadline <= currentQpc) {
                const LONGLONG interval = (trailActive || rippleActive || ornamentActive)
                    ? simulationTicks
                    : GetFallbackPollIntervalTicks();
                g_nextSampleDeadline = currentQpc + interval;
            }
            timerDeadline = g_nextSampleDeadline;
        } else if (g_backBufferDc && g_backBufferBitmap) {
            MaybeReleaseIdleBackbuffer(GetTickCount());
            if (g_backBufferDc && g_backBufferBitmap) {
                const DWORD nowTicks = GetTickCount();
                const DWORD elapsed = nowTicks - g_backbufferIdleSince;
                const DWORD remaining = elapsed >= kBackbufferIdleReleaseDelayMs
                    ? 1
                    : kBackbufferIdleReleaseDelayMs - elapsed;
                timerDeadline = QpcNow()
                    + std::max<LONGLONG>(
                        1,
                        g_qpcFrequency * static_cast<LONGLONG>(remaining) / 1000);
                trimTimerNeeded = true;
            }
        }

        const bool timerNeeded = sampleTimerNeeded || trimTimerNeeded;
        if (timerNeeded) {
            if (!ArmFrameTimer(frameTimer, timerDeadline, qpcFrequency.QuadPart)) break;
        } else {
            CancelWaitableTimer(frameTimer);
        }

        HANDLE handles[] = {g_stopEvent, g_inputWakeEvent, frameTimer};
        const DWORD handleCount = timerNeeded ? 3 : 2;
        const DWORD waitResult = MsgWaitForMultipleObjectsEx(
            handleCount,
            handles,
            INFINITE,
            QS_ALLINPUT,
            MWMO_INPUTAVAILABLE);

        if (waitResult == WAIT_OBJECT_0) {
            running = false;
            continue;
        }

        if (waitResult == WAIT_OBJECT_0 + 1) {
            g_rawInputWakePending = false;
            const bool foregroundChanged = g_foregroundChangePending;
            const bool interactionTriggered = g_interactionPending;
            const bool rawMovement = g_rawMouseMovementPending;
            g_interactionPending = false;

            if (foregroundChanged) {
                RefreshForegroundInputState();
                // Recheck the new foreground immediately. UpdateFullscreenState
                // retains normal same-monitor exit hysteresis but releases
                // suppression for the desktop shell or a different cursor monitor.
                g_fullscreenForegroundWindow = nullptr;
                g_lastFullscreenStrongCheck = 0;
            }

            if (foregroundChanged || interactionTriggered) {
                g_rawMouseMovementPending = false;
                const LONGLONG eventQpc = QpcNow();
                const bool forceRender = interactionTriggered;
                const bool rendered = SmearFrame(hwnd, GetTickCount(), forceRender);
                const bool activeAfterEvent =
                    g_isSmearing || g_historyCount >= 2
                    || HasActiveRipples(QpcNow())
                    || HasActiveMotionAccents(QpcNow())
                    || OrnamentActive();
                if (activeAfterEvent) {
                    const bool fastActive = g_isSmearing || g_historyCount >= 2 || OrnamentActive();
                    nextRenderDeadline = eventQpc
                        + (fastActive
                            ? activeRenderTicks
                            : fadeRenderTicks);
                    g_nextSampleDeadline = eventQpc + simulationTicks;
                } else {
                    g_nextSampleDeadline = g_pollingFallback
                        ? eventQpc + GetFallbackPollIntervalTicks()
                        : 0;
                }
                (void)rendered;
            } else if (rawMovement) {
                g_rawMouseMovementPending = false;
                const LONGLONG movementQpc = QpcNow();
                const bool activeBeforeMovement =
                    g_isSmearing || g_historyCount >= 2
                    || HasActiveRipples(movementQpc)
                    || HasActiveMotionAccents(movementQpc);

                if (!activeBeforeMovement && !g_pollingFallback) {
                    // Bound the first post-idle interval instead of dividing by
                    // the entire idle duration. This retains the first movement
                    // burst even when no second hardware report follows it.
                    const LONGLONG initialSampleTicks = std::max<LONGLONG>(
                        1,
                        g_qpcFrequency * static_cast<LONGLONG>(kInputWakeThrottleMs) / 2000);
                    g_lastSampleQpc = movementQpc - initialSampleTicks;
                    const bool renderFrame = movementQpc >= nextRenderDeadline;
                    SmearFrame(hwnd, GetTickCount(), renderFrame);
                    const LONGLONG sampledQpc = QpcNow();
                    const bool activeAfterMovement =
                        g_isSmearing || g_historyCount >= 2
                        || HasActiveRipples(sampledQpc)
                        || HasActiveMotionAccents(sampledQpc)
                        || OrnamentActive();
                    g_nextSampleDeadline = sampledQpc
                        + (activeAfterMovement ? simulationTicks : 0);
                    if (activeAfterMovement && (renderFrame || sampledQpc >= nextRenderDeadline)) {
                        nextRenderDeadline = sampledQpc
                            + ((g_isSmearing || g_historyCount >= 2 || OrnamentActive())
                                ? activeRenderTicks
                                : fadeRenderTicks);
                    }
                } else if (movementQpc >= g_nextSampleDeadline || !activeBeforeMovement) {
                    const bool renderFrame = movementQpc >= nextRenderDeadline;
                    SmearFrame(hwnd, GetTickCount(), renderFrame);
                    const LONGLONG sampledQpc = QpcNow();
                    const bool activeAfterMovement =
                        g_isSmearing || g_historyCount >= 2
                        || HasActiveRipples(sampledQpc)
                        || HasActiveMotionAccents(sampledQpc)
                        || OrnamentActive();
                    g_nextSampleDeadline = sampledQpc
                        + (activeAfterMovement
                            ? simulationTicks
                            : (g_pollingFallback ? GetFallbackPollIntervalTicks() : 0));
                    if (activeAfterMovement && (renderFrame || sampledQpc >= nextRenderDeadline)) {
                        nextRenderDeadline = sampledQpc
                            + ((g_isSmearing || g_historyCount >= 2 || OrnamentActive())
                                ? activeRenderTicks
                                : fadeRenderTicks);
                    }
                }
            }
            continue;
        }

        if (timerNeeded && waitResult == WAIT_OBJECT_0 + 2) {
            if (sampleTimerNeeded) {
                const LONGLONG sampleNow = QpcNow();
                const bool interactionActiveBeforeFrame =
                    HasActiveRipples(sampleNow)
                    || HasActiveMotionAccents(sampleNow)
                    || OrnamentActive();
                const bool renderFrame =
                    (g_isSmearing || g_historyCount >= 2 || interactionActiveBeforeFrame || OrnamentActive())
                    && sampleNow >= nextRenderDeadline;
                const bool rendered = SmearFrame(hwnd, GetTickCount(), renderFrame);
                g_rawMouseMovementPending = false;

                const LONGLONG sampledQpc = QpcNow();
                const bool interactionActive =
                    HasActiveRipples(sampledQpc)
                    || HasActiveMotionAccents(sampledQpc)
                    || OrnamentActive();
                const bool active = g_isSmearing || OrnamentActive();
                const bool fading = !active && g_historyCount >= 2;
                if (active || fading || interactionActive) {
                    const LONGLONG interval = (active || interactionActive)
                        ? activeRenderTicks
                        : fadeRenderTicks;
                    if (rendered || sampledQpc >= nextRenderDeadline) {
                        nextRenderDeadline = sampledQpc + interval;
                    }
                    g_nextSampleDeadline = sampledQpc + simulationTicks;
                } else if (g_pollingFallback) {
                    g_nextSampleDeadline = sampledQpc + GetFallbackPollIntervalTicks();
                } else {
                    g_nextSampleDeadline = 0;
                }
            } else if (trimTimerNeeded) {
                MaybeReleaseIdleBackbuffer(GetTickCount());
                g_nextSampleDeadline = 0;
            }
            continue;
        }

        if (waitResult == WAIT_OBJECT_0 + handleCount) {
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    running = false;
                    break;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            continue;
        }

        break;
    }

    CancelWaitableTimer(frameTimer);
    CloseHandle(frameTimer);
    if (g_foregroundEventHook) {
        if (!UnhookWinEvent(g_foregroundEventHook)) {
            Wh_Log(L"UnhookWinEvent failed: %lu", GetLastError());
        }
        g_foregroundEventHook = nullptr;
    }
    if (g_rawInputRegistered && hwnd) {
        RegisterMouseRawInput(hwnd, false);
        g_rawInputRegistered = false;
    }
    ClearRipples();
    HideOverlay();
    ReleaseOrnamentAssets();
    ReleaseBackbuffer();
    ReleaseBackgroundSampler();
    ReleaseScreenDC();
    if (g_d2dFactory) {
        g_d2dFactory->Release();
        g_d2dFactory = nullptr;
    }
    g_overlayHwnd.store(nullptr, std::memory_order_release);
    DestroyWindow(hwnd);
    UnregisterClassW(className, instance);
    CoUninitialize();
    return 0;
}

BOOL WhTool_ModInit() {
    LoadSettings();
    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) return FALSE;
    g_inputWakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_inputWakeEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
        return FALSE;
    }
    g_threadHandle = CreateThread(nullptr, 0, OverlayThreadProc, nullptr, 0, nullptr);
    if (!g_threadHandle) {
        CloseHandle(g_inputWakeEvent);
        g_inputWakeEvent = nullptr;
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
        return FALSE;
    }
    return TRUE;
}

void WhTool_ModUninit() {
    if (g_stopEvent) SetEvent(g_stopEvent);
    if (g_threadHandle) {
        WaitForSingleObject(g_threadHandle, INFINITE);
        CloseHandle(g_threadHandle);
        g_threadHandle = nullptr;
    }
    if (g_inputWakeEvent) {
        CloseHandle(g_inputWakeEvent);
        g_inputWakeEvent = nullptr;
    }
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }
}

void WhTool_ModSettingsChanged() {
    g_settingsDirty.store(true, std::memory_order_release);
    HWND hwnd = g_overlayHwnd.load(std::memory_order_acquire);
    if (hwnd && IsWindow(hwnd)) PostMessageW(hwnd, kSettingsChangedMessage, 0, 0);
}

////////////////////////////////////////////////////////////////////////////////
// Windhawk tool mod implementation for mods which don't need to inject to other
// processes or hook other functions. Context:
// https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process
//
// The mod will load and run in a dedicated windhawk.exe process.
//
// Paste the code below as part of the mod code, and use these callbacks:
// * WhTool_ModInit
// * WhTool_ModSettingsChanged
// * WhTool_ModUninit
//
// Currently, other callbacks are not supported.

bool g_isToolModProcessLauncher;
HANDLE g_toolModProcessMutex;

void WINAPI EntryPoint_Hook() {
    Wh_Log(L">");
    ExitThread(0);
}

BOOL Wh_ModInit() {
    DWORD sessionId;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) &&
        sessionId == 0) {
        return FALSE;
    }

    bool isExcluded = false;
    bool isToolModProcess = false;
    bool isCurrentToolModProcess = false;
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLine(), &argc);
    if (!argv) {
        Wh_Log(L"CommandLineToArgvW failed");
        return FALSE;
    }

    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"-service") == 0 ||
            wcscmp(argv[i], L"-service-start") == 0 ||
            wcscmp(argv[i], L"-service-stop") == 0) {
            isExcluded = true;
            break;
        }
    }

    for (int i = 1; i < argc - 1; i++) {
        if (wcscmp(argv[i], L"-tool-mod") == 0) {
            isToolModProcess = true;
            if (wcscmp(argv[i + 1], WH_MOD_ID) == 0) {
                isCurrentToolModProcess = true;
            }
            break;
        }
    }

    LocalFree(argv);

    if (isExcluded) {
        return FALSE;
    }

    if (isCurrentToolModProcess) {
        g_toolModProcessMutex =
            CreateMutex(nullptr, TRUE, L"windhawk-tool-mod_" WH_MOD_ID);
        if (!g_toolModProcessMutex) {
            Wh_Log(L"CreateMutex failed");
            ExitProcess(1);
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            Wh_Log(L"Tool mod already running (%s)", WH_MOD_ID);
            ExitProcess(1);
        }

        if (!WhTool_ModInit()) {
            ExitProcess(1);
        }

        IMAGE_DOS_HEADER* dosHeader =
            (IMAGE_DOS_HEADER*)GetModuleHandle(nullptr);
        IMAGE_NT_HEADERS* ntHeaders =
            (IMAGE_NT_HEADERS*)((BYTE*)dosHeader + dosHeader->e_lfanew);

        DWORD entryPointRVA = ntHeaders->OptionalHeader.AddressOfEntryPoint;
        void* entryPoint = (BYTE*)dosHeader + entryPointRVA;

        Wh_SetFunctionHook(entryPoint, (void*)EntryPoint_Hook, nullptr);
        return TRUE;
    }

    if (isToolModProcess) {
        return FALSE;
    }

    g_isToolModProcessLauncher = true;
    return TRUE;
}

void Wh_ModAfterInit() {
    if (!g_isToolModProcessLauncher) {
        return;
    }

    WCHAR currentProcessPath[MAX_PATH];
    switch (GetModuleFileName(nullptr, currentProcessPath,
                              ARRAYSIZE(currentProcessPath))) {
        case 0:
        case ARRAYSIZE(currentProcessPath):
            Wh_Log(L"GetModuleFileName failed");
            return;
    }

    WCHAR
    commandLine[MAX_PATH + 2 +
                (sizeof(L" -tool-mod \"" WH_MOD_ID "\"") / sizeof(WCHAR)) - 1];
    swprintf_s(commandLine, L"\"%s\" -tool-mod \"%s\"", currentProcessPath,
               WH_MOD_ID);

    HMODULE kernelModule = GetModuleHandle(L"kernelbase.dll");
    if (!kernelModule) {
        kernelModule = GetModuleHandle(L"kernel32.dll");
        if (!kernelModule) {
            Wh_Log(L"No kernelbase.dll/kernel32.dll");
            return;
        }
    }

    using CreateProcessInternalW_t = BOOL(WINAPI*)(
        HANDLE hUserToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
        LPSECURITY_ATTRIBUTES lpProcessAttributes,
        LPSECURITY_ATTRIBUTES lpThreadAttributes, WINBOOL bInheritHandles,
        DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
        LPSTARTUPINFOW lpStartupInfo,
        LPPROCESS_INFORMATION lpProcessInformation,
        PHANDLE hRestrictedUserToken);
    CreateProcessInternalW_t pCreateProcessInternalW =
        (CreateProcessInternalW_t)GetProcAddress(kernelModule,
                                                 "CreateProcessInternalW");
    if (!pCreateProcessInternalW) {
        Wh_Log(L"No CreateProcessInternalW");
        return;
    }

    STARTUPINFO si{
        .cb = sizeof(STARTUPINFO),
        .dwFlags = STARTF_FORCEOFFFEEDBACK,
    };
    PROCESS_INFORMATION pi;
    if (!pCreateProcessInternalW(nullptr, currentProcessPath, commandLine,
                                 nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS,
                                 nullptr, nullptr, &si, &pi, nullptr)) {
        Wh_Log(L"CreateProcess failed");
        return;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void Wh_ModSettingsChanged() {
    if (g_isToolModProcessLauncher) {
        return;
    }

    WhTool_ModSettingsChanged();
}

void Wh_ModUninit() {
    if (g_isToolModProcessLauncher) {
        return;
    }

    WhTool_ModUninit();
    ExitProcess(0);
}
