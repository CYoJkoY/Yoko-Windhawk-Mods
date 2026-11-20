// ==WindhawkMod==
// @id              aleph-canvas
// @name            Aleph - Infinite Window Canvas
// @name:zh-CN      Aleph - 无限窗口画布
// @description     Arrange open windows across a pannable canvas with live DWM previews.
// @description:zh-CN 在可平移画布上管理已打开的窗口，并显示 DWM 实时预览。
// @version         1.4.0
// @author          CYoJkoY
// @github          https://github.com/CYoJkoY
// @license         MIT
// @include         windhawk.exe
// @include         windhawk-mod.exe
// @compilerOptions -ladvapi32 -ld2d1 -ldwmapi -ldwrite -lgdi32 -lole32 -lshell32 -lshcore -luuid -lwindowscodecs
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Aleph - Infinite Window Canvas

Aleph presents eligible application windows on a navigable canvas. Live
windows use DWM thumbnails; minimized and other-desktop windows use icon cards.
Drag cards to arrange them without moving the real desktop windows.

## Controls

- `Alt+Z`: toggle the canvas (configurable). If the hotkey does
  nothing, another program may already use it; pick a different combination.
- Mouse wheel: zoom around the pointer.
- Alt+wheel over a stacked card: cycle the stack; plain wheel still zooms.
- Middle mouse drag: pan the canvas.
- Hold Space and drag empty canvas: pan.
- Drag a card: arrange it; nearby edges snap into alignment.
- Shift+click a stacked card: bring the next window of the same app to the front.
- Double-click a card or press Enter: focus the window; an other-desktop window is pulled onto this desktop first.
- Tab / Shift+Tab: select a card; arrow keys pan; Home fits all cards; Escape hides the canvas.
- Click or drag the minimap: navigate the canvas.
- The canvas hides when another window takes input focus.

## Notes

Card positions are saved in the Windhawk mod storage directory as
`layout-<user-SID>.json`.
Canvas coordinates use physical pixels relative to the virtual desktop origin.
Card badges and the empty-canvas hint follow the interface language setting.
`Auto` uses the system UI language and falls back to English when that language
is not one of the thirteen supported ones.
Icon cards ask the shell for the largest available icon (256 px when the jumbo
image list exists) and fall back to smaller lists, then to the window icon.
Arranging cards never moves real application windows. Activating an
other-desktop card asks the documented `IVirtualDesktopManager` interface to
move that window onto the desktop that displays the canvas, then focuses it. A
window that refuses the move is reported in the Windhawk log; foreground
activation then follows the normal rules for the current process.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- Behavior:
    - language: auto
      $name: Interface language
      $name:zh-CN: 界面语言
      $description: Language used by canvas interface text. Auto follows the system UI language and falls back to English when unsupported.
      $description:zh-CN: 画布界面文本使用的语言。自动模式跟随系统界面语言，不支持时回退为英文。
      $options:
        - auto: Auto
        - en: English
        - fr: Français
        - zh: 简体中文
        - zh_TW: 繁體中文
        - ja: 日本語
        - ko: 한국어
        - ru: Русский
        - pl: Polski
        - es: Español
        - pt: Português
        - de: Deutsch
        - tr: Türkçe
        - it: Italiano
      $options:zh-CN:
        - auto: 自动
        - en: English
        - fr: Français
        - zh: 简体中文
        - zh_TW: 繁體中文
        - ja: 日本語
        - ko: 한국어
        - ru: Русский
        - pl: Polski
        - es: Español
        - pt: Português
        - de: Deutsch
        - tr: Türkçe
        - it: Italiano
    - toggle_hotkey: Alt+Z
      $name: Toggle hotkey
      $name:zh-CN: 显示/隐藏快捷键
      $description: Global hotkey used to show or hide the canvas. Use modifiers such as Ctrl, Alt, Shift, or Win plus one key. Avoid AltGr combinations. If it does nothing, another program may already use the combination.
      $description:zh-CN: 用于显示或隐藏画布的全局快捷键。可组合 Ctrl、Alt、Shift 或 Win 与一个按键；避免使用 AltGr 组合。若快捷键无效，可能是其他程序已占用该组合。
    - open_on_start: false
      $name: Open on startup
      $name:zh-CN: 启动时打开
      $description: Open the canvas when its Windhawk tool process starts.
      $description:zh-CN: Windhawk 工具进程启动时打开画布。
    - animation_ms: 140
      $name: Dive-in animation duration
      $name:zh-CN: 潜入动画时长
      $description: Duration of the camera transition when activating a window, in milliseconds. Range 60-600.
      $description:zh-CN: 激活窗口时的摄像机过渡时长，单位毫秒。范围 60-600。
    - rescan_interval_ms: 400
      $name: Window scan interval
      $name:zh-CN: 窗口扫描间隔
      $description: How often the visible canvas checks for opened or closed windows, in milliseconds. Range 100-5000.
      $description:zh-CN: 画布显示时检查窗口打开或关闭的频率，单位毫秒。范围 100-5000。
  $name: Behavior
  $name:zh-CN: 行为

