# Mode 3 build resource audit and DLL generation scope correction

- Session ID: work-6b9afcedd6d1
- Date: 2026-11-23
- Scope: inspect the latest shared Mode 3 DLL attempt, its automatic host/process resource samples, and the live generation evidence; append findings to the DLL latency issue.

## Findings recorded

- Appended the generation572 attempt audit to `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`.
- Attempt `game-dll-20261123T215115211280Z-e6886c` published generation572 in71.695s. It compiled three units and skipped640/642. Compile took57.778s (two real source TUs at56.274s and32.095s, overlapping), object-cache scan3.001s, LLD link5.402s, coherence0.174s, publish0.743s.
- Resource sidecar captured68 samples at about1s: 12 logical CPUs, mean host CPU99.578%, peak100%, average RAM use88.5%, minimum available RAM869,203,968 bytes. During compile, the two isolated MiMITA PIDs averaged23.37% and25.90% host CPU; two observed `cc1plus` processes averaged4.31% and4.06% host CPU over observed rows. This records pressure but does not prove CPU contention alone caused the compile duration.
- The sidecar attached root Bloodtest/0-NPC context, not a journal-matched Juggernaut workload. The isolated pair was alive, but current workload and generation572 activation were not proven during this attempt. No build or runtime action was initiated in this session.
- Corrected interpretation: `npc.phase-window.module_generation` identifies the folded NPC behavior module API generation, not every function in the active game DLL. A host DLL generation mismatch alone does not invalidate profiling of `npc.cpp` code compiled into the active DLL. `HostLoader::commit()` retains retired DLL handles until process exit, which avoids immediate dangling pointers but may retain memory over repeated reloads.

## Evidence paths

- `.dev/build-timings/game-dll-20261123T215115211280Z-e6886c.json`
- `.dev/build-timings/game-dll-20261123T215115211280Z-e6886c.jsonl`
- `.dev/build-timings/game-dll-20261123T215115211280Z-e6886c.resource.jsonl`
- `.dev/perf-captures/runtime-attribution-20261123T2137Z-{host,server,client}.csv` (earlier, non-matched no-build profile)

## Verification and remaining work

- `python devscripts/dev-loop.py --status --json --no-coord` reported no running build and no pending batch; generation572 was published. The isolated pair PIDs22184/32832 and separate root server PID10164 were alive at inspection.
- Build success is not live activation proof. The <=10s edit-to-live goal remains unmet. Next evidence must have a matched pre/build/post resource window, exact runtime-journal workload, process attribution for compiler/linker and MiMITA, the changed-source/note manifest, and generation/hash activation proof in the target pair.
- `git diff --check` and the session-specific changelog validation are to be recorded after this entry is written.

## Journal reconciliation correction

The initial audit mistakenly searched a stale journal chunk and said the isolated run had no build-window rows. The correct server chunk, `logs/2026-11-23/20261123_195307/server-01/events-0032.jsonl`, records NPC activity at21:52:15.836Z and host DLL generation571 / 67 NPCs / 1 player at21:52:16.078Z and21:52:17.080Z, during the build. The run was Juggernaut Horde on `dust2cyberiav4` in the immediately preceding rows. The root resource sidecar still attached the separate Bloodtest/0-NPC runtime context and omitted the isolated run association; its automatic runtime attribution needs correction.

A later same-server row at21:59:58.603Z reports generation572 with67 NPCs/1 player, so server-side eventual activation is now evidenced. The exact first activation time and the client-side generation are not yet identified; this is not an edit-to-live latency measurement or gameplay behavior proof. The earlier claim that no activation was found is superseded by this correction.

## Exact activation and post-build sample

The CPU profiler owner supplied exact journal references: client PID32832 activated generation572 in `client-01/events-0037.jsonl` at21:52:27.116Z; server PID22184 activated it in `server-01/events-0034.jsonl` at21:52:30.039Z. Sidecar completion was21:52:26.901Z, so activation followed by0.215s/3.138s. From batch seal21:51:14.674Z, request-to-activation was72.442s client /75.365s server. This still misses the <=10s request-to-live target, while showing DLL activation itself was not the 57.8-second bottleneck. Exact source-save time and intended NPC behavior proof remain unmeasured.

