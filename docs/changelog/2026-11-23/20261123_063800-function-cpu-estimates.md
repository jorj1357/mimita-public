# Same-trace MiMITA function CPU estimates

- Reconciled xperf module CPU weights with exclusive per-process function stack hits from the saved 31.674-second WPR capture.
- Produced `.dev/perf-captures/mimita-live-cpu-function-estimates.csv` with estimated whole-machine CPU percentage and CPU milliseconds for sampled functions.
- The largest measured rows are WPR stack-walk routines (about 9.37 host CPU percentage points combined by the estimate); client audio resampling is the largest identified individual MiMITA function at about 0.38 host percentage points.
- Recorded formula, denominators, workload identity, and capture-overhead caveats in the CPU issue. This is not before/after proof and no code or runtime behavior changed.
