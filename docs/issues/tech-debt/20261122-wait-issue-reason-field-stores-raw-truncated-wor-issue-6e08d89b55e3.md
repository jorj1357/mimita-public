# issue-6e08d89b55e3: Wait-issue reason field stores raw truncated worker output

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Occurrence

### 2026-11-23T16:39:39.148Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-23T16:00:36.698Z (issue-ec790f30f5ad): evidence.reason is still a mid-JSON tail, ":47031.862,"build_attempt_id":"game-dll-20261123T160037032706Z-485a05",... }". It is the last 240 chars of the [GAME_FILE] module-identity-provider translation-unit row that set game_build_detail (dev-loop.py:3112/3142), not a readable stage summary. Confirms the same defect on a 2026-11-23 wait.

Evidence: {"issue": "issue-ec790f30f5ad", "reason_tail": ":47031.862,\"build_attempt_id\":\"game-dll-20261123T160037032706Z-485a05\"", "source": "devscripts/dev-loop.py _record_game_build_output detail=line.rstrip()[-240:]", "source_row": "module-identity-provider translation_unit.completed", "wait_id": "game-dll-build:2026-11-23T16:00:36.698Z"}

## Occurrence

### 2026-11-23T17:33:28.095Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-23T16:54:31.578Z (issue-3d02ab0de320): evidence.reason is again a mid-JSON tail: `9849.915,"build_attempt_id":"game-dll-20261123T165431661826Z-6c221c",...,"record_overhead_ms":4.976}`. It is the last 240 chars of the [GAME_FILE] server-physical-contact-actors.cpp row that set game_build_detail (dev-loop.py:3112/3142), not a readable stage summary. Build itself succeeded as generation 565 in 90.92s.

Evidence: {"build_attempt_id": "game-dll-20261123T165431661826Z-6c221c", "issue": "issue-3d02ab0de320", "reason_tail": "9849.915,\"build_attempt_id\":\"game-dll-20261123T165431661826Z-6c221c\"", "source": "devscripts/dev-loop.py _record_game_build_output detail=line.rstrip()[-240:]", "source_row": "server-physical-contact-actors.cpp translation_unit.completed", "wait_id": "game-dll-build:2026-11-23T16:54:31.578Z"}

## Occurrence

### 2026-11-24T04:17:13.659Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-24T03:30:57.098Z (issue-456061c38a8c): evidence.reason is again a mid-JSON tail, not a readable stage summary: 'CFG cleanup\",\"raw\":\"tree CFG cleanup : 0.24 ( 1%) 77k ( 0%)... \"truncated\":true},\"progress_write_ms\":4.341,\"record_overhead_ms\":4.894}'. It is the last 240 chars of the [GAME_FILE] translation_unit.completed row that set game_build_detail (dev-loop.py:3892 reason=self.game_build_detail), same defect as the two 2026-11-23 occurrences. The build attempt itself failed at changed_source_preflight (src/ragdoll/ragdoll-mode.cpp).

Evidence: {"build_attempt_id": "game-dll-20261124T033057531530Z-a2c134", "issue": "issue-456061c38a8c", "journal": "logs/2026-11-24/20261124_022440_01/dev-loop/events.jsonl line 90", "reason_tail": "ggc\":{\"value\":77.0,\"unit\":\"k\"...\",\"record_overhead_ms\":4.894}", "source": "devscripts/dev-loop.py:3892 reason=self.game_build_detail (last 240 chars of raw worker output)", "wait_id": "game-dll-build:2026-11-24T03:30:57.098Z"}

## Occurrence

### 2026-11-24T08:39:19.714Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-24T07:37:44.064Z (issue-8bdc83a5708d): evidence.reason is again a mid-JSON tail, not a readable stage summary: ':50296.649,"build_attempt_id":"game-dll-20261124T073744074582Z-faf01b","source_generation":8,"generation":586,"revision":"69a02bd6...","progress_write_ms":3.63,"record_overhead_ms":4.275}'. It is the last 240 chars of the translation_unit.completed row for build/game/generation-586/module-identity-provider.cpp at wall_time 2026-11-24T07:38:34.477Z (.dev/build-timings/game-dll-20261124T073744074582Z-faf01b.jsonl line 768), set by dev-loop.py:3112 detail=line.rstrip()[-240:] and read at dev-loop.py:3892 reason=self.game_build_detail. Same defect as the three prior occurrences. The build itself succeeded as generation 586 in 136.2s.