- Canvas:
    - show_grid: true
      $name: Show grid
      $name:zh-CN: 显示网格
      $description: Draw a coordinate grid behind window cards.
      $description:zh-CN: 在窗口卡片后方绘制坐标网格。
    - grid_spacing: 96
      $name: Grid spacing
      $name:zh-CN: 网格间距
      $description: World-space spacing between grid lines. Range 16-512.
      $description:zh-CN: 网格线之间的世界坐标间距。范围 16-512。
    - gap: 32
      $name: Card gap
      $name:zh-CN: 卡片间距
      $description: Minimum world-space gap used when packing new cards. Range 0-256.
      $description:zh-CN: 自动排列新卡片时使用的最小世界坐标间距。范围 0-256。
    - snap_distance: 8
      $name: Snap distance
      $name:zh-CN: 磁吸距离
      $description: Screen-pixel distance for aligning card edges and centers. Range 0-48.
      $description:zh-CN: 卡片边缘和中心轴触发对齐的屏幕像素距离。范围 0-48。
    - include_other_desktops: true
      $name: Include other virtual desktops
      $name:zh-CN: 包含其他虚拟桌面
      $description: Show eligible windows cloaked on another virtual desktop as icon cards.
      $description:zh-CN: 将其他虚拟桌面上符合条件的隐藏窗口显示为图标卡片。
    - save_layout: true
      $name: Save card positions
      $name:zh-CN: 保存卡片位置
      $description: Persist card positions between runs.
      $description:zh-CN: 在多次运行之间保存卡片位置。
    - initial_layout: packed
      $name: Initial layout
      $name:zh-CN: 初始布局
      $description: Place new cards in free space or at their real desktop positions.
      $description:zh-CN: 将新卡片放入空闲区域，或放在其实际桌面位置。
      $options:
        - packed: Packed
        - desktop: Desktop positions
      $options:zh-CN:
        - packed: 自动排列
        - desktop: 桌面位置
    - card_stacks_enabled: true
      $name: Group card stacks
      $name:zh-CN: 启用卡片堆叠
      $description: Group windows from the same application and fan older members behind the active card.
      $description:zh-CN: 将同一应用程序的窗口分为一组，并把较早的成员展开到当前卡片后方。
    - card_stack_step_degrees: 65
      $name: Stack angle step
      $name:zh-CN: 堆叠角度步进
      $description: Rotation added by each stack position, in tenths of a degree. Range 0-900.
      $description:zh-CN: 每个堆叠位置增加的旋转角度，单位为十分之一度。范围 0-900。
    - card_stack_max_degrees: 240
      $name: Stack maximum angle
      $name:zh-CN: 堆叠最大角度
      $description: Maximum rotation applied to a stack member, in tenths of a degree. Range 0-900.
      $description:zh-CN: 堆叠成员可用的最大旋转角度，单位为十分之一度。范围 0-900。
    - card_stack_scale_percent: 975
      $name: Stack scale factor
      $name:zh-CN: 堆叠缩放系数
      $description: Per-level scale multiplier, in thousandths. Range 800-1000.
      $description:zh-CN: 每一级堆叠使用的缩放系数，单位为千分之一。范围 800-1000。
    - card_stack_max_depth: 5
      $name: Stack depth
      $name:zh-CN: 堆叠深度
      $description: Maximum number of visible layers behind the active card. Range 0-8.
      $description:zh-CN: 当前卡片后方可见的最大层数。范围 0-8。
    - card_stack_rotation_direction: -1
      $name: Stack rotation direction
      $name:zh-CN: 堆叠旋转方向
      $description: Use 1 for clockwise, -1 for counter-clockwise, or 0 for no rotation.
      $description:zh-CN: 1 为顺时针，-1 为逆时针，0 为不旋转。
  $name: Canvas
  $name:zh-CN: 画布

