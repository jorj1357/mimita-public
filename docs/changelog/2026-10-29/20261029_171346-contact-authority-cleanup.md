# Contact Authority Documentation, Quality Gates, and Inert Deletion

Time created: `2026-10-29T17:13:46Z`
Time last updated: `2026-10-30T00:22:00Z`

## Summary

Defined the target contact/damage authority contract, added repository quality
tooling and the actor/weapon/network matrix contract, created the regression
records, and deleted confirmed inert legacy melee/projectile networking code.

This pass stopped after the safe deletion commit. The shared contact pipeline and
behavior fixes are not implemented yet.

## Documentation

Added:

- `docs/specs/networking/contact-authority.md`
- `docs/specs/dev-loop/quality-gates.md`
- `docs/regressions/2026-10-29/client-prediction-server-authority-REG.md`
- `docs/regressions/2026-10-29/v2.0.7-actor-matrix-coverage-REG.md`
- `devscripts/scenarios/matrix/actor-weapon-matrix.json`
- `devscripts/scenarios/matrix/README.md`

Updated:

- `docs/specs/networking/networking.md`
- `docs/architecture/code-ownership/code-ownership.md`
- `docs/specs/weapons/melee/melee-weapons.md`
- `docs/specs/weapons/weapons.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/specs/dev-loop/scenario-input-recording.md`
- `docs/specs/dev-loop/dev-loop.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/ROUTER.md`

The new spec defines one geometry query -> one `ContactCandidate` -> one shared
resolver -> `applyActorDamage`. It also records the future arbitrary-GLB
projectile and model-triangle collision roadmap, while keeping spheres, capsules,
and authored melee boxes as the current implementation.

## Tooling

Added report-only tools under `devscripts/quality/`:

- `lint.py`
- `check_damage_authority.py`
- `check_contact_ownership.py`
- `dead_code_report.py`
- `duplicate_report.py`
- `coverage_report.py`
- `README.md`

Added `devscripts/dev-loop-matrix.py` and the matrix contract. The full
one-server/N-client launcher is not implemented yet; the current tool validates
and prints the required matrix.

Baseline report on the current tree:

- damage-authority findings: `91`
- weapon-ID branch findings: `40`
- duplicate function-name findings: `4`
- TODO-DELETE / `#if 0` markers: `9`
- repeated normalized windows: `479`
- optional C++ tools not installed: `clang-format`, `clang-tidy`, `cppcheck`,
  `jscpd`
- coverage artifacts: none

These are report-only. They expose the work for the next phase; they are not
acceptance failures.

## Deletion

Removed:

- `src/network/server-melee.cpp`
- `handleMeleeHitRequest`
- `tickServerSwordCombat`
- `computeBladeCapsule`
- `computeHitDamage`
- local `sweptSphereOverlap`
- `mpSendMeleeHitRequest`
- `MeleeHitRequestPacket`
- `PACKET_MELEE_HIT_REQUEST`
- unreachable `startServerSwordAttack` branch and helper
- `mpSendProjectileFireRequest`
- `handleProjectileFireRequest`
- `ProjectileFireRequestPacket`
- `PACKET_PROJECTILE_FIRE_REQUEST`
- `projectileConfig(uint8_t)` and its `#if 0` block
- unused `ServerPlayer` fields: `lastMeleeAttackSerial`, `swordswordState`,
  `meleeCooldownTimer`
- unused client `nextLocalMeleeAttackSerial`
- duplicate melee claim flush gate in `melee-client.cpp`

A swordsword weapon without a move set now receives an explicit rejection
instead of the removed fallback timeline.

## Evidence

Source and docs:

- docs/tooling commit: `413d701` (`docs: define contact authority and quality gates`)
- deletion commit: `6c488994` (`refactor: remove inert legacy contact networking`)

Build:

- dev-loop build `3716` succeeded after deletion

Runtime:

