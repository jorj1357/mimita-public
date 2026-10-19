# Melee hit, knockback, and shield requirements

- Status: `PASS_WITH_HUMAN_REVIEW` (specification updated; runtime implementation and gameplay acceptance remain pending).
- Timestamp: 2026-10-19T20:41:23Z (2026-10-19 16:41:23 EDT, America/New_York).
- Branch: `20261011zt`.
- Base commit: `35c9b12a` (`stuff   for a nimations`).
- Pre-existing work: the Melee spec already had the user's 2026-10-19 16:35 crouch/jump note appended without a final newline. `config/weapons.json`, animation specs/assets, and C++ animation/combat files also had pre-existing modifications. They were not edited in this task.

## Request and result

The user corrected the current Melee problem report: hitboxes show while hitting NPCs; NPCs appear to use Melee movement and RMB camera aim appears fixed; the server still crashes when an NPC dies; NPC hits still do not damage/launch the player. The user also requested immediate knockback and a percent popup, explicit percent-to-knockback targets, and a precise JSON location for tuning.

Updated `docs/specs/gamemodes/supersmashbrosmelee.md`:

- Line 719 now points to the selected curve targets in section 60 instead of calling all force-curve values open.
- Line 725 requires one immediate, predicted/reconciled gameplay result, a floating `+N%` popup, and a target percent update, while keeping the hit effect visual/audio-only.
- Lines 729-737 add section 60 with the user's current statuses, approximate targets (0% weak hit >= 0.5 m; same reference hit at 100% ~= 10 m and 200% ~= 50 m), and the required edit path `config/weapons.json` -> `captain_falcon_moveset.components.melee.knockback_curve`. The current file anchor is `config/weapons.json:1690`; stable key path is called out because line numbers can move. The spec explicitly says that this is a required configuration shape, not proof the key/loader exists.
- Earlier reports below section 60 are labeled historical where superseded by the user's update.

Exact old and new text:

- Section 58, before: “Keep the exact combined force curve, normalization, and caps open as recorded in sections 25–26 until they are selected or explicitly made JSON-editable provisional tuning.”
- Section 58, after: “The initial percent-to-knockback tuning targets and the JSON edit location are specified in section 60; the curve remains editable and subject to playtest tuning.”
- Section 59, before: “When the server confirms a Melee contact, the attacker, target, and observing clients receive one restrained impact response at the confirmed contact point: the existing entity-impact sphere, a short directional hit burst, and a spatial hit sound.”
- Section 59, after: “A confirmed Melee contact immediately produces its single authoritative gameplay result: add percent to the target and apply knockback once. Predict the response locally for immediate feel, then reconcile to the server result without applying a second impulse. Show a brief floating `+N%` number at the confirmed contact point and update the target's percent display.”
- Section 60: no prior text; the entire section is new.

## Review and evidence

- Read `docs/ROUTER.md`, `docs/specs/gamemodes/supersmashbrosmelee.md`, `docs/specs/weapons/weapons.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/task-completion/task-completion.md`, `docs/skills/spec-behavior-review-v1.md`, and `docs/skills/documentation-checker-v1.md`.
- The router-listed `docs/doc-review-09-03-2026.md` is absent in this checkout.
- Source comparison: the existing Melee definition starts under `captain_falcon_moveset.components.melee` at `config/weapons.json:1690`; existing per-action `knockback.base_impulse` fields do not establish that a shared percent curve exists. The requested `knockback_curve` object and runtime loader are therefore a future implementation requirement, not implemented or verified here.
- Documentation TODO scan found only procedural examples/instructions in `docs/ROUTER.md:117` and `docs/skills/documentation-checker-v1.md:22`; no actionable TODO in the edited Melee spec was introduced or changed.
- `git diff --check` reports trailing whitespace on the pre-existing user note at `docs/specs/gamemodes/supersmashbrosmelee.md:881`; this task did not edit that line.
- No tests, build, or runtime scenario were run. No server-crash cause, NPC hit-result path, or visual popup was verified in a live game.

## Remaining work

Implement the shared `knockback_curve` object and its validated loader at the documented JSON path, route player and NPC contacts through the same curve and result path, and reproduce both the NPC-death server crash and NPC-to-player hit in a fresh dev-loop run. Verify immediate prediction, server reconciliation, percent popup, and the 0/100/200 percent launch targets in gameplay. Human playtesting is still needed to accept the curve's feel.