- Appearance:
    - min_zoom_percent: 15
      $name: Minimum zoom
      $name:zh-CN: 最小缩放
      $description: Minimum canvas zoom percentage. Range 15-245.
      $description:zh-CN: 画布最小缩放百分比。范围 15-245。
    - max_zoom_percent: 250
      $name: Maximum zoom
      $name:zh-CN: 最大缩放
      $description: Maximum canvas zoom percentage. Range 20-250 and at least five points above the minimum.
      $description:zh-CN: 画布最大缩放百分比。范围 20-250，且至少比最小值高 5。
    - minimap_enabled: true
      $name: Show minimap
      $name:zh-CN: 显示小地图
      $description: Show the interactive overview map in the lower-right corner.
      $description:zh-CN: 在右下角显示可交互的概览地图。
    - minimap_size: 180
      $name: Minimap size
      $name:zh-CN: 小地图尺寸
      $description: Minimap width and height in display-scaled pixels. Range 96-420.
      $description:zh-CN: 小地图宽高，按显示缩放比例调整。范围 96-420。
    - background_color: 0B0E14
      $name: Background color
      $name:zh-CN: 背景颜色
      $description: Six-digit RGB hex color without the leading #.
      $description:zh-CN: 六位 RGB 十六进制颜色，不含开头的 #。
    - grid_color: 1B2436
      $name: Grid color
      $name:zh-CN: 网格颜色
      $description: Six-digit RGB hex color without the leading #.
      $description:zh-CN: 六位 RGB 十六进制颜色，不含开头的 #。
    - accent_color: 38BDF8
      $name: Accent color
      $name:zh-CN: 强调色
      $description: Six-digit RGB hex color without the leading #.
      $description:zh-CN: 六位 RGB 十六进制颜色，不含开头的 #。
    - text_color: E2E8F0
      $name: Text color
      $name:zh-CN: 文字颜色
      $description: Six-digit RGB hex color without the leading #.
      $description:zh-CN: 六位 RGB 十六进制颜色，不含开头的 #。
    - muted_text_color: 64748B
      $name: Muted text color
      $name:zh-CN: 次要文字颜色
      $description: Six-digit RGB hex color without the leading #.
      $description:zh-CN: 六位 RGB 十六进制颜色，不含开头的 #。
    - card_fill_color: "111827"
      $name: Card fill color
      $name:zh-CN: 卡片填充颜色
      $description: Six-digit RGB hex color without the leading #.
      $description:zh-CN: 六位 RGB 十六进制颜色，不含开头的 #。
  $name: Appearance
  $name:zh-CN: 外观

- Application:
    - exclude_list: [""]
      $name: Excluded executables
      $name:zh-CN: 排除的可执行文件
      $description: One executable name per entry, such as `program.exe`. Names are matched case-insensitively; lines beginning with # are comments.
      $description:zh-CN: 每个条目填写一个可执行文件名，例如 `program.exe`。名称不区分大小写，以 # 开头的条目为注释。
  $name: Application
  $name:zh-CN: 应用程序
*/
// ==/WindhawkModSettings==