- `command_proof_smoke_v1` passed on run `20261029_172617`
- `melee_combat_v1` produced `melee.action.start` for the move-set attack but
  failed its `melee.impulse` assertion; this is not accepted as deletion proof
- a temporary mace smoke did not reach attack because `equipweapon mace`
  produced `weapon.equip.rejected` for the selected weapon set; the temporary
  scenario was removed

Human acceptance:

- none claimed for this pass

## Remaining Work

- commit the deletion
- stop for review
- implement the shared contact pipeline
- add full one-server/N-client matrix launching
- make quality gates blocking after the baseline is clean
- update the regression record with fix evidence


## Follow-up implementation pass

Implemented the first correctness fixes after the deletion stop:

- `AttackRequestPacket` carries `movesetHash`.
- Client sends `MeleeMoveSet::hash`; server compares content hash and logs
  `melee.attack.hash_mismatch` instead of rejecting process-local revisions.
- Melee claims carry `targetGeneration`.
- Melee claims use the per-target rendered server tick.
- Client prediction deduplicates per target/tick.
- Server claim validation uses the claimed hit point and OBB against a lenient
  multi-height target volume; reach is computed from the claimed hit point.
- Added `combat.max_authoritative_damage` with a huge default
  (`999999999999999`) and removed the hardcoded `1..500` damage sink clamp.
- Added the RTT/accuracy targets, late-contact policy, no-offline rule, and
  dev-loop package split to the specs.

Evidence:

- build `3718` succeeded
- `command_proof_smoke_v1` passed
- weapon-set-29 mace smoke passed with `melee.action.start` and accepted
  `weapon.attack.result`
- no `melee.attack.hash_mismatch` or revision-mismatch event in that run
- contact-claim smoke did not reach an NPC and is not accepted as contact proof

Not yet implemented:

- client team/friendly-fire prediction parity
- shared melee damage/knockback resolver
- shared contact pipeline for hitscan/projectile/deathball
- full N-client launcher and matrix execution
- server-view overlay
- human PvP acceptance
## Shared melee formula and client team parity

Added shared melee HP damage/knockback helpers in `WeaponExecution` and routed
the client prediction, server claim, and server animated-contact paths through
them. This removes the `force_scale`/`damage_scale` drift between those paths.

Client prediction now skips same-team targets when the active gamemode has
friendly fire disabled. This is prediction parity only; the server remains the
final authority.

Build `3722` succeeded. Weapon-set-29 mace smoke passed with `melee.action.start`
and accepted `weapon.attack.result`; no hash/revision mismatch was emitted.

Still pending:

- shared contact pipeline for hitscan/projectile/deathball
- full N-client matrix runner
- server-view overlay
- PvP and bad-connection accuracy measurement
## Initial empty hands and checked-in smoke scenarios

- Player boot now starts at equipped slot `0`; the saved hotbar slot is no
  longer applied at boot.
- Server initial spawn no longer applies a role starting weapon; it publishes an
  empty equipped weapon id and slot `0`. Respawns still preserve an owned
  previous item or fall back to the role starting weapon.
- Added `devscripts/scenarios/melee_initial_empty_hands_v1.json`.
- Added `devscripts/scenarios/melee_attack_identity_v1.json`.
- Added a note to `docs/specs/open-gameplay-issues.md` requesting instant
  animation snapping as an explicit optional animation mode in normal gameplay.

Runtime evidence:

- `melee_initial_empty_hands_v1` passed on run `20261029_182302`
- `melee_attack_identity_v1` passed on run `20261029_182324`
- build `3724` succeeded
## Shared server contact resolver, matrix runner, and server view

Added `src/network/server-contact.h/.cpp` with
`resolveServerContact(...)`, which centralizes victim lookup,
generation/lifecycle checks, friendly-fire policy, and `applyActorDamage`.

Migrated:

- hitscan player and NPC damage application
- projectile-rifle direct player and NPC damage application
- deathball player/NPC tick contacts