## Follow-up: shield and implementation detail (2026-10-19 16:54 EDT)

The user asked whether the previous requirements fully specified NPC-to-player hits, stronger editable knockback, percent-number reuse, shield visuals/scaling/break response, and no hitstun. Read the current Melee spec, `docs/specs/weapons/weapons.md`, `docs/specs/gui/guiv2.md`, `docs/specs/effects/effects.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/skills/spec-behavior-review-v1.md`, and `docs/skills/documentation-checker-v1.md`. Inspected current `config/melee.json` and the Captain Falcon definition in `config/weapons.json`. The Melee spec was expanded at sections 24-29 and 59-60:

- Section 24 explicitly requires NPC-to-player contacts to update the player through the same accepted percent/knockback path as any actor pairing; visible overlap alone is not a confirmed hit.
- Sections 25-26 place the shared percent curve in `config/melee.json` under `force.knockback_curve` and per-move base values in `config/weapons.json`. The curve uses linearly interpolated `{target_percent, distance_m}` points at 0/0.5, 100/10, and 200/50, measured as horizontal travel over 60 server ticks on a clear flat lane with no target input. Physics converts distance targets to impulse; meters are not treated as velocity.
- Section 27 expressly forbids hitstun, helpless state, and input lock after hits or shield breaks; movement/actions resume on the next simulation opportunity.
- Section 28 and section 59 require reuse of the existing world-space damage-number renderer with `+{percent_delta}%`, updating the target's percent meter from the same result, plus visible force-scaled impact effects and sound.
- Section 29 specifies the cyan 2 m by 1 m by 1 m shield ellipsoid, editable visual color/alpha tied to durability, tick-1/tick-299 protection multipliers of about 0.01/0.99, natural decay at 300 ticks, and an editable 5.0 m/s upward break impulse (the existing config value). Hit-break also applies that hit's shield-scaled result; natural exhaustion does not invent an attacker hit.
- Section 60 now gives acceptance scenarios for both attack directions, the percent curve, shield drain and presentation, no hitstun, and the separate NPC-death crash investigation.

Exact before/after examples:

- Section 24, before: “The client immediately predicts contact and submits a candidate hit/grab result. The server applies the bounded referee checks in the networking spec, then accepts and publishes the same candidate result or rejects it with a reason.” After: “Players and NPCs use the same contact-result path for every attacker/target pairing, including NPC-to-player.” The new text requires an accepted NPC hit on a player to update percent and movement via the shared result path and says visible overlap alone is not a confirmed hit.
- Section 29, before: “Thus its remaining fraction is `clamp(1 - heldTicks / 300, 0, 1)`. When an attack hits the shield, the fighter's received percent and knockback multipliers are `1 - shieldRemainingFraction`...” After: “For an incoming hit, calculate both received-percent and knockback multipliers from the shield immediately before that hit: `clamp(1 - remainingFraction, 0.01, 0.99)`.” The section now also specifies the cyan ellipsoid, editable color/alpha, and break behavior.
- Section 27, before: “No hitstun. A player must be able to recover from knockback immediately and regain control...” After: “This mode has no hitstun, stun, forced helpless state, or input lock after a hit or shield break.”
- Section 28/59 now says to reuse the existing GUI world-space damage-number renderer with `+{percent_delta}%`; the target percent meter uses the same accepted result.
- Section 60, before: the shared curve was directed to `config/weapons.json` under the move definition. After: the shared curve is placed at `config/melee.json` -> `force.knockback_curve`; each action's base values remain in `config/weapons.json`. Current anchors are `config/melee.json:19` (`force`), `config/melee.json:6` (`shield`), `config/melee.json:11` (`break_upward_impulse`), and `config/weapons.json:1690` (move set).

Before this follow-up, section 24 only described candidate validation/publication generically, section 29's receive multiplier could compute nearly zero on tick 1 and nearly one on tick 299 without explicit clamps or visual contract, and section 60 pointed to a not-yet-existing curve under `config/weapons.json`. The revised text aligns the shared curve with the existing `config/melee.json` force owner; the new curve and `shield.visual` fields are still specification requirements, not implemented config/runtime behavior.

