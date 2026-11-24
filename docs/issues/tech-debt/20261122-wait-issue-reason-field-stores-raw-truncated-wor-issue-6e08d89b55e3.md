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
