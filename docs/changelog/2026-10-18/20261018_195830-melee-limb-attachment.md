# Melee Limb-Attached Hitboxes

Time: 2026-10-18T19:58:30Z (2026-10-18 15:58:30 EDT)
Branch: `20261011zt`
HEAD before this session's changes: `3bf384e4`

## Goal and result

The user reported that melee hit volumes followed a camera-derived estimate
instead of the actual attacking limb. The Melee spec already required the
server to sample the animated part transform; the server implementation still
used a guessed root-height point plus camera-horizontal-forward. This change
adds `animated_limb` attachment as the default, opts all Captain Falcon
hitboxes into it, and evaluates the active clip and limb-local offset on the
authoritative server. The client preview already reads the animated body-part
world transform.

## Exact source and specification changes

1. `src/combat/weapon-melee-config.h:45-46, 62-72`
   - Before: no `MeleeHitboxAttachmentMode`; hitboxes only stored part, shape,
     size, and offset.
   - After: added `CameraBasis` / `AnimatedLimb`, a validity flag, and
     `AnimatedLimb` as the in-memory default.

2. `src/combat/weapon-melee-config.cpp:156-183, 320-330`
   - Before: `parseHitbox` read `local_offset` and did not read an attachment
     mode.
   - After: it reads `attachment_mode` (default `animated_limb`), supports the
     explicit legacy value `camera_basis`, and omits hitboxes with unknown
     mode values.

3. `src/network/server-body-template.cpp:206-238, 258-289` and
   `src/network/server.h:607-615`
   - Before: body templates retained only an AABB offset and half-size.
   - After: templates retain each normalized body-part name and model-root
     relative rest transform. The headless loader applies the same
     `config/bodyparts.json` rest overrides before calculating these
     transforms.

4. `src/network/server-melee-actions.cpp:129-222, 638-642, 670-678,
   803-831, 979-1003`; `src/network/server.h:414-417, 659-665`
   - Before: player hitbox center used a fixed part-height estimate and the
     captured camera basis; the NPC path did the same and did not retain the
     prior limb center for a sweep.
   - After: the server samples the action's unified clip at elapsed action
     tick or the phase clip at its normalized phase sample, interpolates the
     named limb, applies its rest transform and local offset, then applies the
     actor root and model yaw captured at action start. Players and NPCs use
     this sampled center for their contact checks. Missing animation/part data
     skips the contact and emits `melee.hitbox.pose_unavailable`; resolved
     centers emit bounded `melee.hitbox.transform` records. Camera basis
     remains only for explicitly configured legacy hitboxes and separate move
     impulses/directness policy.

5. `config/weapons.json:1699, 1721, 1742, 1759, 1783, 1807, 1828, 1852,
   1876, 1897, 1918, 1942, 1966`
   - Before, example: `"shape": "sphere",` for `fist`.
   - After, example: `"shape": "sphere", "attachment_mode": "animated_limb",`
   - Applied to all 13 configured Captain Falcon punch, kick, and grab
     hitboxes. Existing hitbox sizes and offsets were left as authored.

6. `docs/specs/weapons/weapons.md:333, 363-374` and
   `docs/specs/gamemodes/supersmashbrosmelee.md:717`
   - Before: the spec required sampled limb poses but did not name the mode or
     explicitly forbid fallback camera placement.
   - After: specify `animated_limb` as the default, define the actor/limb/local
     offset transform order, make camera direction/pitch irrelevant to volume
     placement, and require skip-plus-diagnostic when a pose cannot be
     resolved.

7. `docs/regressions/2026-10-18/melee-hit-feedback-and-hitbox-accuracy-REG.md:6,
   54-88`
   - Before: status `ATTEMPTED FIX (1)` and the server/rendered limb mismatch
     remained open.
   - After: status `ATTEMPTED FIX (2)` with this source-level fix and the
     missing live and human acceptance recorded. This is the same regression,
     so its history is appended rather than replaced.

## Existing user changes preserved

The working tree already contained changes to the Captain Falcon animation
profile and clips, account/analytics/audio configuration, animation
specifications/exporter, and earlier Melee changelogs. Those changes were
preserved. The pre-existing `docs/specs/gamemodes/supersmashbrosmelee.md`
notes near the file end were also preserved; the new attachment-mode paragraph
was added in section 58 only.

## Documents and focused review

- Read `docs/ROUTER.md` and followed the weapon, networking, animation,
  runtime-validation, and build routes.
- Reviewed `docs/specs/weapons/weapons.md`,
  `docs/specs/networking/networking.md`,
  `docs/specs/animations/animations.md`, and
  `docs/specs/gamemodes/supersmashbrosmelee.md` sections 17 and 58.
- Read `docs/workflows/runtime-scenario-validation.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/logging-checker-v1.md`, and `docs/regressions/README.md`.
- Spec finding: section 58 already says the server derives the world
  transform from the authoritative root and sampled animated limb pose. The
  old server's camera-basis estimate contradicted this requirement.

## Validation and evidence

- `config/weapons.json` parsed successfully as JSON.
- Canonical `python devscripts/dev-loop.py --request build --wait-for-build`
  published current debug build 2607. Dev-loop status reports source
  generation 11, latest successful generation 11, and
  `C:\mimita-v9\.dev\builds\2607\mimita.exe`.
- Build 2607 `--versioninfo` succeeded and reported
  `EVENTS_JSONL_PATH=logs/10-18-2026/20261018_155827/events.jsonl`.
- No unit or synthetic tests were added or run.
- The dev-loop run still had client PID 27716 on build 2603. Its managed
  server PID 27216 had exited. The existing client was left running; the
  runtime launch path would replace that active client, so no build-2607
  gameplay scenario was run.
- The journal `logs/2026-10-18/20261018_195300/events.jsonl` contains old-build
  melee action starts but no evidence for build 2607's new transform events.
  Therefore contact parity, a live attack, and visual/human acceptance remain
  unverified. Result: `PASS_WITH_HUMAN_REVIEW` for source/build; runtime
  behavior remains pending.

## Known boundaries

- The authoritative body-rest transform currently comes from the default
  player GLB and its global `config/bodyparts.json` overrides, which is the
  Captain Falcon profile's current model path. Per-avatar custom model rest
  transforms still need to be resolved through their own asset path before
  this mode can claim parity for arbitrary rigs.
- This fixes transform ownership. It does not tune sphere radii or local
  offsets; those values remain editable in `config/weapons.json`.