Evidence: {"build_attempt_id": "game-dll-20261124T073744074582Z-faf01b", "generation": 586, "issue": "issue-8bdc83a5708d", "reason_tail": ":50296.649,\"build_attempt_id\":\"game-dll-20261124T073744074582Z-faf01b\"", "source": "devscripts/dev-loop.py:3112 _record_game_build_output detail=line.rstrip()[-240:]; dev-loop.py:3892 reason", "source_row": "translation_unit.completed build/game/generation-586/module-identity-provider.cpp wall_time=2026-11-24T07:38:34.477Z", "wait_id": "game-dll-build:2026-11-24T07:37:44.064Z"}

## Agent suggestion

### 2026-11-24T12:35:58.751Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-24T10:36:51.643Z (issue-8576955c4fb5): evidence.reason is again a mid-JSON tail, not a readable stage summary: ncremental\",\"raw\":\"tree SSA incremental ... progress_write_ms\":3.389,\"record_overhead_ms\":4.062}. It is the last 240 chars of a game-build worker line that set game_build_detail, read at dev-loop.py reason=self.game_build_detail. Same defect as the prior occurrences. The build attempt game-dll-20261124T103652183287Z-745ef3 succeeded as generation 590 in 989.864s.

## Occurrence

### 2026-11-24T19:41:15.127Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-24T19:08:17.245Z (issue-f15a2daf6c16): evidence.reason is again a mid-JSON tail, not a readable stage summary: '9932.729,"build_attempt_id":"game-dll-20261124T190817284712Z-0003b3","source_generation":28,"generation":602,"revision":"1f89e048...","progress_write_ms":4.235,"record_overhead_ms":4.915}'. It is the last 240 chars of a game-build worker line, set by dev-loop.py:3112/3142 detail=line.rstrip()[-240:] and read at dev-loop.py:3892 reason=self.game_build_detail. Same defect as the prior occurrences. The attempt itself failed at compile (src/vehicles/vehicle-system.cpp:700 collisionHalf undeclared); generation 606 later built cleanly.

Evidence: {"build_attempt_id": "game-dll-20261124T190817284712Z-0003b3", "issue": "issue-f15a2daf6c16", "journal": "logs/2026-11-24/20261124_170535/dev-loop/events.jsonl", "over_budget_wall_time": "2026-11-24T19:09:19.574Z", "reason_tail": "9932.729,\"build_attempt_id\":\"game-dll-20261124T190817284712Z-0003b3\"", "source": "devscripts/dev-loop.py:3112/3142 _record_game_build_output detail=line.rstrip()[-240:]; dev-loop.py:3892 reason", "wait_id": "game-dll-build:2026-11-24T19:08:17.245Z"}

## Occurrence

### 2026-11-24T20:08:00.199Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-24T19:41:25.941Z (issue-536424f63590): evidence.reason is again a mid-JSON tail, not a readable stage summary: '5213.615,"build_attempt_id":"game-dll-20261124T194125987951Z-4d7bb0","source_generation":48,"generation":609,"revision":"d9578a88...","progress_write_ms":4.299,"record_overhead_ms":4.849}'. It is the last 240 chars of a game-build worker row, set by dev-loop.py:3112/3142 detail=line.rstrip()[-240:] and read at dev-loop.py:3892 reason=self.game_build_detail. Same defect as the prior occurrences. The attempt itself succeeded as generation 609 in 801.055s (src/entities/player.h fanout 233; 234 TUs compiled). Journal logs/2026-11-24/20261124_192708/dev-loop/events.jsonl dev-loop.wait.over_budget wall_time 2026-11-24T19:42:45.433Z.

Evidence: {"build_attempt_id": "game-dll-20261124T194125987951Z-4d7bb0", "changed_header": "src/entities/player.h", "generation": 609, "header_fanout": 233, "issue": "issue-536424f63590", "journal": "logs/2026-11-24/20261124_192708/dev-loop/events.jsonl", "over_budget_wall_time": "2026-11-24T19:42:45.433Z", "reason_tail": "5213.615,\"build_attempt_id\":\"game-dll-20261124T194125987951Z-4d7bb0\"", "source": "devscripts/dev-loop.py:3112/3142 _record_game_build_output detail=line.rstrip()[-240:]; dev-loop.py:3892 reason", "total_seconds": 801.055, "wait_id": "game-dll-build:2026-11-24T19:41:25.941Z"}

