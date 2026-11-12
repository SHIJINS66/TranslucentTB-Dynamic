# Public benchmark materials

data/ contains a de-identified derivative of the original observations. Absolute
timestamps, window/process handles, user paths, original user settings, battery
capacity and private application logs are omitted. Runs and relative cumulative
CPU/counter deltas remain, so the main-process averages can be independently
recomputed as final CPU seconds / final wall seconds /32 *100. switch-summary.json
is the numerical summary; environment.json records the relevant conditions.
The taskbar was labelled ChatGPT; the target in this setup was the current Codex
desktop client, not a separately started standard ChatGPT client.

scripts/ is a parameterized adaptation of the actual measurement scripts.
No window content is clicked: only the Windows taskbar icon is clicked, and
minimized-state changes are checked. The driver retains the pointer on the icon.
The collector backs up and verifies settings, changes modes temporarily and
restores original bytes when there is no external settings edit. It requires
battery discharge and stops below25% capacity. The driver attempts to undo the
Shell's initial minimize-all operation on normal exit. Avoid interrupting it;
check completion.json and the settings backup if cleanup is interrupted.

Run-TaskbarBenchmark.ps1 defaults to compiling and probing only (no clicks).
With -Run, an explicit current -TargetWindowHandle, and a fresh output directory,
it will minimize other windows, click every4s for about6min, and change sampling
modes. Use a single target window and leave the computer idle during the test.
The runtime hash is intentionally pinned to the tested2.5 executable. Review
all scripts first. A restarted application needs a freshly verified handle.

These portable adaptations were syntax/compile checked, not used to claim
another actual benchmark run. The published measurements came from the original
equivalent local scripts. Their protocol and limitations are in docs/PERFORMANCE.md.
