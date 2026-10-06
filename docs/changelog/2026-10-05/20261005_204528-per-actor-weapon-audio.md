# Per-actor weapon sound retriggering

- EST timestamp: 2026-10-05 20:45:28 -04:00.
- Branch: `afad20a-rebuild`.
- Result: `PASS_WITH_HUMAN_REVIEW`.
- Pre-existing worktree changes were preserved, including the earlier `config/weapons.json` pitch/retrigger edit, unrelated config/source/test edits, `docs/changelog/20261005_201900-dev-loop-server-ownership.md`, and the untracked `docs/changelog/2026-10-06/` directory.

## Final behavior

Retrigger ownership is now scoped to actor plus sound channel. Ten NPCs can fire shotguns simultaneously without stopping one another's sounds; repeated shotgun or revolver shots from the same actor retrigger that actor's own channel. Shotgun and revolver remain separate channels for the same actor.

## Changed owners and paths

- `src/audio/audio.h:52-55` and `src/audio/audio.cpp:385-406` add the shared per-actor/channel owner-key and owned-world-sound helpers.
- `src/combat/weapon-audio.h:11-12` and `src/combat/weapon-audio.cpp:15-38` accept an actor ID, resolve the local network actor when appropriate, and derive retrigger ownership from actor ID plus weapon ID.
- `src/npc/npc-combat.cpp:393-394` and `src/npc/npc-combat.cpp:430-431` pass each NPC's namespaced actor ID for shotgun and revolver playback. The shared hitscan helper is disabled for those explicit NPC sound calls to prevent duplicate playback.
- `src/network/multiplayer-shots.cpp:622-623` scopes replicated shotgun blast audio by shooter ID.
- `src/engine/engine-tick-net.cpp:638-648` scopes replicated shot audio by shooter ID and preserves normal revolver pitch `0.9` and normal shotgun pitch `0.8`.

## Validation

- `python build_agent.py` completed with `BUILD SUCCESS`, return code `0`; final pass compiled 3 files and skipped 524 unchanged files.
- The build linked `mimita.exe` and staged runtime DLLs successfully.
- `git diff --check` found no new whitespace error in this change. It reported the pre-existing trailing-space warning at `config/behavior-profiles.json:185`.
- Source tracing confirms retrigger still calls `AudioManager::stopOwner`, but the owner key is no longer weapon-global.

## Focused review

- Followed `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/weapons/weapons.md`, `docs/architecture/live-development/hot-audio-contract.md`, `docs/features/live-code-development/live-code-development.md`, `docs/operations/asset-management/asset-management.md`, and `docs/operations/task-completion/task-completion.md`.
- `docs/skills/spec-behavior-review-v1.md`: PASS for the requested behavior; implementation ownership and call chains were traced.
- `docs/skills/asset-checker-v1.md`: PASS; no sound asset paths changed.

## Human review still needed

Run a live scene with multiple NPCs firing shotguns and revolvers. Confirm different actors' sounds overlap independently, while rapid repeated shots by one actor retrigger only that actor's sound. Build evidence does not prove the audible result.