## Occurrence

### 2026-11-24T20:26:46.133Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-24T20:06:49.528Z (issue-b18b7d8cf193): evidence.reason is again a mid-JSON tail, ':3962.592,"build_attempt_id":"game-dll-20261124T200649543312Z-74e74d","source_generation":10,"generation":613,"revision":"fd7a1a99...","progress_write_ms":4.092,"record_overhead_ms":4.799}'. It is the last 240 chars of the [GAME_FILE] translation_unit.completed row that set game_build_detail (dev-loop.py:3112/3142 detail=line.rstrip()[-240:]), read at dev-loop.py:3892 reason=self.game_build_detail. Same defect as the prior occurrences. The attempt game-dll-20261124T200649543312Z-74e74d itself failed at link (undefined MimitaNet::mp* symbols) after 107.111s.

Evidence: {"build_attempt_id": "game-dll-20261124T200649543312Z-74e74d", "generation": 613, "issue": "issue-b18b7d8cf193", "outcome": "failed_phase_link", "reason_tail": ":3962.592,\"build_attempt_id\":\"game-dll-20261124T200649543312Z-74e74d\"", "source": "devscripts/dev-loop.py:3112/3142 _record_game_build_output detail=line.rstrip()[-240:]; dev-loop.py:3892 reason", "source_row": "translation_unit.completed changed path src/entities/player-render.cpp", "summary": ".dev/build-timings/game-dll-20261124T200649543312Z-74e74d.json", "total_seconds": 107.111, "wait_id": "game-dll-build:2026-11-24T20:06:49.528Z"}

## Occurrence

### 2026-11-24T20:48:12.376Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-24T20:00:45.963Z (issue-a4eea8fe2246): evidence.reason is again a mid-JSON worker-row tail, not a readable stage summary: ':51674.542,"build_attempt_id":"game-dll-20261124T200045986936Z-3f16b3","source_generation":9,"generation":611,"revision":"d624e3bd1f8425733ccfb674a45cf149f22f9ffd73d054450a8ae1703ba70911","progress_write_ms":3.415,"record_overhead_ms":3.99}'. It is set by dev-loop.py _record_game_build_output detail=line.rstrip()[-240:] and read at check_wait_budget() reason=self.game_build_detail. Same defect as prior occurrences. The attempt game-dll-20261124T200045986936Z-3f16b3 compiled 34 TUs and failed at src/entities/player-render.cpp:634.

Evidence: {"build_attempt_id": "game-dll-20261124T200045986936Z-3f16b3", "issue": "issue-a4eea8fe2246", "reason_tail": ":51674.542,\"build_attempt_id\":\"game-dll-20261124T200045986936Z-3f16b3\",\"source_generation\":9,\"generation\":611,\"revision\":\"d624e3bd1f8425733ccfb674a45cf149f22f9ffd73d054450a8ae1703ba70911\",\"progress_write_ms\":3.415,\"record_overhead_ms\":3.99}", "source": "devscripts/dev-loop.py _record_game_build_output detail=line.rstrip()[-240:]; check_wait_budget reason=self.game_build_detail", "wait_id": "game-dll-build:2026-11-24T20:00:45.963Z"}

## Occurrence

### 2026-11-24T21:29:39.096Z — build-wait-reviewer

Recurrence 2026-11-24T20:19:59Z wait (issue-7ef5043ce95c): evidence.reason for attempt game-dll-20261124T201959398000Z-aa0b47 is the mid-JSON tail '4158.436,"build_attempt_id":"game-dll-20261124T201959398000Z-aa0b47","source_generation":16,"generation":616,...,"record_overhead_ms":4.454}' instead of a readable [GAME BUILD] summary. The same worker window journaled a readable [GAME BUILD] reason for the later 20:55:04 wait, so the corruption depends on which worker line last set game_build_detail.

