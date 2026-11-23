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