The changed files for the follow-up remain this specification and this existing changelog. Additional unrelated working-tree edits were present at follow-up in animation assets, `config/analytics.json`, `config/audio/music-settings.json`, `config/weapons.json`, combat/animation C++ files, the animation exporter, and new animation JSON assets; none were modified for this request. No build, tests, or live gameplay were run. `git diff --check` still flags the pre-existing trailing whitespace in the user's 2026-10-19 crouch note at spec line 885; it was not changed.

The documentation TODO scan found unrelated notes, which remain untouched. The one in the edited Melee spec is `docs/specs/gamemodes/supersmashbrosmelee.md:1`, “todo explainbetter”; suggested replacement is a short statement of the spec's purpose and boundaries before the current introduction. Other broad-scan notes were found across unrelated design docs and were not changed.

## Follow-up: fixed hit percent and WASD versus knockback

The user asked whether the `smash` movement preset accelerates or overwrites horizontal velocity, and specified flat percent-per-move, stronger persistent knockback, and strong steering without erasing impact motion.

Source investigation confirms `config/movement/movement-smash.json` selects `movement_mode: "mimita"`. `src/config/movement-config.cpp` maps `mimita` to `MovementWalkMode::Override`; `src/physics/movement/movement-step.cpp`'s `movementWalkVelocityXY` returns the input direction at configured ground/air speed, replacing existing horizontal velocity whenever `applyBasicWalk` runs. In override mode, the post-collision step also invokes external-impulse control. Therefore this is instant directional overwrite, not gradual WASD acceleration, and the movement step contains behavior that can consume/erase a separate knockback impulse. This is source/config evidence, not a live-game run.

Updated `docs/specs/gamemodes/supersmashbrosmelee.md` sections 25, 26, and 60. Section 25 now says each accepted unshielded hit adds exactly that move's configured `base_percent` at every target percent; contact strength and current target percent affect knockback only. Section 26 and section 60 specify that Melee knockback persists as composable motion while WASD strongly steers on ground and in air, without instant cancellation or hitstun. Section 60 adds a hold-then-reverse-WASD acceptance check. Shield protection remains an explicit section 29 exception to full percent addition.

No code/config implementation, build, tests, or live gameplay were changed or run. Runtime acceptance remains pending. The repository status contains unrelated pre-existing edits; none were modified here. The Melee spec still has the pre-existing opening TODO `todo explainbetter` at line 1; suggested improvement: replace it with a one-sentence purpose and scope statement.

## Follow-up: use xxity2 for Melee AI launches (2026-10-19 17:46 EDT)

- Branch/base: `20261011zt` / `26562692`.
- Request: change the Melee automatic AI scenario launch map from `cramped` to `xxity2`.
- Source path: `devscripts/dev-loop.py` reads each scenario's `launch.map` and applies it as an explicit map override. The Melee start, probe, lifecycle, fight, and combat scenarios had explicit map selections; the `npc-navigation` dev profile and `melee_1` server preset also defaulted to `cramped`.
- Changed `devscripts/scenarios/melee_start_v1.json:8`, `melee_probe_v1.json:8`, `melee_lifecycle_v1.json:8`, `melee_fight_v1.json:8`, and `melee_combat_v1.json:8` from their prior `cramped`/`badhouse` map values to `xxity2`. Changed `devscripts/dev-profiles/npc-navigation.json:5` and `config/server-presets.json:46` from `cramped` to `xxity2`.
- Focused validation parsed all seven modified JSON files, confirmed every Melee launch/profile/preset map is `xxity2`, confirmed `xxity2` is in `config/gamemode-good-maps.json`, and confirmed `assets/maps/xxity2.glb` exists. `git diff --check` reported no whitespace errors for these files (only line-ending normalization notices).
- No build or live game launch was performed; therefore the source configuration is verified but the resulting runtime map load is not. Existing unrelated worktree changes were preserved.
- Documents read: `docs/ROUTER.md`, `docs/specs/gamemodes/supersmashbrosmelee.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/task-completion/task-completion.md`, `docs/skills/spec-behavior-review-v1.md`, and `docs/skills/documentation-checker-v1.md`. Router-listed `docs/regressions/regressions-v1.md` is absent in this checkout; no regression was changed for this map selection.