Evidence: {"build_attempt_id": "game-dll-20261124T201959398000Z-aa0b47", "issue": "issue-7ef5043ce95c", "wait_id": "game-dll-build:2026-11-24T20:19:59.371Z"}

## Occurrence

### 2026-11-25T02:36:53.823Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-25T02:11:23.680Z (issue-0612aaee5a6e): evidence.reason is again the last 240 chars of a [GAME_FILE] translation-unit row, not a readable stage summary: '8273.352,"build_attempt_id":"game-dll-20261125T021123740140Z-5e8bdb","source_generation":67,"generation":633,"revision":"2f19b1b5...","progress_write_ms":3.674,"record_overhead_ms":4.489}'. It matches the module-identity-provider translation_unit.completed row at 2026-11-25T02:12:12.069Z (elapsed_ms 48273.352), set by dev-loop.py:3112/3142 detail=line.rstrip()[-240:] and read at dev-loop.py:3892 reason=self.game_build_detail. Same defect as prior occurrences. The waited-on attempt then failed at worker_supervised reason=timeout (300 s) at 02:16:32.288Z; the next attempt published generation 634 in 180.66 s.

Evidence: {"build_attempt_id": "game-dll-20261125T021123740140Z-5e8bdb", "issue": "issue-0612aaee5a6e", "journal": "logs/2026-11-24/20261124_221014/dev-loop/events.jsonl", "over_budget_wall_time": "2026-11-25T02:12:41.013Z", "reason_tail": "8273.352,\"build_attempt_id\":\"game-dll-20261125T021123740140Z-5e8bdb\"", "source": "devscripts/dev-loop.py:3112/3142 _record_game_build_output detail=line.rstrip()[-240:]; dev-loop.py:3892 reason", "source_row": "module-identity-provider translation_unit.completed at 2026-11-25T02:12:12.069Z", "wait_id": "game-dll-build:2026-11-25T02:11:23.680Z"}

## Agent suggestion

### 2026-11-25T06:11:06.017Z — build-wait-reviewer

Occurrence 2026-11-25T05:46:30.064Z (issue-9c1fc3e3e668, attempt game-dll-20261125T054630085917Z-48e192): evidence.reason is still an unreadable mid-JSON tail beginning 'struction... "raw":"tree CFG construction ...' and ending with "progress_write_ms":4.459,"record_overhead_ms":5.283. This is the same trailing-worker-output defect. Reviewer build-wait-reviewer; no fix claimed.

## Agent suggestion

### 2026-11-25T21:36:44.100Z — build-wait-reviewer

New confirming occurrence (2026-11-25T21:3xZ, review of issue-2e5c794199f1). wait_id game-dll-build:2026-11-25T21:00:55.639Z, elapsed 60.633s. evidence.reason is a mid-JSON tail of a [GAME_FILE] ftime report (ends "truncated":true},"progress_write_ms":58.898,"record_overhead_ms":60.151}). The most recent worker line at the budget check was the translation_unit.completed ftime payload for src/input/gamepad-input.cpp (38.744s) under attempt game-dll-20261125T210055875592Z-33009d (generation 646). Confirms this issue's root cause (_record_game_build_output detail=line.rstrip()[-240:]). The build succeeded (exit 0, gen 646); only the reason is unreadable. Read-only; no fix claimed.

## Agent suggestion

### 2026-11-25T22:05:29.657Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-25T21:45:58.563Z (issue-63ec094b8859, run 20261125_212905): evidence.reason is again a mid-JSON tail, not a readable stage summary: :57765.0,"build_attempt_id":"game-dll-20261125T214558738291Z-806cc2","source_generation":25,"generation":650,"revision":"5d18bf5a...","progress_write_ms":6.234,"record_overhead_ms":6.828. It matches the translation_unit.completed identity-provider row that set game_build_detail (dev-loop.py:3112/3142 detail=line.rstrip()[-240:]), read at the wait arm dev-loop.py:~3892 reason=self.game_build_detail. The waited-on attempt failed worker_timeout at 309.344s; the retry published generation 651 in 84.812s. Same defect as prior occurrences; no source edit made.

## Agent suggestion

### 2026-11-25T22:32:59.809Z — build-wait-reviewer

