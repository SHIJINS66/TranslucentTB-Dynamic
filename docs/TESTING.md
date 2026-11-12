# Testing status and known issues

## Verified scope

The application build corresponding to this candidate completed a serial Release/x64 full-solution build and passed **96 tests from 44 suites**, with no skipped capture tests. These include scheduling, input coalescing, bounded startup retries, mode/configuration behaviour, repeated GDI capture-buffer reuse and resizing/resource checks.

Earlier Windows Sandbox evaluation covered the mode selector's persistence, delayed painting, basic click/key colour changes and normal exit restoration. The 2026.2.5 hotfix subsequently received user testing of the WeChat/desktop-switch scenario; the user reported it basically passed. The attempted automated before/after foreground regression fixture did not yield a valid comparison and is **not** counted as passed.

The current executable was also measured under the four-second taskbar-switch workload. All 84 recorded clicks toggled the target's minimized state; all measured points kept the pointer on the taskbar. Active modes sampled and wrote gradients, and their measured stop-count increments were zero. This validates those state/counter observations, not every individual presented frame. See [PERFORMANCE.md](PERFORMANCE.md).

rc1 changes only documentation, public benchmark exports and packaging. It does not introduce another application build or repeat the user-interaction tests. Its executable hash is the tested hotfix hash in `BINARY-SHA256.json`.

The 2026.2.5-rc1 installer was run over the existing installation on Windows 11 x64 (build 26300). Silent setup finished in about five seconds with exit code 0; all 33 packaged payload files matched `installer/ALL-PAYLOAD-SHA256.json`; the previous installation was backed up under `%LOCALAPPDATA%\TranslucentTBDynamic\Backups`; the existing `settings.json` was preserved byte-for-byte. The Microsoft runtime prerequisites were already present and were skipped.

## Unresolved and untested

- **Text/icon contrast:** no automatic taskbar foreground recolouring is implemented. Extreme backgrounds can reduce readability.
- HDR, protected content, special desktops, all DPI/multimonitor arrangements and third-party taskbar modifications are not comprehensively covered.
- Low-level input hooks may be denied or removed. Low-rate capture remains, but immediate response to every input is not guaranteed.
- Hover/flyout protection can delay refresh. Startup retries are finite; very slow content still relies on later events and fallback capture.
- **Post-install start:** after an overwrite install the app shows its restart-required dialog and exits until Explorer is restarted, because Explorer still holds the previous ExplorerTAP.dll. This happens even when the reinstalled binary is byte-identical, since the installer refreshes the file timestamp.
- DWM process CPU was unavailable in the current measurement and was left blank, never recorded as zero. GPU power/cost was not isolated.

## Suggested feedback

For a reproducible report, describe Windows build, display count/scaling, sampling mode and relevant parameters, whether the pointer remained on the taskbar, and the exact action sequence. Redact personal window titles and private configuration before sharing diagnostics. Do not attach local settings backups or crash dumps publicly without review.

This candidate is for testing and discussion. It is not declared merge-ready against the latest upstream branch.
