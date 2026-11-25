# Daily implementation and progress review: 2026-11-25

- Task: `daily-review-20261125`
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base/final commit: `eb92b02b21e62fd5ec496eee4dcad832f901bb7a`
- Scope: Read-only snapshot of implementation, build, runtime, performance, and coordination evidence. No source, config, build, or runtime changes by this review.

## What changed and what is implemented

- Game DLL generation 632 published from source generation 59, SHA `309ad58da6a8b8fc01c066792a76d75d25a9f312820c777c504eba8d7c0e878a`; the attempt completed in 23.121 s. Evidence: `.dev/build-timings/game-dll-20261125T012923599004Z-51b848.json`.
- The corresponding EXE build identity is `2026-11-25T01:21:28.987918Z:5dda08799cefdf4caad640a34a806c92e4a03c3886acdd4aa943be8c558eeb8b`, source generation 58. The status snapshot showed the root runtime stopped, with no current server/client.
- The clean Bloodtest run `20261125_013015` directly demonstrated projectile body-part classification and client gore. It did not demonstrate remote shooter-side hole effects, organ chunks, corpse shooting, or human visual acceptance. Its changelog and issue `issue-2e7b0cd6f868` contain the details.
- Vehicle arena/freeplay configuration validation emitted `config.gamemode.invalid` because `vehicle_possess_actor` is absent from the canonical masterlist although present in both mode JSON files and consumed by the loader. A complete, unowned issue was created: `issue-4e0dc0d71267`. The exact selected-mode runtime outcome remains unverified.

## Better, worse, and uncertain

- Better: generation 632 published after an earlier DLL attempt spent 248.211 s compiling and failed at link with three unresolved symbols; a cached retry failed in 9.004 s. This is recovery evidence, not proof that the cause is fixed generally.
- Mixed performance: the clean Bloodtest run reported 267–312 FPS and p50 2.9–3.4 ms, but p95 was 4.6–5.6 ms and p99 5.9–6.7 ms. Because these tail frame times exceed the 4.167 ms per-frame 240 FPS criterion in `docs/specs/performance/performance.md`, the changelog's “240 FPS budget met” wording needs reconciliation; average FPS alone does not establish that criterion.
- Server timing was 60.08 ticks/window overall and 60.73 in the last 40 windows, with 5/40 steady windows over 16.67 ms and NPC spikes up to 102 ms. This does not meet a 99.99% 60 Hz claim. An earlier run with two NPCs had 176/177 windows over 16.67 ms (worst 54.49 ms); workloads/runs differ, so this is not a matched before/after comparison.
- A prior DLL attempt recorded host CPU mean 97.546%, peak 100%, and only about 22 MB available RAM; generation 632 recorded CPU mean 58.978%, peak 82.619%, and about 311 MB minimum available RAM. These are separate build attempts, not a controlled resource comparison and not evidence of runtime CPU improvement.

## Quiet-time findings

- The review surfaced a measurement-language mismatch that can be missed when looking only at average FPS: the reported tail frame times conflict with the per-frame requirement. Keep both values visible in future summaries and do not label the budget met until reconciled against the raw frame samples.
- Repeated config validation warnings during Bloodtest startup reveal an invalid vehicle-mode config contract, although the warning alone does not prove the selected vehicle mode fails at runtime.
- This is the first saved daily snapshot, so there is no prior daily baseline for trend claims. The broad coordination board also reports many open/unproven/stale items; these are workflow-state counts, not a measure of completed product behavior.

## Evidence and limits

- Runtime evidence: `logs/2026-11-25/20261125_013015/` and `logs/2026-11-25/20261125_013233/`; run `013015` is stopped and its manifest has blank agent/requested-agent fields and `pinned_exe=false`.
- Build evidence: `.dev/build-timings/` entries above; build success proves publication/compilation, not feature behavior.
- Coordination evidence: current dev-loop status and agent coordination board at review time. Resource and run-identity concerns were cross-signaled to likely owners; no reply was present in the inbox at review time.
- Human acceptance: not performed by this review.
- Follow-up: validate `vehicle_possess_actor` for both modes through pinned dev-loop runs; reconcile the FPS percentile claim against raw samples; retain a matched NPC/tick performance comparison and complete unresolved visual/gameplay acceptance.
