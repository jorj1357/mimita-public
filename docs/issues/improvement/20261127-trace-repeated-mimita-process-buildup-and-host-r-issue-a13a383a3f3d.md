# issue-a13a383a3f3d: Trace repeated MiMITA process buildup and host resource exhaustion

## Human report (verbatim, immutable)

> can u invedsitgate somehow  why 8 mimtia open on comptuer its like it &#x20;
> it was so much running taht it  made the networking bad liek the whole explorer died  what happeend why is it so  resourceintenseive


## Agent suggestion

### 2026-11-27T01:21:22.437Z — codex-investigate-process-buildup

Additional direct evidence from the retained 5-minute resource capture .dev/resource-measurements/20261127T010558Z-dashboard-refresh-open.jsonl:
- Metadata pins run 20261127_001718, TDM/atdm, 12 logical CPUs, visible dashboard PID7820, managed server PID11572 and client PID29544; scheduled sample duration 300 s. The file contains 258 one-second nominal observations from 2026-11-27T01:07:11.308Z through 01:12:01.476Z, with gaps/missed observations during host load.
- Across those 258 observations, host CPU was 99.9–100% (mean 100%). Available RAM averaged 726.65 MB, minimum 284.04 MB. Commit averaged 39,573.9/44,512.3 MB (88.9%), peaked 40,906.23 MB (91.9%); pagefile use averaged 8,182 MB.
- MiMITA-related processes tracked in every sample: verified run server PID11572 averaged 7.86% whole-host CPU and private bytes peaked at 1,418.5 MB; verified client PID29544 averaged 14.48% whole-host CPU and private bytes peaked at 4,533.7 MB; two unverified servers PID20596 and PID5440 averaged 7.18% and 0.51% whole-host CPU (identity gaps remain). Dashboard averaged 0.82% and daemon 1.03% host CPU. At these observed rates the four game processes averaged ~30.0% of total host CPU. This is a substantial contribution during severe host pressure, not a complete causal explanation: host CPU remained saturated and other top competitors were not included in this capture.
- Separate later notes in docs/changelog/2026-11-26/20261126_231400-dashboard-resource-measurement.md found FL64, DWM, Defender, System, OpenCode, and ChatGPT also consuming material resources in earlier intervals; those are not same-window competitors for this capture.
- The run identity and exact PID pairing suggest repeated dev-loop tests may create multiple server/client MiMITA PIDs, while unverified child servers can outlive or fall outside their tracked run. The sample does not show eight simultaneous processes or prove Explorer/network causality.