Occurrence (wait game-dll-build:2026-11-25T21:53:31.363Z, run 20261125_212905): recorded reason is exactly a mid-JSON tail, "9906.0,""build_attempt_id"":""game-dll-20261125T215331554351Z-19bd08"",...". Confirms game_build_detail is the last 240 chars of the raw [GAME_FILE] worker line (dev-loop.py:3112 detail=line.rstrip()[-240:]; 3142 game_build_detail=detail) and is copied verbatim into the wait reason (dev-loop.py:3892). Reviewer: build-wait-reviewer.

## Occurrence

### 2026-11-26T00:03:55.923Z — build-wait-reviewer

Wait `game-dll-build:2026-11-25T23:04:23.764Z` (build attempt game-dll-20261125T230423811208Z-c638b8, run 20261125_221046) again stored reason as a mid-JSON tail starting with `:59141.0,`. Confirmed on issue-4ab3b491ad6c and again on issue-6d572fc03e97 (reason starts `:67515.0,`). The unchanged source is dev-loop.py:_record_game_build_output `detail = line.rstrip()[-240:]`; the last worker line was a [GAME_FILE]/[GAME_STAGE] JSON payload. Occurrence count now 2 new today (23:04 and 23:50 UTC). No duplicate issue filed.

Evidence: {"reason_prefixes": [":59141.0,", ":67515.0,"], "wait_ids": ["game-dll-build:2026-11-25T23:04:23.764Z", "game-dll-build:2026-11-25T23:50:11.848Z"]}

## Agent suggestion

### 2026-11-26T01:09:21.087Z — build-wait-reviewer

Third same-day occurrence on wait game-dll-build:2026-11-26T00:29:50.339Z (issue-4a0450674f5f, attempt game-dll-20261126T002950581148Z-fff09d, run 20261126_002257): evidence.reason is again a mid-JSON tail starting ":52563.0,". Same unchanged source path dev-loop.py:_record_game_build_output detail=line.rstrip()[-240:]. The wait record now also shows build_id=2026-11-26T00:06:41.220046Z and generation=null instead of the attempt/gen 660, so this record also loses build identity in the snapshot. Evidence appended in the 01:06Z review on issue-4a0450674f5f; no duplicate issue filed.

## Occurrence

### 2026-11-26T01:39:32.531Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-26T00:55:56.696Z (issue-ac23ab33d184; attempt game-dll-20261126T005556909723Z-fa8154). evidence.reason is still the last 240 characters of a [GAME_FILE] row, starting `s":57328.0,` and ending with progress_write_ms/record_overhead_ms. It still embeds the in-flight attempt id and generation 662, but it is not a readable stage summary. Source unchanged: dev-loop.py:3112 `detail = line.rstrip()[-240:]`, assigned at dev-loop.py:3142, read at dev-loop.py:3892. Same-truncation sibling observation: dev-loop.wait.reviewer-wake's launch_result (build_friction.py:250, `launched.stdout.strip()[-2000:]`) also recorded a mid-JSON/prompt fragment for this wait instead of a structured launch result. Owner may decide whether that belongs in this issue's acceptance. No duplicate issue filed.

Evidence: {"build_attempt_id": "game-dll-20261126T005556909723Z-fa8154", "issue": "issue-ac23ab33d184", "journal": "logs/2026-11-26/20261126_005302/dev-loop/events.jsonl", "reason_prefix": "s\":57328.0,", "sibling": "devscripts/build_friction.py:250 launch_result=stdout[-2000:]", "source": "devscripts/dev-loop.py:3112,3142,3892", "wait_id": "game-dll-build:2026-11-26T00:55:56.696Z"}

## Agent suggestion

### 2026-11-26T15:53:41.232Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-26T15:41:17.898Z (issue-ab0d4daf980a; attempt game-dll-20261126T154118055873Z-1d5285, generation 690, run 20261126_145549; journal logs/2026-11-26/20261126_145549/dev-loop/events.jsonl). evidence.reason is the same mid-JSON tail starting ':57547.0,' and ending with 'progress_write_ms:6.898, record_overhead_ms:7.611'. It matches the [GAME_FILE] translation_unit.completed row for src/network/community-match-client.cpp at wall 15:42:15.831Z (elapsed_ms 57547.0), which was the last worker line to set game_build_detail (dev-loop.py:3112 detail=line.rstrip()[-240:], assigned :3142, read :3892). Same defect; no duplicate issue filed. The waited-on attempt then failed at compile in src/vehicles/vehicle-system.cpp:1322 (shared_ptr to raw VehicleProfile*), 74.172s total. The record also shows the related stale identity: build_id=2026-11-26T06:10:41.962780Z (EXE artifact id via self.latest_build) and generation=688 (previous publish) instead of the in-flight generation 690, and it carried no run_id.

