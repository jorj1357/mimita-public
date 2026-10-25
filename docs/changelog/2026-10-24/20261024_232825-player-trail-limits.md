# Player Trail Limits

- Recorded: `2026-10-24T23:28:25Z` (display: `2026-10-24 19:28:25 EDT`, `America/New_York`)
- Branch: `20261011zt`
- HEAD: `0472121baef8d96342508d82aef5598de97daff7` (unchanged)
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request and source finding

The user reported that edits to `config/playervisuals.json` did not affect
player trails and asked to remove arbitrary trail number limits.

`docs/specs/visuals/visuals.md`, section 7, requires that a valid save be
detected, validated, applied, and update existing visible players without a
restart. `PlayerVisualsConfig::pollReload()` is called from the active engine
setup tick, and the renderer reads the config while drawing existing actors.
The user's pre-existing config edits set `enemy.trail.maxSamples` to `128` and
`teammate.pointLight.heightOffset` to `-91`. Before this change,
`readTrail()` rejected `maxSamples > 64`, and `readPointLight()` rejected
`heightOffset` outside `[-100, 100]`; because loading is atomic, either value
rejected the whole file and retained the prior settings. The inspected config
also sets trail width to `0.55`, which is within the removed `[0.01, 4.0]`
range.

The user's modified `config/playervisuals.json` was already dirty before this
task and was preserved. Other pre-existing shared-checkout edits were
preserved. Additional edits appeared during this work in avatar, entity, and
network source/config files; those were not changed by this task.

## Changes

In `src/config/player-visuals-config.cpp`, `readTrail()` previously required
`maxSamples <= 64` and `width <= 4.0`; it now has no upper bound for either.
It still requires finite values, at least two samples to form a ribbon,
positive width/sample interval/lifetime, nonnegative minimum speed, and alpha
within `[0, 1]`.

`readPointLight()` previously rejected finite `heightOffset` values outside
`[-100, 100]`. That arbitrary position ceiling was removed so the user's
current file can validate as a whole. Finite-value validation remains.

`player_visuals.config.applied` now records the enemy trail minimum speed,
sample interval, lifetime, sample count, and width, so a runtime journal can
show exactly which trail settings were installed.

In `src/render/render-player.cpp`, removed the eviction of trail histories
after 128 actors. Existing stale-history cleanup after five seconds remains.

Exact validation change in `readTrail()`:

```cpp
// Before
settings.lifetimeSeconds <= 0.0f || settings.maxSamples < 2 || settings.maxSamples > 64 ||
settings.alpha < 0.0f || settings.alpha > 1.0f || settings.width < 0.01f || settings.width > 4.0f

// After
settings.lifetimeSeconds <= 0.0f || settings.maxSamples < 2 ||
settings.alpha < 0.0f || settings.alpha > 1.0f || settings.width <= 0.0f
```

Exact point-light change:

```cpp
// Before
settings.radius < 0.1f || settings.radius > 1000.0f ||
std::abs(settings.heightOffset) > 100.0f
err = name + " intensity must be 0..10, radius 0.1..1000, and heightOffset -100..100";

// After
settings.radius < 0.1f || settings.radius > 1000.0f
err = name + " intensity must be 0..10 and radius 0.1..1000";
```

The renderer no longer has this deleted limit:

```cpp
if (gTrailHistories.find(entityId) == gTrailHistories.end() &&
    gTrailHistories.size() >= 128) {
    auto oldest = gTrailHistories.begin();
    for (auto it = gTrailHistories.begin(); it != gTrailHistories.end(); ++it)
        if (it->second.lastSeen < oldest->second.lastSeen) oldest = it;
    gTrailHistories.erase(oldest);
}
```

## Review

- `docs/skills/spec-behavior-review-v1.md`: `PASS_WITH_HUMAN_REVIEW`. The code
  path contradicted the hot-reload intent by rejecting otherwise finite values
  due to arbitrary upper bounds. Existing actors read the installed settings
  on render. Runtime application and visual appearance still need observation.
- `docs/skills/logging-checker-v1.md`: `PASS_WITH_HUMAN_REVIEW`. Reused the
  existing categorized apply/reject events and added bounded config values to
  the apply event; no per-frame logging was added.
- `git diff --check`: passed.

## Build and runtime evidence

- The shared dev-loop published build `3106` from source generation `13` and
  reported it current at publication. The two changed source files had no
  later edits. Additional unrelated source edits later advanced the working
  tree to generation `19`; at final review the dev-loop reported another
  build in progress, so build `3106` is not the latest whole-checkout build.
  The two files changed by this task were not modified after build `3106`
  published.
- Ran the exact executable
  `C:\mimita-v9\.dev\builds\3106\mimita.exe --versioninfo`.
  It reported `EVENTS_JSONL_PATH=logs/10-24-2026/20261024_192720/events.jsonl`
  with working directory `C:\mimita-v9`. This confirms executable identity
  and journal selection only; `--versioninfo` exits before loading player
  visuals.
- The existing `npc-navigation` dev-loop run still had its server alive, and
  its client was no longer running. Launching build `3106` through the loop
  would stop/replace the server because its build identity differs. It was not
  launched, to preserve the active server. No new live config-apply event or
  visual observation is claimed.
- No new synthetic test was added. Human visual acceptance remains open.

## Required follow-up

Replace the currently running dev-loop server/client with the newly built
artifact when the user authorizes that session interruption, then inspect the
new run's `player_visuals.config.applied` event for the edited values and
observe enemy movement trails in game. The shared checkout has concurrent
unrelated edits, so build identity should be rechecked before that launch.

## Documents and focused review set

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/visuals/visuals.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/regressions/README.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`
