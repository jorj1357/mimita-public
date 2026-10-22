# Reconnect memory report and low-resource accessibility requirement

- Time: 2026-10-22T17:47:08Z (2026-10-22 13:47:08 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD at start: `853366427f4285e43a21005e82838f000260668d`
- Result: `PASS_WITH_HUMAN_REVIEW` — the reconnect memory report is recorded for investigation and low-resource use is explicit in accessibility guidance; the report remains unverified.

## Scope and source state

Recorded the user's approximate report of memory increasing during reconnect
attempts, with a sustained high reading afterward. The values are explicitly
marked approximate and units are unknown. The issue asks for a bounded live
measurement and an owner trace; it does not call this a confirmed leak or
assign a cause.

Updated the accessibility architecture to make resource efficiency an
accessibility requirement across CPU, GPU, memory, storage, and network use.
It now states that low-power devices and poor or unstable connections should
remain usable, that features should scale up on stronger systems, and that
optional detail should yield before core gameplay and essential information.
This aligns with the performance specification's existing requirement that
the engine remain usable on low-power computers and preference for low memory
use, predictable execution, and low allocation counts.

At task start, these changes already existed and were preserved:
`config/accounts/default.json`, `config/analytics.json`,
`config/audio/music-settings.json`, `docs/jorj-docs/20261022plan.md`,
`docs/specs/open-gameplay-issues.md`, `shaders/basic.vert`,
`src/effects/effect-part-blood.cpp`, `src/effects/effect-part-particles.cpp`,
`src/engine/engine-tick-render.cpp`, `src/engine/engine-tick-ui.cpp`,
`src/engine/engine-tick.cpp`, `src/entities/player-render-mesh.cpp`,
`src/entities/player-render.cpp`, `src/network/multiplayer-context.h`,
`src/network/multiplayer-interpolation.cpp`, `src/render/render-player.cpp`,
and `src/render/render-player.h`. Three untracked changelogs were also present:
`docs/changelog/2026-10-22/20261022_171528-gameplay-issues-intake.md`,
`docs/changelog/2026-10-22/20261022_171650-zombie-tower-editor-blocker.md`,
and `docs/changelog/2026-10-22/20261022_172026-live-blender-map-authoring-intake.md`.
The issue inbox had concurrent/pre-existing Zombie Tower map-authoring rows;
they were preserved. This task changed only the inbox additions below, the
accessibility document, and this changelog.

## Exact changes

### `docs/specs/open-gameplay-issues.md`

Intake table, line 112. Before, the new row did not exist; the surrounding
existing JSON configuration row was:

```markdown
| JSON configuration range and error feedback | The player wants JSON settings to accept very large values (example: `-9999999999999999999999999999` through `999999999999999999999999999999999`) and wants invalid or unsupported values to be obvious, including which file and setting caused the problem. | Open design issue. Decide safe per-setting numeric ranges and whether out-of-range values are accepted, clamped, or rejected while preserving the last valid configuration. Record a categorized error in `events.jsonl` with the config path, field, and reason, then surface a clear, deduplicated in-game tip through the existing tips presentation path. Exact wording, severity, and repeat/throttle behavior remain open. | [`../architecture/json-configuration/json-configuration.md`](../architecture/json-configuration/json-configuration.md), [`debug-logging/debug-logging.md`](debug-logging/debug-logging.md), [`gui/gui-general.md`](gui/gui-general.md) |
```

After, line 112 contains this new row:

```markdown
| Memory use grows sharply during reconnect attempts | The player reports RAM use rising by roughly 100 MB per second while reconnecting, then staying around 4,180 (exact value and units uncertain); the player says the figures are approximate and is concerned about the size of the increase. | User-reported, not reproduced; cause and exact memory units/measurements are unknown. Measure process memory across a bounded reconnect sequence, identify which owner retains or allocates the growing memory, and verify whether usage is released or remains elevated after reconnect succeeds or stops. Do not infer a leak or blame a component from the estimate alone. | [`networking/networking.md`](networking/networking.md), [`performance/performance.md`](performance/performance.md), [`../architecture/accessibility/accessibility.md`](../architecture/accessibility/accessibility.md) |
```

Issue index, line 122. Before, this new index row did not exist. After:

```markdown
| RAM use reportedly climbs rapidly during reconnect attempts and remains high | Approximate report: about 100 MB/s growth while reconnecting and around 4,180 afterward; exact amount and units are uncertain | Needs a bounded live reconnect measurement and memory-owner trace before identifying cause or remedy | [`networking/networking.md`](networking/networking.md), [`performance/performance.md`](performance/performance.md), [`../architecture/accessibility/accessibility.md`](../architecture/accessibility/accessibility.md) |
```

### `docs/architecture/accessibility/accessibility.md`

Core goal, lines 33–40. Before, the section ended with the existing sentence
`platform adapters separated so the same game behavior can travel across
devices.` No resource-efficiency requirement followed it. After, this was
added:

```markdown
Resource efficiency is a core accessibility requirement. Use the lowest
practical CPU, GPU, memory, storage, and network resources for the behavior a
feature needs; avoid unnecessary work, allocations, retained data, and
transmitted data. Do not assume a player has a powerful GPU or CPU, abundant
RAM, a new device, or a fast and reliable internet connection. MiMITA should
remain responsive and useful on low-power devices and poor or unstable
connections, while scaling up on stronger systems. Optional detail and
nonessential work should yield before core gameplay and essential information.
```

Performance and platform boundaries, lines 92–97. Before, the paragraph ended
with `simulation deterministic and separate from rendering frame rate.` No
resource measurement or retry-growth requirement followed it. After, this was
added:

```markdown
Choose designs and implementations for minimal practical resource use across
the supported device range. Measure CPU, GPU, memory, storage, and network
costs on representative low-power and high-power systems. Treat large or
unbounded resource growth, including during retries and reconnection, as an
accessibility and performance issue to investigate; do not assume high memory
or network capacity will hide it.
```

## Documents and focused review

- Read `docs/ROUTER.md`, `docs/architecture/accessibility/accessibility.md`,
  `docs/specs/performance/performance.md`, the reconnect sections of
  `docs/specs/networking/networking.md`,
  `docs/specs/open-gameplay-issues.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/skills/documentation-checker-v1.md`,
  `docs/skills/spec-behavior-review-v1.md`, and
  `docs/skills/efficiency-checker-v1.md`.
- The router requests `docs/doc-review-09-03-2026.md`; that file is absent in
  this checkout.
- Specification and Behavior Review: `PASS_WITH_HUMAN_REVIEW`. The existing
  performance spec already says the engine should remain usable on low-power
  computers and prefers low memory use; this addition makes the user's
  device-wide CPU/GPU/memory/storage/network requirement explicit in the
  accessibility owner. The reconnect observation has not been measured, so
  the issue remains an investigation item.
- Efficiency Checker: `NOT_APPLICABLE` to code review because this task made no
  code changes. The existing performance specification was read; no code cost
  or root cause was inferred.
- Documentation Checker: `PASS_WITH_HUMAN_REVIEW`. The three new issue links
  resolve. The whole-docs TODO scan found 186 matches to its broad pattern.
  Related routed TODO: `docs/specs/networking/networking.md:107` says
  `9 8 2026 1028 est jorj - todo explain all console commands either in here or a centralized document liek C:\mimita-priv-v8\docs\specs\terminal\terminal.md`.
  Suggested expansion: make the current terminal command reference the single
  authoritative location and link to it from networking. It was not edited.

## Validation and review

- `git diff --check -- docs/architecture/accessibility/accessibility.md
  docs/specs/open-gameplay-issues.md`: passed. Git printed its existing
  LF-to-CRLF conversion warning for the issue inbox.
- Confirmed the new issue links resolve to networking, performance, and
  accessibility documents.
- Documentation/specification only: no source code, build, tests, live
  reconnect run, memory measurement, or human gameplay acceptance was
  performed or claimed.
- Human/runtime follow-up is needed to reproduce reconnect growth, measure
  memory with confirmed units, and identify the first owner retaining or
  allocating the growth. Low-resource expectations are now written policy;
  this intake does not establish performance on any device or connection.
