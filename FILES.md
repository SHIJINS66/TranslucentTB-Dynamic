# File guide

One-line purpose for every file and folder in this repository. Start with `README.md`; this page is for orientation once you are inside the tree.

## Top level

| Path | Purpose |
|---|---|
| `README.md` | Project overview: screenshots, install, configuration ranges, measured cost, known issues. |
| `README.zh-CN.md` | Same overview in Simplified Chinese. |
| `LICENSE.md` | Full GNU GPLv3 text, inherited from upstream TranslucentTB. |
| `MODIFICATIONS.md` | What this community build changed, on which upstream commit, and how it is attributed. |
| `CHANGED-FILES.txt` | The 33 files that differ from the upstream 2026.2 source baseline. |
| `BINARY-SHA256.json` | Hashes of the six application binaries and resources that get installed. |
| `SOURCE-SHA256.json` | Raw-byte SHA-256 for the 422 application/source-project files in `src/`. |
| `SOURCE-CONTENT-SHA256.json` | The same 422 files hashed after CRLF→LF normalisation, so line-ending conversion does not look like tampering. |
| `REPOSITORY-SHA256.json` | SHA-256 for all 474 published files (itself excluded); used by `tools/verify_snapshot.py`. |
| `.gitattributes` | Forces LF storage on text and marks `.zip`, `.tar.gz`, `.png`, `.ico` as binary. |
| `.gitignore` | Keeps build output, runtime files, local settings, dumps, logs and drafts out of the repository. |

## `docs/`

| Path | Purpose |
|---|---|
| `docs/BUILD.md` | How to build the solution with Visual Studio 2026, vcpkg and the pinned dependencies. |
| `docs/DESIGN.md` | How the dynamic tint works: zone layout, capture scheduling, adaptivity, hover protection, restore-on-exit. |
| `docs/PERFORMANCE.md` | CPU and battery measurement setup, exported numbers and how they were calculated. |
| `docs/PERFORMANCE.zh-CN.md` | Chinese version of the performance report. |
| `docs/RELEASE.md` | Release notes in Chinese and English: install, upgrade, rollback, verification result. |
| `docs/TESTING.md` | What was actually verified, and the known issues and limits. |
| `docs/UPSTREAM.md` | Notes for the upstream maintainer: what to review, and how this work relates to issue #242. |
| `docs/images/desktop.png` | Plain wallpaper behind a coloured taskbar gradient. |
| `docs/images/chrome.png` | Browser window shading the taskbar. |
| `docs/images/netease-music.png` | Music player with a pink/grey taskbar gradient. |
| `docs/images/audition.png` | Audio editor with a dark taskbar gradient. |

## `benchmarks/`

| Path | Purpose |
|---|---|
| `benchmarks/README.md` | How the four-second taskbar-switch benchmark is run and what it measures. |
| `benchmarks/data/environment.json` | Machine, OS build, CPU, GPU, display and tool versions used for the measurement. |
| `benchmarks/data/automated-test-summary.json` | Counts and names of the automated tests that ran. |
| `benchmarks/data/switch-clicks.csv` | Every recorded click: timestamp, target window, resulting state. |
| `benchmarks/data/switch-samples.csv` | Per-interval CPU samples for each measurement state. |
| `benchmarks/data/switch-summary.json` | Aggregated means and the arithmetic behind the published percentages. |
| `benchmarks/scripts/Run-TaskbarBenchmark.ps1` | Orchestrates a run: builds the driver, drives the workload, collects samples. |
| `benchmarks/scripts/Collect-TaskbarSwitch.ps1` | Samples process CPU and writes the CSV/JSON exports. |
| `benchmarks/scripts/TaskbarSwitchDriver.cs` | C# helper that clicks a taskbar icon every four seconds without moving the pointer. |

## `installer/`

| Path | Purpose |
|---|---|
| `installer/setup.iss` | The Inno Setup 6 recipe actually used for this release: per-user install, backup, prerequisites, bilingual UI, embedded source ZIP. |
| `installer/settings.json` | Default settings shipped with a fresh install: dynamic tint on, Adaptive, 10 zones, 120 FPS, 30 captures/sec. |
| `installer/prerequisites.ps1` | Checks the Microsoft UI.Xaml / VCLibs packages and installs them only when missing. |
| `installer/payload-hashes.json` | Hashes of the six application binaries and resources that must land in the install directory. |
| `installer/ALL-PAYLOAD-SHA256.json` | Hashes of every packaged file, including the prerequisite AppX packages and licences. |
| `installer/ChineseSimplified.isl` | Simplified Chinese message file for Inno Setup. |
| `installer/README.md` | How to reconstruct the installer workspace and compile it with `ISCC.exe`. |

