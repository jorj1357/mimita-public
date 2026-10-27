# Melee cadence issue intake

Time: 2026-10-27T06:17:34Z (2026-10-27 02:17:34 EDT)
Branch: `20261011zt`
HEAD: `eb79b2de vehcile and 4 new melee wepaon attemptetd to add in this commit not tested first ai pass etc`

## Result

Added a melee-cadence issue to `docs/specs/open-gameplay-issues.md:1291` under `New intake — 2026-10-27`.

The entry preserves the user's requested rule (“there should be no hidden cooldowns or limits”; one attack per tick, repeatedly, for every melee weapon), references the existing melee specification's explicit no-generic-cooldown / once-per-victim-per-weapon-per-tick rule, and flags the current HAFS and Chainsaw `damageTickInterval: 0.05` values in `config/weapons.json` for runtime tracing. It also records the request for stronger architecture and weapon documentation that names the invariant and its owner so a future config field cannot silently reintroduce throttling. This is issue intake, not an implementation or a source diagnosis.

## Exact change

Before, the 2026-10-27 intake ended with:

```md
- **Explore camera-influenced ragdoll arm poses:** The player wants ragdoll
  arms to respond somehow to camera control. This is an early design request:
  define how camera-driven arm control can coexist with physical ragdoll motion,
  authority, and other observers before selecting behavior or implementation.
```

After, appended:

```md
- **Melee attack cadence must not be throttled by hidden limits:** The player
  states: “there should be no hidden cooldowns or limits”; all melee weapons
  should allow “1 attack per tick over and over and over,” with no added
  artificial limit. The shared melee specification already states there is no
  generic melee hit cooldown and permits contact damage once per victim,
  weapon, and 60 Hz simulation tick. However, the current HAFS and Chainsaw
  entries in `config/weapons.json` contain `"damageTickInterval": 0.05`, which
  is a user-identified concern and needs its runtime meaning traced; do not
  assume it is harmless configuration or silently treat it as an intended
  attack throttle. Reconcile attack requests, contact damage sampling, and
  network batching with the one-attack-per-tick requirement, then remove or
  redefine any field that imposes an unintended interval. Strengthen the
  architecture/weapon documentation with an explicit owner and invariant so
  future weapon fields cannot reintroduce hidden melee cadence limits.
```

## Documents, review, and validation

- Read `docs/ROUTER.md`, `docs/operations/task-completion/task-completion.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/specs/weapons/melee/melee-weapons.md`, `docs/specs/weapons/weapons.md`, and `docs/specs/open-gameplay-issues.md` around the intake and cadence references.
- The existing owner spec already states no generic melee hit cooldown, 60 Hz simulation, and up to one hit per victim/weapon/tick. The intake records the apparent configuration concern without presuming how `damageTickInterval` behaves in code.
- Documentation/spec review: PASS_WITH_HUMAN_REVIEW; the one-attack-per-tick request is captured, while exact semantics between attack animation/request cadence and continuous contact damage remain for implementation tracing. Architecture documentation expansion is requested for follow-up.
- `git diff --check`: PASS. No build, runtime trace, or gameplay test was performed; the field's effective behavior and user-visible cadence remain unverified.
- Preserved existing unrelated dirty worktree changes from the prior gameplay task; no config, implementation, or animation files were edited in this follow-up.
