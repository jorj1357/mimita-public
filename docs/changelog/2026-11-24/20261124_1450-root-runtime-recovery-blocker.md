# Root runtime recovery gate — 2026-11-24 14:50Z

- A point OS sample at 14:50:41.747Z showed 0.534GiB free of 15.649GiB visible RAM (96.6% used). CPU was not sampled.
- Root dev-loop had server PID 25840 only, no client, no pending build, and `published_stale` / `builds_match=false`. The configured workload was Bloodtest/slaughterhouse1/1 NPC.
- The server journal at 14:50:21.366Z confirms 1 NPC/0 players and 60 ticks (mean 1.070ms, p95 6.444ms, max 9.822ms). Host DLL generation 594 and NPC module generation 593 were reported at 14:50:30.668Z and 14:50:30.985Z.
- This is not the live Juggernaut client/server acceptance workload. No client, new runtime, or build was started under current memory conditions. Full evidence is appended to CPU issue issue-ed3ac775283d and linked from latency issue issue-136d4553ff92.


## Bounded server-only resource sample at 2026-11-24T14:54Z

- Artifact: `.dev/perf-captures/mode3-server-only-resource-20261124T1450Z.csv`; UTC14:54:22.783–14:54:36.781, 15 rows, 12 logical CPUs.
- Host CPU averaged36.22% (25.7–45.0%); available RAM averaged2.71GiB/minimum2.69GiB; used memory averaged82.687%.
- Server PID25840 averaged105.0% of one logical CPU /8.75% whole-host, RSS27.61MB. Root daemon PID19372 averaged5.833% of one logical CPU /0.486% host, RSS49.99MB. Collection wall time averaged0.789ms/max2.653ms per row. psutil CPU time for the sampler rounded to0.0ms and is quantized, not evidence of zero CPU cost.
- Matching server journal confirms Bloodtest/slaughterhouse1, 1 NPC/0 players, 14 windows/848 ticks, mean reported tick average0.971ms and maximum individual tick12.765ms. Host DLL g594 and NPC module g593 remain mismatched. No client/build; top competing-process attribution unknown. This does not prove build causality and is not Juggernaut proof.
- Full issue records: CPU issue issue-ed3ac775283d and latency issue issue-136d4553ff92.