## `patches/`

| Path | Purpose |
|---|---|
| `patches/full-on-upstream-2026.2.patch` | Complete change set, applied on a clean upstream 2026.2 checkout. |
| `patches/adaptive-on-author-2026.2.patch` | Adaptive sampling layered onto the author's 2026.2 development baseline. |
| `patches/desktop-flash-on-2026.2.4.patch` | Desktop-switch flash fix applied on 2026.2.4. |

## `src/`

Application source, based on upstream TranslucentTB 2026.2 (`d4636e43`).

| Path | Purpose |
|---|---|
| `src/TranslucentTB/` | The main executable: tray icon, taskbar worker, dynamic tint capture and gradient rendering. |
| `src/Xaml/` | XAML pages and views, including the tray flyout and the dynamic tint settings page. |
| `src/Xaml/Strings/` | UI strings: `en-US`, `zh-CN` and `zh-TW` resource files (identical key sets; `en-US` is the project default language). |
| `src/ExplorerTAP/` | Taskbar appearance service DLL injected into Explorer, plus its IDL interface. |
| `src/ExplorerHooks/` | Hook DLL and hook plumbing used inside Explorer. |
| `src/ProgramLog/` | Logging library and its build configuration. |
| `src/Common/` | Shared configuration, scheduling utilities and undocumented-API headers used by the other projects. |
| `src/Tests/` | Unit tests for configuration, capture buffers, scheduling and the settings panel. |
| `src/AppPackage/` | MSIX packaging project: manifest, PRI resources and images for a packaged build. |
| `src/WindowsUdk/` | Shell view coordinator interfaces needed by the taskbar hook. |
| `src/vcpkg/` | Overlay ports for Detours and member-thunk. |

## `tools/`

| Path | Purpose |
|---|---|
| `tools/verify_snapshot.py` | Verifies a ZIP against `REPOSITORY-SHA256.json` and `SOURCE-SHA256.json`, checking CRC, file set and every hash. |

## `vendor-source-archives/`

| Path | Purpose |
|---|---|
| `vendor-source-archives/*.tar.gz` | Eleven pinned dependency source archives (WIL, fmt, spdlog, googletest, Detours, member-thunk, rapidjson) kept so the GPL/notice obligations can be met without network access. |

---

# 文件说明（中文）

每个文件和目录的用途各一行。先读 `README.md`，这份是进仓库后的地图。

## 顶层

| 路径 | 用途 |
|---|---|
| `README.md` | 项目总览：截图、安装、参数范围、实测开销、已知问题。 |
| `README.zh-CN.md` | 简体中文版总览。 |
| `LICENSE.md` | GNU GPLv3 全文，沿用上游 TranslucentTB。 |
| `MODIFICATIONS.md` | 本社区版改了什么、基于哪个上游提交、如何署名。 |
| `CHANGED-FILES.txt` | 相对上游 2026.2 源码基线有差异的 33 个文件。 |
| `BINARY-SHA256.json` | 会被装到本地的六个程序文件与资源的哈希。 |
| `SOURCE-SHA256.json` | `src/` 下 422 个源码与工程文件的原始字节 SHA-256。 |
| `SOURCE-CONTENT-SHA256.json` | 同一批文件按 CRLF→LF 归一化后的哈希，避免换行转换被误判为篡改。 |
| `REPOSITORY-SHA256.json` | 全部 474 个发布文件的哈希（不含自身），供 `tools/verify_snapshot.py` 校验。 |
| `.gitattributes` | 文本统一按 LF 存储，`.zip`、`.tar.gz`、`.png`、`.ico` 标记为二进制。 |
| `.gitignore` | 挡住编译产物、运行时文件、本地配置、转储、日志和草稿。 |

## `docs/`

| 路径 | 用途 |
|---|---|
| `docs/BUILD.md` | 用 Visual Studio 2026、vcpkg 和固定依赖构建整套解决方案的步骤。 |
| `docs/DESIGN.md` | 动态取色怎么工作：分区、取色调度、自适应、悬停保护、退出恢复。 |
| `docs/PERFORMANCE.md` | CPU 与功耗的测量方法、导出的数据和计算口径。 |
| `docs/PERFORMANCE.zh-CN.md` | 性能报告的中文版。 |
| `docs/RELEASE.md` | 中英双语发布说明：安装、升级、回退、验证结果。 |
| `docs/TESTING.md` | 实际验证过的范围，以及已知问题与限制。 |
| `docs/UPSTREAM.md` | 给上游维护者的说明：建议审阅哪些点、与 #242 的关系。 |
| `docs/images/desktop.png` | 纯壁纸下的任务栏渐变。 |
| `docs/images/chrome.png` | 浏览器窗口影响任务栏取色。 |
| `docs/images/netease-music.png` | 音乐播放器下的粉灰渐变。 |
| `docs/images/audition.png` | 音频编辑器下的深色渐变。 |