## Agent suggestion

### 2026-11-26T17:25:37.181Z — build-wait-reviewer

Occurrence 2026-11-26 gen 698 / issue-4fb7a24c54b0: the over-budget wait evidence reason was `"s":57812.0,"build_attempt_id":"game-dll-20261126T165624137995Z-32c560","source_generation":20,...` - the tail of the last worker output line. Root: dev-loop.py:3112 sets detail = line.rstrip()[-240:] and dev-loop.py:3892 stores it as reason. The actual phase (compile 140.3s, 64 TUs, max TU 57.3s) is not readable from the record; the attempt id survived only because it fell inside the last 240 chars. Journal logs/2026-11-26/20261126_165201/dev-loop/events.jsonl:128; timings .dev/build-timings/game-dll-20261126T165624137995Z-32c560.json.

## Agent suggestion

### 2026-11-26T23:11:55.158Z — build-wait-reviewer

Occurrence 2026-11-26T22:39Z (wait game-dll-build:2026-11-26T22:38:08.169Z, attempt game-dll-20261126T223808304994Z-2d57e6, run 20261126_201711). The wait issue reason field is again a raw truncated worker-output tail rather than readable context: `:24828.0,build_attempt_id:game-dll-20261126T223808304994Z-2d57e6,source_generation:7,generation:731,revision:4633798b...,progress_write_ms:6.061,record_overhead_ms:7.005}`. In this occurrence the fragment happens to embed the true attempt id, generation, and revision (so identity is recoverable by luck), but it starts mid-value and is not human-readable. Journal: logs/2026-11-26/20261126_201711/dev-loop/events.jsonl. Observed; fix remains owned here.

## Agent suggestion

### 2026-11-27T04:11:24.733Z — build-wait-reviewer

Occurrence 2026-11-27T03:48:13.916Z (wait game-dll-build:2026-11-27T03:46:49.521Z, wait issue issue-f36679f4ba95, attempt game-dll-20261127T034649716100Z-80c771, run 20261127_032430): evidence.reason is again a mid-JSON tail, not a readable stage summary - it starts `59485.0,"build_attempt_id":...` and ends `..."progress_write_ms":18.099,"record_overhead_ms":18.723}`. It is the tail of the dev-loop.game-build.file row for src/network/multiplayer-reconcile.cpp at wall 03:47:49.421Z (elapsed_ms 59485.0), set by dev-loop.py detail=line.rstrip()[-240:] and read as the wait reason. Same occurrence also carries the stale identity: evidence.build_id=2026-11-27T01:43:27.658897Z and generation=null instead of the in-flight attempt/gen 759 (revision 26a2b63e...). Journal logs/2026-11-27/20261127_032430/dev-loop/events.jsonl lines 91-92; build summary .dev/build-timings/game-dll-20261127T034649716100Z-80c771.json. No duplicate issue filed; no fix claimed.

## Agent suggestion

### 2026-11-27T06:53:15.222Z — build-wait-reviewer

Recurring reason-slice occurrence: wait game-dll-build:2026-11-27T06:22:39.059Z (issue-b783370b40b2, attempt game-dll-20261127T062239069032Z-64213c, generation 782) again stored evidence.reason as a mid-JSON tail starting "60149.03,\"build_attempt_id\":...". Same unchanged source: dev-loop.py _record_game_build_output detail = line.rstrip()[-240:]. This is a new occurrence of the empty readble-reason defect; no duplicate issue filed. Identity was also stale there (build_id=2026-11-27T04:28:12.568764Z, generation=781 while the in-flight attempt was gen 782, no run_id).

## Occurrence

