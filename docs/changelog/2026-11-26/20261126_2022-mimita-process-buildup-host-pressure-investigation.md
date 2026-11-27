# MiMITA process buildup and host pressure investigation

## User report

The user reported seeing eight MiMITA processes and said the load made networking unreliable and Explorer stop responding. The verbatim report is preserved in context ctx-30cfe96f70b8 and issue issue-a13a383a3f3d.

## Findings

### Current process/run observations

- At 2026-11-27T01:17:03.994Z, a host snapshot found 2 mimita.exe processes, 12 logical CPUs, 15.65 GB visible RAM, and only 0.28 GB available. Their working sets were 117.6 MB (PID 17732) and 694.5 MB (PID 27984).
- logs/2026-11-27/20261127_011550/run.json maps those PIDs to one server/client pair, run 20261127_011550, Bloodtest on Slaughterhouse 1 with no NPCs. The manifest later records this run as stopped at 01:17:56Z.
- A second isolated run, 20261127_011757, started at 01:17:57Z, with one TDM server/client pair on atdm. evidence-watch matched server PID 5900 to the live server journal. Its manifest shows build 2026-11-27T00:44:04.497616Z and generation 748. This successive pair does not prove overlap with the first run.
- Thus this inspection directly observed a severe low-memory state and rapid successive test runs, but did not observe eight simultaneous MiMITA processes.

### Retained five-minute resource capture

Aggregated .dev/resource-measurements/20261127T010558Z-dashboard-refresh-open.jsonl, which is pinned to run 20261127_001718 (TDM on atdm, one player and mean nine NPCs in the corresponding sample notes):

- 258 one-second nominal observations were captured from 01:07:11.308Z to 01:12:01.476Z. Host CPU was 99.9–100% (mean 100%).
- Available RAM averaged 726.65 MB and fell to 284.04 MB. Commit averaged 39,573.9/44,512.3 MB (88.9%) and peaked at 40,906.23 MB (91.9%). Pagefile use averaged 8,182 MB.
- The verified server PID 11572 averaged 7.86% of whole-host CPU; verified client PID 29544 averaged 14.48%. Two additional server processes, PIDs 20596 and 5440, were not matched to a run and averaged 7.18% and 0.51%. Together these four game processes averaged about 30.0% of host CPU. The managed client's private bytes peaked at 4,533.7 MB; the managed server's peaked at 1,418.5 MB.
- These processes made a substantial contribution during severe host pressure. They do not explain the remaining host CPU or identify every major competing process. The separate earlier resource review found FL64, DWM, Defender, System, OpenCode, and ChatGPT consuming material resources in other intervals; that is not same-window attribution.
- The sample metadata says a DLL batch was queued and no compile was active at precheck. This evidence does not support blaming an active compiler for that sample.

### Interpretation and limits

The best supported explanation is overlapping or successive dev-loop test work, with a TDM client/server workload and two unverified extra server processes appearing in a separate high-load sample. The dev-loop topology intentionally represents each server/client pair as two mimita.exe processes, and its runtime registry specification calls for concurrency caps by CPU, memory, ports, and disk. Existing records show gaps in attribution for some processes. This makes process accumulation a credible investigation lead.

CPU saturation, very low available RAM, high commit, and pagefile use plausibly explain broad desktop unresponsiveness, including Explorer stalls. MiMITA used about 30% of host CPU in the retained sample, so it was a material contributor. These observations do not prove MiMITA caused the reported networking failure or Explorer crash; the exact eight-process incident, network counters/errors, and all competing processes at that time are unknown.

## Tracking

Created complete issue issue-a13a383a3f3d, linked to issue-ed3ac775283d (CPU use) and issue-f5f8af2fe6bd (launch attribution). The issue includes the exact user quote, reproduction, acceptance, risks, proof expectation, and appended resource evidence.

## Evidence by type

- **Source:** Read-only inspection of runtime registry, multi-instance topology, live evidence observer, system resource measurement specification, run manifests, and journals.
- **Build:** None requested or run.
- **Runtime:** Read-only process/run snapshots and existing pinned resource capture; no process was stopped, restarted, or adopted.
- **Human review:** Explorer/networking causality and the exact eight-process episode remain unconfirmed.
- **Code changes/tests:** None.