## `benchmarks/`

| 路径 | 用途 |
|---|---|
| `benchmarks/README.md` | 四秒任务栏切换基准怎么跑、测的是什么。 |
| `benchmarks/data/environment.json` | 测量用的机器、系统版本、CPU、GPU、显示器与工具版本。 |
| `benchmarks/data/automated-test-summary.json` | 已运行自动测试的数量与名称。 |
| `benchmarks/data/switch-clicks.csv` | 每次点击的时刻、目标窗口与结果状态。 |
| `benchmarks/data/switch-samples.csv` | 各测量状态下按间隔采到的 CPU 样本。 |
| `benchmarks/data/switch-summary.json` | 均值汇总，以及公开百分比背后的算式。 |
| `benchmarks/scripts/Run-TaskbarBenchmark.ps1` | 一次完整测量：构建驱动、执行负载、收集样本。 |
| `benchmarks/scripts/Collect-TaskbarSwitch.ps1` | 采集进程 CPU 并写出 CSV/JSON。 |
| `benchmarks/scripts/TaskbarSwitchDriver.cs` | 每四秒点一次任务栏图标、且不移动指针的 C# 辅助程序。 |

## `installer/`

| 路径 | 用途 |
|---|---|
| `installer/setup.iss` | 本版实际使用的 Inno Setup 6 脚本：用户级安装、备份、依赖准备、双语界面、内嵌源码 ZIP。 |
| `installer/settings.json` | 全新安装的默认配置：动态取色开启、自适应、10 段、120 FPS、30 次/秒。 |
| `installer/prerequisites.ps1` | 检查 Microsoft UI.Xaml / VCLibs，缺失时才安装。 |
| `installer/payload-hashes.json` | 必须落到安装目录的六个程序文件与资源的哈希。 |
| `installer/ALL-PAYLOAD-SHA256.json` | 所有被打包文件的哈希，含依赖 AppX 与许可证。 |
| `installer/ChineseSimplified.isl` | Inno Setup 的简体中文消息文件。 |
| `installer/README.md` | 如何重建安装器工作区并用 `ISCC.exe` 编译。 |

## `patches/`

| 路径 | 用途 |
|---|---|
| `patches/full-on-upstream-2026.2.patch` | 完整改动，打在干净的上游 2026.2 上。 |
| `patches/adaptive-on-author-2026.2.patch` | 自适应采样部分，叠加在作者 2026.2 开发基线上。 |
| `patches/desktop-flash-on-2026.2.4.patch` | 桌面切换闪帧修复，打在 2026.2.4 上。 |

## `src/`

应用源码，基于上游 TranslucentTB 2026.2（`d4636e43`）。

| 路径 | 用途 |
|---|---|
| `src/TranslucentTB/` | 主程序：托盘图标、任务栏工作线程、动态取色与渐变渲染。 |
| `src/Xaml/` | XAML 页面，含托盘菜单与动态取色参数页。 |
| `src/Xaml/Strings/` | 界面字符串：`en-US`、`zh-CN`、`zh-TW`，三套键完全相同，`en-US` 为工程默认语言。 |
| `src/ExplorerTAP/` | 注入 Explorer 的任务栏外观服务 DLL 及其 IDL 接口。 |
| `src/ExplorerHooks/` | Explorer 内部使用的钩子 DLL 与挂接代码。 |
| `src/ProgramLog/` | 日志库及其构建配置。 |
| `src/Common/` | 共享配置、调度工具与未公开 API 头文件。 |
| `src/Tests/` | 配置、采样缓冲、调度与参数面板的单元测试。 |
| `src/AppPackage/` | MSIX 打包工程：清单、PRI 资源与图片。 |
| `src/WindowsUdk/` | 任务栏钩子所需的 Shell 视图协调接口。 |
| `src/vcpkg/` | Detours 与 member-thunk 的 overlay port。 |

## `tools/`

| 路径 | 用途 |
|---|---|
| `tools/verify_snapshot.py` | 用 `REPOSITORY-SHA256.json` 与 `SOURCE-SHA256.json` 校验 ZIP：CRC、文件集合、每个哈希。 |

## `vendor-source-archives/`

| 路径 | 用途 |
|---|---|
| `vendor-source-archives/*.tar.gz` | 11 个固定版本的依赖源码包（WIL、fmt、spdlog、googletest、Detours、member-thunk、rapidjson），离线也能满足 GPL 与第三方许可义务。 |