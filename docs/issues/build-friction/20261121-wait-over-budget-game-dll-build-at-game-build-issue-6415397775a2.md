# issue-6415397775a2: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Agent suggestion

### 2026-11-21T03:26:27.455Z — build-wait-reviewer

Bounded wake observation: the next attempt after this wait also failed, and the failure is a slow changed-source preflight, not codegen. Dev status at activation: lifecycle=game_build_failed, detail='changed-source preflight failed: src/network/multiplayer-projectiles.cpp in 1240.728s', attempt=game-dll-20261121T022901426487Z-d15759, latest published build=2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871. Evidence suggests the 60s wait budget fires long before preflight finishes (~20.7 min here), so reviewer wakes are expected and not by themselves proof of a wedged builder. Suggested next evidence: record per-attempt preflight duration plus the source hash/paths that changed, and check whether concurrent agent edits to src/network/multiplayer-projectiles.cpp during preflight are the intended trigger. No edits or build requests made by this activation.