### 2026-11-27T07:59:02.838Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-27T07:23:16.007Z (issue-778fe2977118; attempt game-dll-20261127T072316157011Z-06c929; run 20261127_072104). evidence.reason is again a mid-JSON tail: `6085.139,"build_attempt_id":"game-dll-20261127T072316157011Z-06c929","source_generation":25,"generation":784,"revision":"18132b98...","progress_write_ms":6.052,"record_overhead_ms":7.447}`. It is the last 240 chars of a [GAME_FILE] worker row, not a readable stage summary. Source unchanged: devscripts/dev-loop.py:3112 `detail = line.rstrip()[-240:]`, assigned at dev-loop.py:3142, read at dev-loop.py:3892 `reason=self.game_build_detail`. The waited-on attempt succeeded as generation 784 in 68.594s, so the raw tail is the only defect here. No duplicate issue filed.

Evidence: {"agent": "build-wait-reviewer", "build_attempt_id": "game-dll-20261127T072316157011Z-06c929", "generation": 784, "issue": "issue-778fe2977118", "journal": "logs/2026-11-27/20261127_072104/dev-loop/events.jsonl", "journal_line": 52, "over_budget_wall_time": "2026-11-27T07:24:28.723Z", "reason_prefix": "6085.139,\"build_attempt_id\":\"game-dll-20261127T072316157011Z-06c929\"", "source": "devscripts/dev-loop.py:3112,3142,3892", "total_seconds": 68.594, "wait_id": "game-dll-build:2026-11-27T07:23:16.007Z"}

## Agent suggestion

### 2026-11-27T16:03:12.576Z — build-wait-reviewer

Confirmed recurrence 2026-11-27 on issue-3736371ce4de (wait game-dll-build:2026-11-27T07:55:43.159Z, attempt game-dll-20261127T075543204684Z-cefc30). evidence.reason is again a mid-JSON tail, not a stage summary: ':58155.9,"build_attempt_id":"game-dll-20261127T075543204684Z-cefc30","source_generation":24,"generation":787,...'. The wait record is otherwise valid (60.533s, gen 787, 107.3s genuine build). Reviewer: build-wait-reviewer.

## Agent suggestion

### 2026-11-27T17:48:46.154Z — build-wait-reviewer

Occurrence 2026-11-27T16:54:14.303Z (issue-795749b08e31, wait game-dll-build:2026-11-27T16:54:14.303Z, attempt game-dll-20261127T165414319868Z-c53bb1, run 20261127_165709, generation 798). evidence.reason is again a mid-JSON worker tail: '10213.216,"build_attempt_id":"game-dll-20261127T165414319868Z-c53bb1","source_generation":3,"generation":798,"revision":"6afedba40b7640dc725e8ae7f081235b3f51e09d0d362b28f910f01f221122f1","progress_write_ms":4.122,"record_overhead_ms":4.882}'. It is exactly the last 240 chars of the module-identity-provider translation_unit.completed row at .dev/build-timings/game-dll-20261127T165414319868Z-c53bb1.jsonl:765 (elapsed_ms 10213.216), set by dev-loop.py:3112 detail=line.rstrip()[-240:], assigned :3142, read :3893 reason=self.game_build_detail. Same unchanged defect. The build itself succeeded as generation 798 in 435.208s. No duplicate issue filed.

## Agent suggestion

### 2026-11-27T17:55:24.267Z — build-wait-reviewer

Occurrence 2026-11-27T16:18:04.967Z (wait issue-4aacd4d05363; attempt game-dll-20261127T161805119364Z-b52361, generation 795, run 20261127_155356): evidence.reason is again a mid-JSON tail, `5921.888,"build_attempt_id":"game-dll-20261127T161805119364Z-b52361","source_generation":32,"generation":795,...`, the last 240 chars of a [GAME_FILE] worker row. Source unchanged: devscripts/dev-loop.py:3112 `detail = line.rstrip()[-240:]`, assigned at :3142, read at :3892 `reason=self.game_build_detail`. The same record also carries stale identity: evidence.build_id=2026-11-27T08:12:40.178185Z (EXE artifact via self.latest_build) and generation=794 while the in-flight attempt was gen 795; no run_id. The attempt itself succeeded in 107.063s (exit 0, gen 795). Journal logs/2026-11-27/20261127_155356/dev-loop/events.jsonl line 81. No duplicate issue; no fix claimed.

## Occurrence

