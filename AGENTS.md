# AGENTS.md

> 本文件记录本仓库的运行时安全红线。所有接入本仓库的 Coding Agent 必须无条件遵循。

## 运行时安全红线（不可协商）

- **禁止自行重启 Windhawk**：不得结束、重启或强杀 `Windhawk.exe`、`windhawk-mod.exe`、
  `windhawk-cli.exe` 的宿主进程，不得卸载或重载 mod，不得通过脚本、任务管理器、
  `Stop-Process`、`taskkill`、服务重启等任何方式触发运行时重载。
- **原因**：Windhawk 进程与 Clash 内核等常驻服务存在联动，重启 Windhawk 会连锁关闭
  Clash 内核，进而导致 Windhawk 无法正常启动，恢复成本极高。
- **验证只走静态与离线路径**：源码改动一律通过仓库编译器与 Python 测试夹具验证
  （`python tools/bundle.py --check`、`python -m unittest discover -s tools/tests -t .`、
  原生夹具 `tools/tests/fixtures/aleph_canvas_native.cpp`）。运行时行为由用户手动验证。
- **不得声称未验证的运行时结论**：DWM 缩略图渲染、Alt 键真实状态、UWP 窗口、
  多虚拟桌面等只能在交互式 Windows 会话中观察的行为，未经验证时不得写入 PR、
  审查评论或交付说明。