Post-build sample `.dev/perf-captures/postgen572-mode3-20261123T2211Z.csv` covered22:11:54.175–22:12:08.305Z, 12 rows /12 logical CPUs: host CPU mean99.992%, RAM use88.967%, available RAM mean1.853GB/min1.307GB; server22184 14.20% host, client32832 18.53%, pair32.73%; their RSS means206.2/228.0MiB. Sampler CPU averaged2.05% of one core (~0.17% host). This sample is about19 minutes after the build, not matched, and does not prove which processes caused continued saturation or what exact mode was active in that interval.

## Generation 573 and correction to sample timing

The root Mode3 attempt `game-dll-20261123T220945368970Z-09cabb` published generation573 after848.502s (22:09:45.369–22:23:53.871Z). It compiled72/643 units and skipped571. Changed inputs were two `.cpp` files plus three headers: `src/gamemode/gamemode.cpp`, `src/npc/npc.cpp`, `src/entities/aimbody-config.h`, `src/gamemode/gamemode.h`, and `src/render/post-fx.h`. Header changes explain why unedited translation units were recompiled: their objects depended on changed public headers. The retained last32 invalidation reasons show21 dependents of `gamemode.h`,6 of `post-fx.h`,4 of `aimbody-config.h`, and1 path-normalization case;40 compile reasons are absent because the summary caps the list. Do not skip valid dependents; reduce header fanout instead.

Build phases: changed-source preflight116.643s; object-cache scan2.091s; compile655.819s; LLD link50.465s/642 objects/~485MB; coherence0.887s; publish0.470s. The combined worker metric772.452s includes preflight and must not be added to these phases. Resource sidecar:534 samples/12 logical CPUs, host CPU mean99.996%, available RAM mean1.131GB/minimum19.19MB, pagefile committed mean42.87GB/max47.21GB of48.24GB limit. During compile `cc1plus` processes used about10.12% host CPU combined; tracked MiMITA processes29.25%; RobloxPlayerBeta12.93%, FL64 9.41%, SnapOS64 7.96%. This demonstrates severe concurrent pressure, but absent matched idle baseline and disk I/O it does not prove the cause of slow compilation. Sidecar runtime context was inaccurate/incomplete; target-run journals identify Juggernaut Horde, `dust2cyberiav4`,67 NPCs/1 player.

Correction: the earlier paragraph labels `.dev/perf-captures/postgen572-mode3-20261123T2211Z.csv` “post-build.” Its22:11:54.175–22:12:08.305Z capture overlapped generation573 during/near changed-source preflight. It measured host CPU mean99.992%, available RAM mean1.853GB/min1.307GB, and the pair at32.73% host CPU combined. Keep the raw filename; this is an in-build sample, not a post-build baseline.

Client PID32832 activated generation573 at22:23:55.311Z and server PID22184 at22:23:58.910Z, 1.440s/5.039s after publish. Request-to-live was about850.4s/854.0s; exact source-save time is missing. The <=10s goal remains unmet. This run indicates activation was not the main delay: expensive compilation, header invalidation, and the monolithic link dominated. Mode3 prevents repeated builds but does not itself accelerate an individual build.

## Correction: full generation 573 invalidation stream

The previous section said40 compiled units had no retained invalidation reason. That conclusion used only the attempt summary, whose `cache_invalidations` array is capped at32. The full `.jsonl` timeline contains all69 invalidation events for69 unique translation units:43 depend on `src/gamemode/gamemode.h` (42 absolute-path entries plus1 equivalent relative-path entry),17 depend on `src/render/post-fx.h`, and9 depend on `src/entities/aimbody-config.h`. The other three compiled units were the two directly edited `.cpp` files (`src/gamemode/gamemode.cpp`, `src/npc/npc.cpp`) and generated `module-identity-provider.cpp`. This accounts for all72 compilations;571/643 units were skipped. The actual observability gap is truncation in the summary view; the per-attempt timeline preserved the evidence.
