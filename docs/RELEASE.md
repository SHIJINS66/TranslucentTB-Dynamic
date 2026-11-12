# 2026.2.5-rc1 — community preview

## 中文说明

这是基于 TranslucentTB 2026.2 的非官方实验版，面向 Windows 11 x64。原软件和新增功能一起安装，不需要额外常驻配套进程。

本版提供任务栏分区动态取色、集成开关与参数面板、自适应/连续两种方式，包含缩略图保护、慢启动补刷新、正常退出恢复和桌面切换闪透明修复。rc1 与用户测试的 2026.2.5 热修使用同一主程序，仅更新文档、公开测试数据和打包材料。

安装文件：`TranslucentTB-Dynamic-2026.2.5-rc1-win11-x64-Setup.exe`。
配套源码：`TranslucentTB-Dynamic-2026.2.5-rc1-Source.zip`。
校验值：`SHA256SUMS.txt`，由发布输出目录提供。

安装前退出现有 TranslucentTB 和其他任务栏外观工具。全新安装默认开启自适应动态取色，10 段/120 FPS/采样上限 30 次每秒；覆盖安装保留个人配置。安装器会先备份现有安装文件，依赖准备失败会停止。rc1 安装器已在本机完成覆盖安装验证：静默安装约 5 秒、退出码 0，33 个载荷文件哈希与清单一致，原 `settings.json` 逐字节保留，旧版本自动备份，微软运行时依赖已存在直接跳过。

若更换了 Explorer 中已加载的 ExplorerTAP.dll，需要重启 Explorer 或注销后登录以加载匹配版本。rc1 相比用户刚才使用的 2.5 热修没有更换该 DLL。实测覆盖安装后首次启动会提示"已更新，请重启 Windows"并退出，重启 Explorer 后正常。

不喜欢动态效果时，在托盘菜单关闭“动态取色”即可回到原软件外观规则。需要回退程序时，先退出，保留自己的 settings.json，再使用已保存的上一版本安装包。安装器备份保存在 `%LOCALAPPDATA%\TranslucentTBDynamic\Backups`，不要把这一目录当成可直接分享的公开素材。

每 4 秒切换应用的本机测试中，自适应主进程平均 CPU 约 0.0137%，连续约 0.0381%，按 32 逻辑处理器总容量计算；该场景降幅约 64%。整机平均放电遥测为关闭 46.48 W、自适应 46.63 W、连续 44.96 W，未得到可归因于软件的省电结论。[测试报告](PERFORMANCE.zh-CN.md)。

安装器未签名，本版不属于官方发行。没有自动调整图标/文字对比度，HDR、特殊桌面及第三方任务栏组合也尚未充分验证。[已知问题](TESTING.md)。

## English notes

Unofficial, unsigned Windows 11 x64 preview based on TranslucentTB 2026.2. The installer includes the original app and integrated dynamic tint feature. rc1 uses the unchanged, user-tested 2.5 hotfix executable and updates release materials only.

It adds zone gradients, an integrated toggle/settings page and Adaptive/Continuous sampling, preserving hover protection, delayed-start refresh and normal-exit restoration. A pending-capture gap no longer intentionally restores all taskbars to transparent desktop appearance.

Exit other taskbar appearance utilities before installing. Existing settings are preserved; a fresh install enables Adaptive with 10 zones, 120 rendering FPS and a requested limit of 30 captures/sec. Backups are under `%LOCALAPPDATA%\TranslucentTBDynamic\Backups`. If replacing an ExplorerTAP already loaded into Explorer, restart Explorer or sign out/in. The rc1 installer was verified by installing over the existing installation on a Windows 11 x64 host: silent setup finished in about five seconds with exit code 0, all 33 packaged payload files matched the release manifest, the existing settings.json was preserved byte-for-byte and the previous installation was backed up. The first launch after an overwrite install shows the restart-required dialog and exits until Explorer is restarted.

Measured main-process CPU averages in the four-second toggle workload were 0.0137% Adaptive and 0.0381% Continuous on a 32-logical-processor host. The roughly 64% reduction is workload/process-specific. No attributable whole-machine power or battery-life improvement is established. Read [PERFORMANCE.md](PERFORMANCE.md) and [TESTING.md](TESTING.md), including the unresolved debug-logging crash and missing automatic text/icon contrast adjustment.
