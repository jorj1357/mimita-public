# Juggernaut physics-time spike issue intake

- Time: 2026-10-22T17:52:06Z (2026-10-22 13:52:06 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD at start: `853366427f4285e43a21005e82838f000260668d`
- Result: `PASS_WITH_HUMAN_REVIEW` — recorded the player's 13:49 report as an unverified issue; no cause or 240 FPS result is claimed.

## Scope and source state

Added one performance report to the current open gameplay issue intake and
index. The player is working toward 240 FPS in Juggernaut with all actors on
screen and reports that the physics-ms value jumps substantially. The player
was not connected to a server at the time. The exact actor count, metric
definition, frame/physics timings, build, and local run mode were not supplied.

The NPC behavior specification already records a distinct connected run with
68 actors, collision time of roughly 0.02–0.09 ms, and server tick cost of
4–9 ms. That evidence does not confirm or explain this player's separate
non-server-connected observation. The performance spec describes 240 FPS as a
4.17 ms rendered-frame target, not a guarantee or a gameplay tick-rate change.

At initial status inspection, the following other edits were already present
and were preserved: `config/accounts/default.json`, `config/analytics.json`,
`config/audio/music-settings.json`,
`docs/architecture/accessibility/accessibility.md`,
`docs/jorj-docs/20261022plan.md`, `docs/specs/gamemodes/payload.md`,
`docs/specs/open-gameplay-issues.md`, `shaders/basic.vert`,
`src/effects/effect-part-blood.cpp`, `src/effects/effect-part-particles.cpp`,
`src/engine/engine-tick-render.cpp`, `src/engine/engine-tick-ui.cpp`,
`src/engine/engine-tick.cpp`, `src/entities/player-render-mesh.cpp`,
`src/entities/player-render.cpp`, `src/network/multiplayer-context.h`,
`src/network/multiplayer-interpolation.cpp`, `src/render/render-player.cpp`,
and `src/render/render-player.h`. Previously untracked changelogs were also
present: `docs/changelog/2026-10-22/20261022_171528-gameplay-issues-intake.md`,
`docs/changelog/2026-10-22/20261022_171650-zombie-tower-editor-blocker.md`,
`docs/changelog/2026-10-22/20261022_172026-live-blender-map-authoring-intake.md`,
and `docs/changelog/2026-10-22/20261022_174708-reconnect-memory-low-resource-accessibility.md`.
During this task, `config/aimbody.json` and
`docs/changelog/2026-10-22/20261022_175106-payload-rules-source-audit.md`
appeared in the worktree; they were preserved. This task changed only the two
new rows below and this changelog.

## Exact change

File: `docs/specs/open-gameplay-issues.md`

The 2026-10-22 intake table, line 113. Before, this entry did not exist; the
preceding existing memory issue row was:

```markdown
| Memory use grows sharply during reconnect attempts | The player reports RAM use rising by roughly 100 MB per second while reconnecting, then staying around 4,180 (exact value and units uncertain); the player says the figures are approximate and is concerned about the size of the increase. | User-reported, not reproduced; cause and exact memory units/measurements are unknown. Measure process memory across a bounded reconnect sequence, identify which owner retains or allocates the growing memory, and verify whether usage is released or remains elevated after reconnect succeeds or stops. Do not infer a leak or blame a component from the estimate alone. | [`networking/networking.md`](networking/networking.md), [`performance/performance.md`](performance/performance.md), [`../architecture/accessibility/accessibility.md`](../architecture/accessibility/accessibility.md) |
```

After, the new row at line 113 is:

```markdown
| Juggernaut physics time spikes while aiming for 240 FPS with all actors visible | Reported at 2026-10-22 13:49 (time as provided by the player): while working toward 240 FPS with all actors on screen, the reported physics-ms value jumps substantially. The player was not connected to a server at the time. | User-reported, not measured or reproduced. Exact actor count, frame/physics timings, metric definition, build, and local run mode are unknown. Profile the same non-server-connected Juggernaut scenario with actor count and frame/physics timings recorded, then trace the first expensive physics owner before proposing an optimization. The separate connected 68-actor measurement in the NPC behavior spec does not establish this report's cause or result. | [`performance/performance.md`](performance/performance.md), [`gamemodes/juggernaut.md`](gamemodes/juggernaut.md), [`npc-behavior/npc-behavior.md`](npc-behavior/npc-behavior.md), [`../architecture/collision/collision.md`](../architecture/collision/collision.md) |
```

The issue index, line 124, also had no entry for this report. After:

```markdown
| Juggernaut physics time reportedly spikes while aiming for 240 FPS with all actors visible | Player-reported at 2026-10-22 13:49 while not connected to a server; exact frame/physics timings and actor count are unknown | Reproduce and profile this local run separately from the connected 68-actor measurement already recorded in the NPC behavior specification | [`performance/performance.md`](performance/performance.md), [`gamemodes/juggernaut.md`](gamemodes/juggernaut.md), [`npc-behavior/npc-behavior.md`](npc-behavior/npc-behavior.md), [`../architecture/collision/collision.md`](../architecture/collision/collision.md) |
```

## Documents and focused review

- Read `docs/ROUTER.md`, `docs/specs/open-gameplay-issues.md`,
  `docs/specs/performance/performance.md`,
  `docs/specs/gamemodes/juggernaut.md`,
  `docs/specs/npc-behavior/npc-behavior.md`,
  `docs/architecture/collision/collision.md`, `docs/regressions/README.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/skills/documentation-checker-v1.md`,
  `docs/skills/spec-behavior-review-v1.md`, and
  `docs/skills/efficiency-checker-v1.md`.
- The router requests `docs/doc-review-09-03-2026.md` and
  `docs/regressions/regressions-v1.md`; both are absent in this checkout.
  `docs/regressions/README.md` states that regression records are for confirmed
  behavior regressions, so this unmeasured report was recorded as an issue,
  not as a confirmed regression.
- Specification and Behavior Review: `PASS_WITH_HUMAN_REVIEW`. The recorded
  goal is the player's current scenario, not a universal 240 FPS promise. The
  physics metric and cause remain unknown. The existing connected 68-actor
  run is kept separate from this report.
- Efficiency Checker: `NOT_APPLICABLE` to source review because this task made
  no code changes and did not claim an optimization. The performance and NPC
  behavior specs were checked for existing target and measurement evidence.
- Documentation Checker: `PASS_WITH_HUMAN_REVIEW`. All four owner links on
  the new entry resolve. The router's documentation review file is missing.
  The repository-wide TODO pattern scan found existing notes; the networking
  specification at line 107 says `9 8 2026 1028 est jorj - todo explain all
  console commands either in here or a centralized document liek
  C:\mimita-priv-v8\docs\specs\terminal\terminal.md`. Suggested
  expansion: establish one authoritative terminal command reference and link
  to it from networking. No TODO was changed.

## Validation and review

- `git diff --check -- docs/specs/open-gameplay-issues.md`: passed. Git printed
  its existing LF-to-CRLF conversion warning for the inbox.
- Confirmed all four new owner links resolve to current files.
- Documentation only; no code, tests, build, live run, profiling, or human
  performance acceptance was performed or claimed.
- Follow-up requires a reproducible local Juggernaut run with actor count,
  build/run identity, FPS, exact physics metric and unit, and comparable timing
  samples. The 240 FPS target must be assessed against measured frame time and
  the relevant CPU/GPU/frame-pacing limit.
