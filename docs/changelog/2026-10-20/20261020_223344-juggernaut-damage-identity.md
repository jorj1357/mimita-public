# Task

- Task ID: juggernaut-damage-identity
- Summary: Explain and correct NPC-to-player Juggernaut damage attribution; record the requirement in the relevant specs.
- Status: BLOCKED
- Date, time, timezone: 2026-10-20T22:33:44Z, UTC
- Branch: current checkout (branch name not captured)
- Base commit: not captured
- Final commit: not committed

## Pre-existing changes

- The checkout was already dirty before this work. The existing Juggernaut and networking spec edits were preserved; this session only appended the new sections described below.
- Initial dirty paths included account and presentation configuration, `docs/jorj-docs/20261020plan.md`, `docs/specs/npc-behavior/npc-behavior.md`, actor-collision configuration/code, movement/collision and ragdoll code, and earlier changelogs/specs.
- More paths appeared modified during the session (`src/network/multiplayer-reconcile.cpp`, `src/network/packets.h`, `src/network/server-players.cpp`, `src/network/server.h`, and `src/sim/simulate-tick.cpp`). They were not edited by this work and were left untouched.

## Requested behavior

Juggernaut hits should visibly damage Fighters. Clarify the cause in plain language and record the requirement in the specs that own the mode and network damage behavior. Keep invisible avatars and missing shot visuals as separate symptoms until their render/send paths are proven.

## Specification alignment

- `docs/specs/gamemodes/juggernaut.md`: the mode reuses shared actor lifecycle, weapons, damage, NPC, and networking systems. Added the explicit requirement that the NPC ID, kind, and team survive friendly-fire checks.
- `docs/specs/networking/networking.md`: the server owns confirmed damage. Added the actor identity/team contract and required structured fields.
- `docs/specs/weapons/weapons.md`: existing candidate filtering already blocks teammates when friendly damage is disabled; this fix preserves that policy.
- Focused review: `docs/skills/spec-behavior-review-v1.md` — PASS_WITH_HUMAN_REVIEW; `docs/skills/logging-checker-v1.md` — current damage events identify the rejection, and the new fields identify actor kind/team.
- Finding: spec-to-code disagreement. The requested shared actor behavior existed as a goal, but NPC attackers were rejected by the typed damage owner and the Juggernaut path instead called the player-only wrapper with ID zero.

## Exact implementation changes

### `src/network/server-npcs.cpp`

- `simulateSharedNpcs`, around lines 1351-1362.
- Before: `applyServerDamage(players, *nearestPlayer, 0, damage, knockback, ServerDamageSource::Hitscan)`.
- After: construct a `ServerActorDamageRequest` with `n.id` and `ServerActorKind::Npc`, then call `applyActorDamage(players, npcs, damageRequest)`.
- This preserves the real actor identity and retains the existing confirmed NPC damage packet/kill attribution call.

### `src/network/server-damage.cpp`

- `applyActorDamage`, around lines 278-337: accepts a present NPC attacker for a player victim; resolves its team, with the match roster as fallback; keeps the existing team equality check.
- `applyPlayerDamageLegacy`, around lines 71-224: accepts resolved actor team/kind, emits actor ID/type/team and target type/team on applied and rejected events, and avoids counting NPC damage as a Zombie Tower player stat.
- Before: `phase1-player-attacker-required` rejected every typed NPC attacker.
- After: actor lookup accepts either Player or NPC and passes the resolved team to the shared player-victim health logic.

## Diagnostics and source evidence

- Owner/category: `applyPlayerDamageLegacy`, StructuredLogger `Network` events `server.damage.rejected` and `server.damage.applied`.
- Existing journal: `logs/2026-10-20/20261020_221119/events.jsonl`.
- It contains 775 `server.damage.rejected` records for hitscan damage to actor 1 with `attacker_id=0`, `attacker_team=0`, `target_team=0`, reason `same-team damage blocked`.
- The call in `server-npcs.cpp` used attacker ID 0; team-mode rosters include team assignments for NPCs. This is the first confirmed divergence. No matching `npc.damage` event was found in that journal.
- Avatar: actor 130 logged a fallback binding while `avatar_async_pending`; this does not prove it was visually invisible.
- Shot visuals: the journal does not provide enough correlated sender/receive/render evidence to assign the missing bullet visuals to a stage.

## Validation

- Build command: `python devscripts/dev-loop.py --profile npc-navigation --request build --wait-for-build --build-mode debug --build-timeout 36000 --json`.
- Result: failed before publishing. Compiler reported the pre-existing error `src/sim/simulate-tick.cpp:317:48: error: 'const struct ActorCollisionData' has no member named 'profileFor'; did you mean 'profiles'?`. That file was already part of unrelated in-progress actor-collision work and was not changed here.
- The existing run/client was left running; no new executable was launched and no live gameplay scenario was run. Source change, build, live behavior, and visual acceptance are separate claims; runtime fix remains unverified.
- No test was added or run.

## Regression review

- Regression added: `docs/regressions/2026-10-20/npc-damage-loses-team-identity-REG.md`.
- Status remains `ATTEMPTED FIX (1)` until a fresh Juggernaut round proves enemy damage applies and same-team damage is rejected.

## Human acceptance

- Still required: observe enemy Juggernauts reduce player health in a real round and verify friendly fire remains blocked.
- Still separate: confirm the avatar is visible and correlate each shot's server send, client receive, and rendered muzzle/tracer.
