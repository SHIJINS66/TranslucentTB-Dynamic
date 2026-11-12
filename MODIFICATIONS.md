# Modification and attribution record

Modification snapshot: **2026-11-12**, community packaging label **2026.2.5-rc1**.

Base application: TranslucentTB 2026.2, upstream commit `d4636e439865df0a1a1419db408e055740ce5c74`. Original copyright, GPLv3 licence, icon attribution and third-party notices remain with the source and installer. No upstream endorsement is claimed.

Changes add configurable dynamic zone tint, integrated XAML menu/settings controls, Adaptive/Continuous sampling, coalesced input/window hints with bounded startup retries and low-rate fallback, reusable GDI capture resources, gradient caching and normal-exit restoration, and a foreground pending-capture fix that keeps the visible brush until a new target is available. en-US, zh-CN and zh-TW strings and schema/configuration handling are included.

`CHANGED-FILES.txt` lists differences from the upstream source baseline. `patches/` contains verified alternatives for three baselines. `SOURCE-SHA256.json` covers all 422 materialized application/source-project files. rc1 changes the outer documentation, benchmark exports and packaging, not that tested application snapshot or its binaries.

Development used AI coding assistance, with user-directed requirements, manual testing and automated checks. The code has not received independent upstream review; unresolved issues and test limits are documented in `docs/TESTING.md`.

Microsoft runtime components and dependency source archives retain their respective notices/licences. Build tools, installed user settings, local backups, private application logs, dumps and generated dependency caches are not included as public repository content.
