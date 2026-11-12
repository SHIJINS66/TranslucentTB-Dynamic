# Notes for TranslucentTB maintainers

This is an experimental implementation related to [dynamic taskbar colouring, issue #242](https://github.com/TranslucentTB/TranslucentTB/issues/242), based on upstream 2026.2 commit `d4636e439865df0a1a1419db408e055740ce5c74`.

The [existing discussion](https://github.com/TranslucentTB/TranslucentTB/issues/242#issuecomment-574740300) raises polling cost and text/icon contrast. This implementation keeps low-rate polling rather than presenting input events as complete pixel-change detection. It does not solve automatic foreground contrast. The thread also contains a [separate Taskbar Match implementation](https://github.com/TranslucentTB/TranslucentTB/issues/242#issuecomment-5639577421); no novelty or first-implementation claim is made here.

## Scope and review entry points

- `Common/config/dynamictint.hpp`, schema and the existing XAML tray/settings integration: optional feature, validated controls, separate Adaptive/Continuous choice.
- `Common/util/dynamictintschedule.hpp`: absolute deadlines, coalesced input hints, bounded startup retries, idle fallback and short motion tracking.
- `TranslucentTB/taskbar/dynamictint.cpp`: per-zone sampling, filtering and bounded reusable GDI capture storage.
- `TranslucentTB/taskbar/taskbarattributeworker.cpp/.hpp`: event hints, input-hook lifecycle, hover/foreground handling and rendering.
- `ExplorerTAP/taskbarappearanceservice.cpp`: cached gradient application and the exit-restoration cache-clear fix. The desktop-switch hotfix itself adds no new RPC method.

The most recent hotfix distinguishes a pending empty target from a completed capture with no usable targets. It keeps the previous visible brush through an adaptive deadline gap rather than restoring every taskbar to its desktop appearance.

Please review reentrancy/lifetime handling around shell callbacks, input-hook threading/fallback, presentation ownership and shutdown, contrast/readability, protected-content behaviour, and Windows/display compatibility.

## Evidence

The application passed 96 existing/added tests, including GDI buffer reuse/resource checks. On one 32-logical-processor dual-display host, the four-second application-toggle workload measured main-process averages of 0.0020% off, 0.0137% Adaptive and 0.0381% Continuous. The approximately 64% reduction is limited to this process and workload; it is not whole-machine CPU, watts saved, or battery life. Explorer-hosted work was observed but not isolated. See [PERFORMANCE.md](PERFORMANCE.md) and the public counter exports.

## Alternative patches

Apply only the patch for the baseline you actually have, at that checkout's root:

| Patch | Required baseline |
|---|---|
| `full-on-upstream-2026.2.patch` | Upstream commit above |
| `adaptive-on-author-2026.2.patch` | Earlier 419-file author snapshot with parameter/exit fixes |
| `desktop-flash-on-2026.2.4.patch` | Previous 422-file community 2.4 snapshot |

Each was applied and compared against the current 422-file application snapshot after line-ending normalization. This repository's outer documentation/packaging layout is not part of those application patches. `SOURCE-SHA256.json` verifies the materialized application files; `REPOSITORY-SHA256.json` covers the outer release materials.

The intended next step is discussion about scope and architecture, rather than assuming the current patch is acceptable for direct merge. If maintainers prefer a separate project or a smaller refactor, that decision can be made before preparing a PR. No message, PR or release has been sent by this preparation task.