Added the runnable matrix path to `devscripts/dev-loop-matrix.py`:

- reuses or restarts the dev-loop server when its build is stale
- stops the dev-loop's single client
- launches N clients with unique journals and command queues
- executes per-client steps and assertions
- writes `matrix-run.json`
- leaves the server running for the next case

Added `server_view [0|1]` to toggle the authoritative-position debug overlay and
suppress local predicted/replica capsules while enabled.

Evidence:

- build `3732` succeeded
- `melee_initial_empty_hands_v1` passed
- `melee_attack_identity_v1` passed
- two-client matrix example passed with `assertions=2 failed=0`
- `server_view 1` command delivered and completed

Still pending:

- projectile explosion splash migration
- godball claim migration
- full geometry validation sharing for every family
- PvP/bad-connection accuracy measurement
## Explosion, godball, and shared geometry validation

Migrated the remaining damage families to `resolveServerContact`:

- rocket/grenade explosion splash for players and NPCs
- legacy godball hit claims, with damage recomputed server-side from the claimed
  ball speed and the godball definition
- generic melee claims
- physical-contact melee contacts
- spy-knife contacts

`ServerContactRequest` now carries the shared geometry contract:

- contact kind and authoritative contact point
- contact tick with an explicit age window
- reach limit and hit-volume tolerance
- body-part box/capsule validation through one shared owner
- optional world line-of-sight validation
- bounded duplicate suppression keyed by attacker/victim/kind/tick/contact ID

Removed the duplicate explosion splash LOS/body-point helpers from
`server-projectiles.cpp`; explosion now uses the shared body-volume and
occlusion owner. Removed direct NPC health/knockback writes from the explosion,
godball, and spy-knife paths.

Added `devscripts/scenarios/explosion_smoke_v1.json`.

Evidence:

- build `3742` succeeded
- `explosion_smoke_v1` passed (`assertions=2`, run `20261029_185934`)
- `melee_attack_identity_v1` passed (`assertions=2`, run `20261029_185953`)
- explosion journal run `20261029_185934` contains
  `server.damage.applied` with `source=rocket_explosion`
- melee identity journal run `20261029_185953` passed

Still not runtime-proven:

- NPC-target explosion splash (the smoke scenario exercises self splash)
- godball claim acceptance against a live remote player/NPC
- PvP and bad-connection accuracy matrix
## Melee claim phase alignment, diagnostics, and shockwave PNG config

Investigated the recorded scenario
`devscripts/scenarios/user/scenario-20261029_195848-meleetest.jsonl`.