### 2026-11-27T18:31:51.292Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-27T17:05:04.518Z (issue-4ddfb305b25b; attempt game-dll-20261127T170504574022Z-3ad5f1; run 20261127_170216). evidence.reason is again a mid-JSON tail: `30747.545,"build_attempt_id":"game-dll-20261127T170504574022Z-3ad5f1",...` - the last 240 chars of a worker row, not a readable stage summary. Source unchanged: devscripts/dev-loop.py:3112 `detail = line.rstrip()[-240:]`, assigned at dev-loop.py:3142, read at dev-loop.py:3893 as reason. The waited-on attempt succeeded as generation 799 in 700.557s (server-gamemode.cpp 622.768s), so the raw tail is the only defect here. No duplicate issue filed.

Evidence: {"agent": "build-wait-reviewer", "build_attempt_id": "game-dll-20261127T170504574022Z-3ad5f1", "generation": 799, "issue": "issue-4ddfb305b25b", "reason_prefix": "30747.545,\"build_attempt_id\":\"game-dll-20261127T170504574022Z-3ad5f1\"", "source": "devscripts/dev-loop.py:3112,3142,3893", "total_seconds": 700.557, "wait_id": "game-dll-build:2026-11-27T17:05:04.518Z"}

## Agent suggestion

### 2026-11-28T08:28:34.351Z — build-wait-reviewer

Occurrence 2026-11-28 (build-wait-reviewer, wake-63b15223f7f1, review on issue-30a5dbfd79be).
Wait game-dll-build:2026-11-28T07:11:57.814Z still stores a raw truncated reason: ':57406.0,"build_attempt_id":"game-dll-20261128T071157863196Z-8d63e6","source_generation":1,"generation":810,"revision":"e7af1bad...","progress_write_ms":6.498,"record_overhead_ms":7.142}'. It embeds the real attempt id/generation but is not a readable stage summary. Source unchanged: dev-loop.py:3130 `detail = line.rstrip()[-240:]`, assigned at :3160, read for reason at :3912. Same defect as prior occurrences; no duplicate issue filed.

## Occurrence

### 2026-11-28T08:52:01.001Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-28T07:56:14.606Z (issue-d7dc1872d630; attempt game-dll-20261128T075614669320Z-cb02af; run 20261128_061051). evidence.reason is again a mid-JSON tail: `:60156.0,"build_attempt_id":"game-dll-20261128T075614669320Z-cb02af","source_generation":23,"generation":814,...` - the last 240 chars of a [GAME_FILE] worker row, not a readable stage summary. Source unchanged: devscripts/dev-loop.py:3130 `detail = line.rstrip()[-240:]`, assigned at :3160, read at :3912 `reason=self.game_build_detail or self.last_message`. The waited-on attempt succeeded as generation 814 in 404.234s, so the raw tail is the only defect here. No duplicate issue filed.

Evidence: {"agent": "build-wait-reviewer", "build_attempt_id": "game-dll-20261128T075614669320Z-cb02af", "generation": 814, "issue": "issue-d7dc1872d630", "journal": "logs/2026-11-28/20261128_061051/dev-loop/events.jsonl", "journal_line": 629, "over_budget_wall_time": "2026-11-28T07:57:37.473Z", "reason_prefix": "60156.0,\"build_attempt_id\":\"game-dll-20261128T075614669320Z-cb02af\"", "source": "devscripts/dev-loop.py:3130,3160,3912", "total_seconds": 404.234, "wait_id": "game-dll-build:2026-11-28T07:56:14.606Z"}

## Agent suggestion

### 2026-11-28T17:56:21.542Z — build-wait-reviewer

Occurrence 2026-11-28T17:34:21.168Z (review issue-6ed1b2897c5b, attempt game-dll-20261128T173421264252Z-3b08fd): evidence.reason is again the last 240 chars of a [GAME_FILE]/[GAME_STAGE] progress JSON row, starting ':58250.0,"build_attempt_id":...' and ending with progress_write_ms/record_overhead_ms. Source unchanged: dev-loop.py:3112 detail=line.rstrip()[-240:], assigned :3142, read at :3912 reason=self.game_build_detail or self.last_message. Makes the wait reason unreadable even though the attempt id is embedded. Journal logs/2026-11-28/20261128_171225/dev-loop/events.jsonl line 136. No duplicate issue filed.
