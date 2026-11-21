# issue-23148d00bf5b: Risk: premature hot-layer migration without live game proof

## Human report (verbatim, immutable)

> i worry this might be the super quick to jump to hot reloda architecture issue i had beofre, where im changing every file all at once without testing the actual mimita game which lead to a code base that was un reason-about able


## Triage

### 2026-11-20T02:33:22.961Z — wake-context

Summary: Risk watch: broad migration must be sliced and live-proven
Owner: wake-context
Priority: high

## Scheduled independent review

### 2026-11-21T22:49:57.371Z — live-build-gui-timer-resource-audit

A bounded independent review was queued as wake wake-4d0ffe4ad80d after 187 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (risk-open-live-proof-missing)

### 2026-11-21T22:55:53.464Z — issue-review-sweeper

Reviewer: issue-review-sweeper

**Independent bounded review — issue-review-sweeper, wake-4d0ffe4ad80d, 2026-11-21T22:5xZ.** No source edits, no build, no close, no fix claim.

**Observed facts**

- Issue state/readiness: `issue-23148d00bf5b` is `triaged`, readiness score 1/8 (`ready=false`), missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, and risk. It has 0 tasks, no related_build, no related_run, no proofs, and no linked artifacts. Its only content is the user quote, a triage summary, and the 22:49:57Z scheduling note for this review. This is a standing risk watch, not a defect with its own artifact.
- The in-scope candidate slice is bounded, not a repo-wide migration. `git diff --stat` shows 9 tracked files changed with 337 insertions/30 deletions; the behavior-bearing sources are `src/mapeditor/map-editor.cpp` (+274/-6), `src/mapeditor/map-editor-commands.cpp` (+42/-17), `src/mapeditor/map-editor.h` (+8), `src/gamemode/map-config.cpp` (+6/-1), plus a 1-line behavior-neutral comment in `src/effects/zombie-tower-effects.cpp`. Capability issue `issue-d69d22e34259` owns 3 open tasks with written acceptance (`xxity-stacked-city-blocks`, `xxity-world-editor-settings`, `xxity-world-fog`), so the work is sliced rather than "changing every file at once."
- Build generation 436 (attempt `game-dll-20261121T222152398558Z-ad2cd0`, revision `631d0fcb...`, success at 2026-11-21T22:36:55Z, total 903.413s) compiled exactly that slice: 14 compiled, 628 skipped, 4 changed paths. The preceding attempt `...729a42` failed at link on undefined references to `MapEditor::setGenerationZone*` from a partially applied edit; the retry succeeded. That retry is already recorded append-only in `issue-136d4553ff92` (2026-11-21T22:2xZ entry) and is not re-filed here.
- Live run `20261121_224058` (client PID 24192) loaded `build\game\generation-436\game-g436.dll` (`game.host.probe.started`, "generation loaded") and ran map `payload1`; client journal also records `map-editor.config.loaded` for `config/mapeditor.json`. It records no `map-editor.generation-zone.*.apply` events, and a repo-wide `rg` over `logs/` finds zero such events. The new emitters at `src/mapeditor/map-editor.cpp:2717,2752,2790,2823` were never exercised in a journal.
- The owning changelog (`docs/changelog/2026-11-21/20261121_222546-infinite-xxity-generation-zone-map-editor-controls.md`) states: "Runtime proof is pending the active candidate DLL build; stacked district content and map-owned fog remain open tasks" with `Status: in-progress`.

**Inference / uncertainty**

- High confidence: the "live game proof" half of this risk is currently UNMET for the gen436 map-editor controls. Compilation, DLL activation, and a general client/server payload1 run exist; nothing shows the new controls applied, saved, reloaded, or bounded.
- Medium confidence: this is a bounded, in-progress slice rather than the feared broad hot-layer migration. The file count, task slicing, single owner, and 14-of-642 compile set all contradict a whole-tree rewrite.
- Unknown: whether the controls work at all. "Not yet tested" and "tested and broken" are indistinguishable from the current evidence.

**Contrary evidence**

- The game runs on gen436: client loaded the DLL, loaded `payload1`, completed `network.client.connect.preflight` with `reason_code=preflight_ready` / `result=ok`, and produced `client.first-frame`. This shows the generated binary is live and stable in the sampled run; it does not prove the map-editor feature.
- The capability issue has full readiness (8/8) and explicit acceptance for each slice, so the risk is being governed, not ignored.

**Next discriminating check**

- In an isolated dev-loop run pinned to gen436 or newer, clone the Infinite XXITY project, open Map Editor, and exercise each new control path (`zone_field`, generation origin, cell size, stream radius with spacing mode, repeat axes); save and reload. Require client events `map-editor.generation-zone.origin.apply`, `.cell-size.apply`, `.radius.apply`, and `.repeat-axes.apply` carrying the changed values and the same `run_id`/generation, plus the persisted config/worldgen values after reload. If any event is missing, record the first missing stage and owner rather than treating "pending proof" as a pass. Coordinate with `infinite-city-scale-fog-editor`, which still owns all three `issue-d69d22e34259` tasks.

**Process review**

- None found beyond what is already recorded. The avoidable link retry from the mid-edit snapshot (`...729a42`) and the 60s flat build-wait evidence are already captured in `issue-136d4553ff92`; the unproven slice is expected for in-progress capability work and is explicitly declared pending by its owner. Appending either again would duplicate existing records.