Recorded-track analysis (client vs server actor health, index-aligned because the
client track's `server_tick` field is a constant):

- client damage events `93`, server damage events `103`
- client predicted damage `960`, server applied damage `900` (6.25% excess)
- client death transitions `7`, server respawns `14`
- per-NPC HP mismatch ticks ranged from `0` to `46`

The first divergence is that the server recomputed melee action phase from
`pkt.contactTick - attacker.meleeActionStartTick`, while the client used its own
local action phase. The server action starts when the request arrives, so those
ticks are not the same domain. That made `actionActive`, action min/max damage,
and rev multipliers disagree near phase boundaries.

Changes:

- `SpyKnifeContact` now carries `actionElapsedTicks`.
- The client records the elapsed client action phase for each melee claim.
- The server uses that elapsed phase for `actionActive`, min/max damage, and
  rev decay instead of mixing client render ticks with server start ticks.
- Added `melee.contact.claim.accepted` fields for client/server action elapsed,
  action active, force, damage multiplier, rev multiplier, and health before/after.
- The client records melee NPC predictions in `predictedNpcDamage` /
  `predictedNpcHitMs` so the existing mismatch disagreement path can report
  server/client damage differences.
- Client `melee.client.contact_claim` now logs action elapsed and action-active.

Added client prediction journal events:

- `net.prediction.damage.applied`
- `net.prediction.damage.confirmed`
- `net.prediction.damage.rollback`

Playback attempt:

- The original scenario rejected at `weapon_set_mismatch` until the server was
  launched with weapon set `29`.
- A generation-adjusted copy reached generation readiness but failed
  `start_readiness_timeout`: the recorded start position did not settle on the
  current server, so no frames played. The analysis above uses the recorded
  client/server actor tracks directly.

## Rocket shockwave PNG and animation config

Added per-weapon explosion shockwave fields:

- `shockwaveTexture` custom relative PNG path
- `shockwaveStartRotationDeg` / `shockwaveEndRotationDeg`
- `shockwaveStartScale` / `shockwaveEndScale`
- `shockwaveStartAlpha` / `shockwaveEndAlpha`
- `shockwaveRandomRotationMinDeg` / `shockwaveRandomRotationMaxDeg`

PNG shockwaves render as a surface-aligned textured quad and take precedence over
`shockwaveModel`. Random rotation chooses independent start and end angles per
spawn. Added one bounded `explosion.shockwave.spawn` journal event per explosion.

Rocket config now has the new fields with the existing model preserved and
`shockwaveTexture` empty by default.

Evidence:

- build `3755` succeeded
- `explosion_smoke_v1` passed; journal run `20261029_205418` contains
  `explosion.shockwave.spawn` with randomized start/end rotation, start/end
  scale `1.0 -> 2.5`, alpha `1.0 -> 0.0`, and lifetime `0.18`
- `melee_attack_identity_v1` passed on build `3754`, run `20261029_205437`

Still required:

- live melee claim acceptance measurement after the phase alignment change
- NPC-target melee probe that actually connects (the available probe was launched
  into the air by the NPC before ground punches could connect)
- visual acceptance of the PNG shockwave
## Melee future-contact rejection fix

The new recorded scenario
`devscripts/scenarios/user/scenario-20261029_205916-meleeagain.jsonl` has client
tracks only (`tracks_complete=false`, no `server_actors`), so it cannot prove
server acceptance directly. Its client lifecycle shows `13` death transitions
and `20` respawns, with `1527` client-predicted damage across the NPCs.

A live mace probe on `cramped` reproduced the real bug. The server received valid
mace claims but rejected every one with `invalid_contact`. Example:

- client `contact_tick=975`
- server receive `tick=926`
- delta `+49` ticks

The client render/interpolation clock was ahead of the newest server snapshot.
The server's melee validation rejected any `contactTick > tick`. The Spy Knife
path already clamped this; the generic melee path did not.

Changes:

- `melee-client.cpp` now clamps the claim contact tick to
  `latestServerTick`/`latestLocalSnapshotTick`, matching the Spy Knife path.
- The server generic melee claim path allows up to `120` future ticks and
  evaluates the pose at `min(claimTick, serverTick)`.
- Claims older than `600` ticks still reject.
- Rejections now distinguish `future_contact`, `stale_contact`, and
  `invalid_contact`, and include `server_tick` and `delta_ticks`.
- The final melee rejection log now reports the shared resolver reason instead
  of the empty damage-sink reason.
- Added client batch flush and server batch receive events.

Evidence:

- build `3762` succeeded
- `melee_mace_probe_v1` passed, run `20261029_211825`
- `melee_attack_identity_v1` passed, run `20261029_212033`
- that run had `melee.client.contact_claim=1`, `melee.contact.claim.accepted=1`,
  and the matching `server.damage.applied` with `source=physical_contact`
- the same run shows `net.prediction.damage.confirmed` for the NPC

Still required:

- a longer melee scenario across mace/chainsaw/sword to measure the full
  acceptance percentage
- playback of the client-only recording after adding a server actor track or a
  start-state compatibility path
## Automatic server track recording

The current recorder already starts a server authoritative sidecar whenever a
networked `scenario_startrecord` begins. Verified with a fresh recording:

- header tracks: `client_input`, `client_actors`, `server_actors`
- footer: `tracks_complete=true`
- server frames captured: `151`

`scenario_endrecord` now waits up to 6 seconds for the server sidecar to finish
and merge, then reports either `recording stopped and merged` or the exact merge
error. This prevents the previous failure mode where closing the game immediately
after `scenario_endrecord` left a client-only file.

Evidence:

- build `3766` succeeded
- fresh recording `scenario-20261029_213848-endwaitcheck.jsonl` merged in `0.6s`
  with `tracks_complete=true`
- journal contains `scenario.record.merged`

The supplied `scenario-20261029_205916-meleeagain.jsonl` cannot be retrofitted:
it was recorded without a server track (`tracks_complete=false`). It must be
re-recorded with the current build. The live mace probe already showed the fixed
path accepting a claim and applying `server.damage.applied`.
## Melee action tail and current accuracy measurement

Analyzed the server-inclusive recording
`scenario-20261029_214442-meleea133.jsonl`.

Current measured result before the action-tail fix:

- client damage events `154`, server `169`
- client predicted damage `2398`, server applied `2199` (9.05% excess)
- client death transitions `15`, server respawns `24`
- player client action-active ticks `1216`, server `665`
- client action starts `156`, server action starts `156`
- client-active/server-inactive ticks `808`
- server-active/client-inactive ticks `257`
- action-ID mismatch while both active: `0`

The first action divergence was client active on `rev` while the server action
ID was already empty. The server action timeline had ended, so late client claims
fell back to base damage instead of the authored action profile.

Fix:

- `ServerPlayer` retains `lastMeleeActionId` after the action timeline ends.
- The claim handler uses the current action ID while active, otherwise the last
  started action ID.
- This preserves the authored min/max damage and rev multiplier for late claims.

Playback of the recording is still blocked before frames begin:
`start_readiness_timeout`. Diagnostics now show the teleport target and actual
client/server position. The recorded start is near live NPCs and terrain, so the
player is pushed several meters before readiness; the recording itself is used
for track analysis rather than frame playback.

Evidence:

- build `3770` succeeded
- recorded analysis above
- live mace claim acceptance remains `1/1` from the earlier probe

Still required:

- a stable replay start or collision-free playback setup
- a post-fix recording to measure the full 99.99% target across mace/chainsaw/
  swordsword
## Melee percentage measurement and stale-life suppression

Measured `scenario-20261029_220304-meleedmgs.jsonl` (server-inclusive,
`tracks_complete=true`).

Pre-fix recorded result:

- client damage events `200`, server `211`
- client predicted damage `2514`, server applied `2356`
- client event matched to server damage within +/-3 ticks: `197/200 = 98.5%`
- within +/-10 ticks: `198/200 = 99.0%`
- within +/-60 ticks: `199/200 = 99.5%`
- melee totals: sword `1120 -> 1041`, chainsaw `556 -> 536`, mace `5 -> 5`
- player action starts `105` client, `116` server
- client-only active ticks `3157`, server-only active `55`

The one remaining unmatched event at +/-60 was a stale-life prediction:
client saw NPC `200005` at health `17` and epoch `2`, predicted a lethal `17`,
while the server had already respawned it at health `100` and epoch `3`.

Fix:

- `mpApplyPredictedDamage` now skips prediction when the interpolation target's
  authoritative generation/epoch is newer than the rendered replica.
- Logs `net.prediction.damage.skipped_stale_life`.
- Melee prediction does not emit hit effects or send a claim when prediction was
  enabled but skipped for a stale target life.
- `lastMeleeActionId` retains the authored action profile for late claims.

Modeled post-fix result on the same recording:

- `11` stale-life predictions are excluded
- `189` non-stale predictions remain
- `189/189 = 100%` have matching server damage within +/-10 ticks

Evidence:

- build `3772` succeeded
- recording analysis above

Still required:

- a fresh recording made with build `3772` to confirm the modeled 100% result at
  runtime and to verify exact per-contact damage, not only event presence
## Melee action start, target pose, and confirmed-only feedback

Analyzed `scenario-20261029_225323-meleelaaae.jsonl` and reproduced the
"melee action could not start" path.

Root causes:

- `serverMeleeStartAction` rejected animation-only actions when
  `tick < meleeNextAllowedTick`, even though the client owns the action cadence.
- The failure branch emitted a disagreement but did not send an AttackResult.
- Client melee geometry used the current rendered target position while sending
  a different contact tick; server rewound to that tick and rejected the claim
  as `outside_claim_radius` (observed ~10 m difference).
- The claim did not carry the client action identity, so the server could use a
  different current/retained action profile.
- Speculative melee damage/hitmarker feedback was shown before confirmation.

Changes:

- Animation-only melee actions bypass the server cooldown gate.
- The failed-start branch now sends a rejected AttackResult.
- `SpyKnifeContact` carries `actionIndex`; the server uses the client action
  profile directly.
- Client melee claim geometry samples the target interpolation buffer at the
  claimed contact tick and shifts body-part boxes to that pose.
- Melee HP, hitmarker, blood, and damage numbers are now server-confirmed only.
  The client still sends claims immediately; no speculative melee damage is
  displayed.
- Existing stale-life prediction guard and retained last action ID remain.

Evidence:

- build `3778` succeeded
- the mace probe journal shows the new `feedback=server_confirmed` claim field
- `melee_attack_identity_v1` passed on build `3772`, run `20261029_221320`

The supplied recording predates these changes and playback still fails before
frames due to start-position/NPC-collision readiness. A fresh recording on build
`3778` is required to measure the final acceptance percentage. By construction,
every displayed melee damage event is now server-authoritative.
## Per-weapon right-arm ragdoll melee mode (2026-10-30)

Implemented the requested per-weapon right-arm physical melee mode.

- Added `ArmRagdollConfig` to `WeaponDefinition` and parsed
  `components.melee.arm_ragdoll` from each weapon config.
- Enabled it on mace, chainsaw, and swordsword; spyknife remains unchanged.
- Added a right-arm-only path to `RagdollModeSystem`: only `rightArm` is
  dynamic, other limbs are pinned to the current animation pose, and only the
  right-arm node is written back.
- LMB released applies weak camera-forward torque; LMB held adds the configured
  torque. No tap duration is hardcoded; momentum is emergent.
- Fixed melee action start is bypassed for these weapons. Continuous weapon
  contact claims and the shared server damage resolver remain in use.
- Chainsaw LMB hold drives rev decay through the existing rev multiplier path.
- Added `RAGDOLL_NET_ARM_RAGDOLL`; remote clients apply only the right arm, and
  the server merges the replicated arm with animated body boxes for contact.
- Added bounded `armragdoll.activate` and `armragdoll.motor` diagnostics.
- Added `agent.holdprimary <frames>` as a dev/scenario input hook and
  `arm_ragdoll_probe_v1.json`; updated `melee_mace_probe_v1.json` for the new
  mode and moved `melee_attack_identity_v1.json` to the non-arm-ragdoll
  Captain Falcon move set.

Validation evidence:

- build `3800` succeeded
- `arm_ragdoll_probe_v1` passed on run `20261030_003731`
- `melee_attack_identity_v1` passed on run `20261030_003845`
- `melee_mace_probe_v1` passed on run `20261030_003822`
- runtime motor samples show `primary_held=true`, arm speed reaching the
  configured cap, and the arm settling after release.

Still requires human visual acceptance for feel, swing weight, and weapon
collision against walls. Remote two-client visual verification remains pending.
Skill review: `docs/skills/spec-behavior-review-v1.md` applied. The requested
behavior is a new opt-in mode; no existing weapon was silently converted except
the three explicitly enabled definitions. No unresolved spec-code conflict was
found. Human visual acceptance and a remote two-client run remain open.
