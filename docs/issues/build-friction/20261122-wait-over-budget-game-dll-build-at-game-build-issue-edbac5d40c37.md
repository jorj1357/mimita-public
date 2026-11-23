# issue-edbac5d40c37: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Agent suggestion

### 2026-11-22T16:31:00.980Z — fcs-npc-speed-combat-investigation

Additional evidence from FCS measurement attempt: root DLL generation 502 failed twice at 2026-11-22T16:24:45Z and 16:27:20Z in devscripts/game_build_worker.py:1348 with UnboundLocalError: local variable 'progress' referenced before assignment. Traceback in logs/2026-11-22/20261122_144609_01/dev-loop/events.jsonl under dev-loop.game-build.failed; request auto-8 and concurrent game-dll request accepted 16:28:40Z. Measurement source changes are queued but no successful DLL/runtime proof yet. The active build-efficiency owner controls game_build_worker.py; do not overlap edits.

## Agent discussion and suggestions

### 2026-11-22T16:56:10.182Z — fcs-npc-speed-combat-investigation

Message: `msg-8168673acff9` (evidence)

Subject: Generation 503 compiler wait blocks FCS metric runtime proof

The root dev-loop candidate game-dll-20261122T163256885716Z-84a726 has not advanced its JSONL build timings since both NPC TUs started at 16:40:22Z (server-npcs.cpp, npc.cpp). At 16:50:48Z, 10 g++.exe processes started together; the worker pythonw PID 28264 showed 10.08 CPU seconds and 5.3 MB working set at 16:54Z. No translation_unit.completed event exists yet. The candidate includes our NPC metric paths in generation 503 and the build note 38ac9e35cb8e. I am not editing build_worker.py; please check if this overlaps your build latency investigation or if an existing diagnosis explains the idle compiler fanout. The shared Juggernaut runtime remains running.

## Agent suggestion

### 2026-11-22T17:34:33.707Z — fcs-npc-speed-combat-investigation

Further evidence, same shared game build delay: candidate game-dll-20261122T163256885716Z-84a726 generation 503, source generation 9, has run for over 45 minutes. Its timing JSONL shows 138 translation_unit.started and 28 completed as of ~17:20Z; completed TUs include npc.cpp (589.5s), server-npcs.cpp (621.8s), combat weapon files (280–689s), devtools/debug files (551–724s). The worker submits 10 compiler jobs but the journal has queued source entries with long queue_wait values. This attempt is still progressing but does not yet have DLL publication. FCS runtime proof is blocked pending finish.
