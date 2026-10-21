# Swordsword contact implementation

- Result: `PASS_WITH_HUMAN_REVIEW` for implemented source/build changes;
  overall Swordsword implementation remains incomplete against its spec.
- Local time: 2026-10-20 20:32:11 EDT.
- Canonical time: 2026-10-21T00:32:11Z.
- Final update time: 2026-10-20 20:34:06 EDT
  (2026-10-21T00:34:06Z).
- Branch: `20261011zt`.
- HEAD: `f6d6eeb2ce409153c8d23933b32c77eee39a5b48`.
- Runtime started on build 2706 and remained running; it was not replaced.

## What changed

### `src/combat/weapon-swordsword.cpp`

`startSlash` and `startLunge`, lines 432-454:

- Before: slash added `DASH_IMPULSE * 0.5f` to the owner's horizontal
  velocity, and lunge added full `DASH_IMPULSE`.
- After: both functions only select attack state/timers and play their start
  sound; they do not change owner velocity.
- Before: the active lunge multiplied all owner velocity by
  `(1.0f - lungeDrag * dt)`.
- After: that attack-only velocity drag is removed.
- Reason: Swordsword input must not propel or otherwise change the wielder's
  movement. Contact knockback remains in the contact result code.

### `src/network/server-physical-contact.cpp`

`buildPhysicalShape`, lines 91-105:

- Before: Swordsword shared the Melee/Hafs branch and returned no shape unless
  `physicalAttackTimeline` was committed and active.
- After: Swordsword always constructs its configured capsule from the current
  server actor position and facing. Melee and Hafs remain attack-window gated.
- Reason: Swordsword contact must remain enabled while idle; click actions are
  presentation only.

### `src/network/server-damage.cpp` and `src/network/server-physical-contact.cpp`

`applyActorDamage`, lines 291-397, and
`applyPhysicalContactNpcHit`, lines 279-296:

- Before: `applyActorDamage` returned `phase1-player-victim-required` for every
  NPC victim, and Swordsword physical contact changed NPC health directly.
- After: the shared damage owner resolves either player or NPC victims,
  applies the existing known-team friendly-fire rule for NPC victims, records
  `server.damage.applied` / `server.damage.rejected` with actor IDs, types,
  teams, health, source, and knockback, and the player-to-NPC contact handler
  submits a typed `ServerActorDamageRequest` through that owner.
- Reason: Swordsword player-to-NPC damage should share authoritative damage
  decisions and actor identity instead of maintaining a second health-mutation
  path.

## Known incomplete work

This change does not complete the behavior contract in
`docs/specs/weapons/swordsword.md`:

- The server physical-contact tick, lines 523-585, still iterates players as
  attackers only. NPC-attacker contact, including NPC-vs-player and
  NPC-vs-NPC, is not implemented here, even though the shared damage owner can
  now accept typed NPC attackers when another caller submits one.
- Server contact still uses an approximate actor-position/facing capsule, not
  the visible weapon transform or real blade body contacts. The force curve,
  paired sound scaling, and confirmed-contact presentation are not completed.
- The current input state machine does not provide all four required click
  outcomes, RMB-held defensive state, or NPC AI equivalents. Weapon-specific
  pose availability and NPC animation playback still need implementation and
  authored clips.
- Local-only NPC contact remains attack-state gated and does not test players;
  online server contact is the changed owner in this patch.

These gaps need shared actor damage and weapon-animation work before claiming
the full feature. No balance numbers or animation poses were invented.

## Evidence

- Documents read: `docs/ROUTER.md`, `docs/specs/weapons/swordsword.md`,
  `docs/specs/weapons/weapons.md`, `docs/specs/networking/networking.md`,
  `docs/specs/animations/animations.md`,
  `docs/architecture/collision/actor-vs-actor-collisions.md`,
  `docs/architecture/player-npc-systems/player-npc-systems.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/specs/debug-logging/debug-logging.md`,
  `docs/architecture/terminal-commands/terminal-commands.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/architecture/live-development/live-development.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/skills/spec-behavior-review-v1.md`, and
  `docs/skills/logging-checker-v1.md`.
- Missing routed documents: `docs/doc-review-09-03-2026.md` and
  `docs/regressions/regressions-v1.md` are referenced but absent in this
  checkout.
- Source check: `git diff --check` reported trailing whitespace in the
  concurrent `config/actor-collision.json` edit and line-ending conversion
  warnings for working-copy files; neither was changed here.
- Build: final canonical dev-loop request succeeded, build 2708, source
  generation 34; 2 translation units compiled and 559 skipped. Build 2707 was
  produced before the shared damage change. The active runtime remains build
  2706.
- Fresh executable: `C:\mimita-v9\.dev\builds\2708\mimita.exe`.
- `--versioninfo`: reported
  `EVENTS_JSONL_PATH=logs/10-20-2026/20261020_203344/events.jsonl`. This is
  metadata-only and is not gameplay evidence.
- Runtime scenario: not run. The live build-2706 client/server session was
  preserved. No damage, kill, animation, or multiplayer acceptance is claimed.
- Focused skill result: `spec-behavior-review-v1` — incomplete against the
  complete contract; the remaining disagreements are listed above.
- Focused skill result: `logging-checker-v1` — actor damage now logs the
  authoritative result and rejection context; blade-shape selection and
  contact-to-confirmation runtime evidence still need observation.

## Pre-existing and concurrent work preserved

At session start, unrelated edits existed in `config/accounts/default.json`,
`config/analytics.json`, `config/animations.json`,
`config/audio/music-settings.json`, `config/video-settings.json`, and
`config/weapons.json`; `docs/specs/weapons/swordsword.md` and the earlier
`20261021_002239-swordsword-spec-clarity.md` were also already modified or
untracked. `config/actor-collision.json` appeared modified during this session.
None were changed as part of the implementation described above.

## Human review still needed

Review the in-game wielder movement and continuous blade contact using build
2708, then complete the listed actor-pair, force/audio, input, animation, NPC,
and visual-contact requirements before marking Swordsword complete.
