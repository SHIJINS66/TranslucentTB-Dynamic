# CPU and battery-telemetry measurement

Measured on2026-11-12 using the unchanged2026.2.5 hotfix executable, also packaged in rc1. Hardware/environment: Intel Core i9-13980HX,32 logical processors, Windows11 Pro for Workstations build26300, two displays,66% brightness, Performance power scheme, battery discharge. Settings:16zones/120 rendering FPS/requested capture limit40/sec/automatic depth.

The taskbar button was labelled ChatGPT; in this setup its application identity was the current Codex desktop client, not a separately launched standard ChatGPT client. Other windows were initially minimized through the Shell. A script clicked that same taskbar icon every4seconds, alternating restored/minimized states, keeping the pointer on the icon and not operating application content.

Order:off,Adaptive,Continuous,Continuous,Adaptive,off. Each run settled for8seconds then measured48seconds; CPU/counters were sampled roughly each second, battery telemetry roughly every5seconds.

## Main-process results

CPU%=delta cumulative process CPU seconds/delta wall seconds/32*100. The denominator is total logical-processor time capacity, not frequency-weighted utilization. It can differ from Task Manager's instantaneous display. CPU ms/sec also reports process CPU time without dividing by logical processor count.

| Mode (mean of two runs) | Main-process CPU | Main-process CPU ms/sec | Explorer CPU | Captures/sec, both displays | Gradient writes/sec |
|---|---:|---:|---:|---:|---:|
| Dynamic tint off | 0.0020% | 0.650 | 0.0696% | 0.00 | 0.00 |
| Adaptive | 0.0137% | 4.392 | 0.0839% | 0.92 | 13.32 |
| Continuous | 0.0381% | 12.201 | 0.0752% | 0.83 | 13.70 |

Adaptive used about **64.0%** less main-process CPU time than Continuous **in this workload**. This is not whole-machine CPU, a general guarantee, or a power-saving percentage. Explorer hosts the helper and unrelated Shell work; it was observed but not isolated. DWM process CPU was unavailable and left null rather than zero. GPU cost was not independently measured.

All84clicks, including settling periods, toggled the minimized state, with intervals3.983–4.014s. Each measurement window contained12clicks (72total);12other clicks were in settling periods. All measurement points kept the pointer over the taskbar. Both enabled modes captured and wrote gradients, with zero measured dynamic-stop increments. GDI counts stayed50→50 and new capture allocations were zero in each run. These are state/counter observations, not frame-by-frame presentation verification.

Hover protection and selection allowances apply to both modes, so neither actually sampled40times/sec here. The CPU difference cannot be attributed entirely to capture frequency. Per-run means,1-second P95/max, counters and instrumentation cost are included in the exported JSON. The Chinese report has the full per-run table.

## Whole-machine power, briefly

Mean battery discharge telemetry was46.48W off,46.63W Adaptive and44.96W Continuous. Background/hardware activity, telemetry delay/quantization and instrumentation prevent attributing those differences to this feature. No watts-saved or battery-life improvement is established.

## Limits and data

Only two short runs per mode, one machine and one application-toggle pattern were measured. One-second maxima are not subsecond peaks. Background work was not fully isolated; sampling/click-driver CPU is included as separate observed processes. The sampler's first-run CPU was higher than later runs, an additional confound for whole-machine comparisons. Settings were restored byte-for-byte after measurement.

An earlier static-scene2.4 experiment measured about96.7% less main-process CPU time; it is a different version/workload and is not this test result. Neither96.7% nor64% is a whole-machine power claim.

[Public data](../benchmarks/data) is a de-identified derivative: relative timing and cumulative CPU/counter deltas remain, while absolute timestamps, user paths, handles, private configuration and diagnostic logs are omitted. [Benchmark materials](../benchmarks/README.md) document the parameterized reproduction scripts, which were syntax/compile checked rather than used to claim another run.

Executable SHA256:`23817D7A63F2C274D3698FCC48BA2DD6CD9B02271C54E4CC351942A50E4773A3`.
