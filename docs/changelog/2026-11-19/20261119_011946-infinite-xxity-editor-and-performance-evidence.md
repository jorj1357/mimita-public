# Infinite XXITY editor and performance evidence

- Extended the creator spec with `mapeditor_clone`, independent source
  preservation, and a local project flow for cloning an Infinite World.
- Extended the Procedural Infinite World spec with chunk-part size/material
  dropdown controls, explicit world radius/lookahead/LOD/collision/fog
  settings, bounded far representation, and a 1,000 m/s + 1,000 m radius
  stress scenario with one active NPC and 240 FPS / 60 Hz evidence gates.
- Added runtime event contracts for fork/dependency identity, saved part
  dimensions/materials, profile/fog application, streaming/radius/velocity,
  collision sync, and an aggregated p99.99 performance summary.
- Existing-config evidence: Infinite XXITY's worldgen connector radius is
  60 m; connected-world presentation configuration already has a 1,000 m
  visual/far-proxy radius, 1,000 m fog end, and one-second lookahead. These
  values do not prove a 1,000 m generation radius or successful fast travel.
- Live runtime inspected: dev-loop run `20261119_010408` is a Duel session
  with build ID `2026-11-19T00:53:35.016823Z:eabcdcd2e22ec7eb301a8e858578833b98188c88dad26c9bd04e50106e34022d`. Its client and server journals contain no target clone/editor/XXITY stress events. The unrelated run was preserved; no feature runtime validation was possible because `mapeditor_create` and `mapeditor_clone` are not implemented in source.
- Validation: `git diff --check` and referenced-document review. No build or
  new runtime scenario was run. The currently running executable was only
  inspected, not replaced or steered.
- Review instructions used: `docs/skills/spec-writing-v1.md`,
  `docs/skills/spec-behavior-review-v1.md`, and
  `docs/skills/logging-checker-v1.md`.

## Bloodtest body authoring and visual proof — 2026-11-19

- Expanded the Bloodtest target contract for editable avatar body dimensions,
  named skeleton joints, separate damage/body/world-collision geometry, living
  whole-body or torso/head/leg capsule profiles, and shared role-driven
  movement. Defined the deliberate 1 m/s baseline, weaker jump, a Bloodtest
  fresh-press jump repeat delay (not an input buffer), smooth sprint ramps,
  role-owned stamina, and fighter/juggernaut/monster role examples.
- Added a local Map Editor Play Test projectile bench requirement that can
  launch an authored physical object at an NPC/ragdoll, save/replay a launch
  recipe, reset the target, and review body-layer/blood results on the shared
  per-tick timeline. Added bounded success-event contracts for body/profile
  loading, movement/stamina changes, projectile/contact, layer transition,
  effect application, and replay completion.
- Updated movement's default no-cooldown rule with the scoped Bloodtest jump
  repeat-delay exception; presses during the delay are discarded and never
  buffered. Routed Bloodtest work to its owning specs and linked its body data
  to the existing avatar rig in the creator spec.
- Added visual artifact requirements for Infinite XXITY fog acceptance: a
  screenshot or short video, camera position, run ID, and build ID must show
  fog hiding unready chunk boundaries. Journal values alone cannot establish
  visual coverage.
- Behavior review found an active-config/spec gap: `config/gamemodes/bloodtest.json`
  currently selects the `bloodtest` preset, whose config uses max speed 12,
  jump velocity 16, and a 0.2 s jump buffer. Target spec now calls for the
  shared `deliberate` 1 m/s baseline. The deliberate preset already exists;
  role movement overrides, stamina/sprint data, and the editor/test-bench
  operations are not implemented by this spec-only change.
- Runtime check: safe dev-loop status reported run `20261119_012255` in
  Juggernaut mode with the already-published build ID
  `2026-11-19T00:53:35.016823Z:eabcdcd2e22ec7eb301a8e858578833b98188c88dad26c9bd04e50106e34022d`.
  Its client/server journals had none of the new Bloodtest/editor/fog stress
  events. Dev-loop status also reported a failed queued codegen check
  (`packet_registry_codegen.py`); no build was requested and the current run
  was not replaced or steered. Runtime proof and visual review remain open.
- Documentation validation: `git diff --check` passed. Existing unrelated
  working-tree edits were preserved. Repository TODO scan notes left
  untouched: `docs/specs/procedural-infinite-world/procedural-infinite-world.md:1`
  (`9 7 2026 1712 jorj todo - explain the inf dng slr infinite world gneration , and like`;
  suggest explaining how connector generation and shared streaming produce the
  infinite world); `docs/specs/weapons/melee/swordsword.md:238` (`todo need to
  make the poses animation things etc dont have tem right now`; suggest naming
  required clips/poses and how they bind to sword actions); and
  `docs/architecture/player-npc-systems/npc-movement.md:22`
  (`9 8 2026 1019 est jorj todo - explain`; suggest summarizing current
  navigation/movement ownership and linking multi-floor acceptance); and
  `docs/specs/gamemodes/zombie-tower/zombie-tower.md:5861`
  (`2026 10 06 1603 jorj todo - explain zombie tower`; suggest a concise
  summary of the co-op survival premise, floor progression, and mode contract).

## In-game creator entry and launch flow

- Reordered the end-user workflow so Create → New Map is the primary entry;
  terminal `mapeditor_create`/`mapeditor_clone` remain aliases to the same
  project action for the first implementation slice and power users.
- Added New Monster blockout creation with editable head/torso/arms/legs,
  named pivots, part transforms, per-part material/texture assignment, and
  later replacement with imported model nodes. Required starter clips are
  idle, walk, attack_1, and hurt; attack_2 and other clips can be authored.
- Added optional weapon model/socket attachment, explicit local Play Test
  controls for applying validated damage/health/movement/hitbox revisions,
  and fixed-tick activation semantics for existing and future spawned actors.
- Specified a Create / My Creations / Community launch surface. The Community
  screen keeps server Join separate from local Play Local, and local projects
  remain playable offline without publishing. Remote package discovery,
  signing, and automatic network fetch remain governed by Distribution and
  are later than local project launch.
- Added acceptance scenarios for UI map creation, block transforms, primitive
  monster authoring, live tuning, spawn-volume use, and My Creations launch.
  Updated Router routes and linked the shared GUI specification for
  creator-launch UI ownership. Reconciled the old console-first wording so
  Create UI and terminal call one project action.
- Added five delivery slices separating the map loop, monster placement,
  monster authoring, local live-tune/launch, and remote sharing/discovery.
- Extended the event chain to distinguish durable project creation from a
  loaded editor, prove transform/spawn-volume application and monster spawn,
  and show a live server-side tuning revision taking effect on a later attack.
- Scope assessment: specifications are sufficient to begin a staged local
  implementation. The complete feature is too broad for one implementation
  slice; exact movement/blood tuning, model/animation authoring, map editing,
  content launch, and remote distribution should land and be validated in
  separate slices. No additional product decision blocks the first UI-to-map
  creation slice. The source changes are documentation only; no build or
  gameplay runtime validation was performed for this spec update.
