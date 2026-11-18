# Fun Counter Strike NPC objective priority

- Task/session: 2026-11-18 FCS NPC objective priority
- Owner: Codex
- Status: PASS_WITH_HUMAN_REVIEW
- Time: 2026-11-18T09:35:29Z (2026-11-18 04:35:29 EST)
- Branch/base: `checkpoint/one-checkout-condensation-20261111` / `43b89938b8aa29e3bffab982a544972f8dad7968`

## Request and specification

Make FCS NPCs prioritize reaching bomb sites: Ts travel to their chosen site to
plant, then defend; CTs defend sites and stop plants. Inspect live NPC position
events to confirm movement.

The routed contracts are `docs/specs/gamemodes/funcounterstrike.md` (NPC
objective strategy) and `docs/specs/npc-behavior/npc-behavior.md` (Fun Counter
Strike behavior-profile contract). They require a stable round site choice for
Ts, all living Ts to travel there, and site defense/plant prevention by CTs.
The implementation disagreed because objective travel and defense had weight
1.0, allowing combat utility scores to take precedence. The FCS config parser
accepts this weight up to 5.0.

## Change

In `config/gamemodes/funcounterstrike.json`, `npc_travel.objective_bias` changed
from `1.0` to `5.0` (current line 293). This weights objective travel/defense
above combat scores while retaining the existing shared utility and navigation
owners. The pre-existing FCS HUD settings in this file were preserved.

## Live evidence

- Run: `20261118_092825`, server PID `34640`, map `dust2cyberiav4`, mode
  `funcounterstrike`.
- Server journal:
  `logs/2026-11-18/20261118_092825/server-01/events.jsonl`.
- The journal records `team.site-choice` selecting A in rounds 0–2 and B in
  rounds 3–4. It records Ts selecting `MoveToObjective` and CTs selecting
  `DefendSite`; recent entries include live actor positions and objective
  targets. Example: T actor `100016` was recorded at
  `[-633.25, -25.28, 2356.03]` with a route destination near site B
  `[-909.78, -91.33, 2351.31]`; other Ts appeared near their selected site.
- Movement is not uniformly successful. The same run records `blocked` and
  `initial` `npc.nav-plan-failed` events and `no_progress`
  `npc.movement-progress-failed` events. Some observed actors remained far from
  their chosen site. Thus the journal proves objective-goal selection and
  movement toward the site for some actors, not arrival by the whole team or a
  completed plant/defend cycle.
- The journal is live and the game DLL reports generation 341. The dev-loop
  status at completion reported a new source build in progress for generation
  338, triggered by the shared checkout's existing C++ edits. No build request
  was issued and no build was awaited. The config change's effective value is
  not directly emitted in a structured event, so runtime attribution to the
  new weight is not independently proven.

## Validation and remaining review

- `git diff --check -- config/gamemodes/funcounterstrike.json`: passed.
- No tests were run or added.
- No executable build evidence for this config change.
- Human gameplay/visual acceptance: not performed.
- Follow-up is needed to correlate the active config value with position and
  route success, and address the observed path/progress failures before claiming
  that every NPC reliably reaches a site and completes its objective.

## Documents and focused review

- Read `docs/ROUTER.md`, `docs/specs/dev-loop/agent-lanes-build-authority.md`,
  `docs/specs/gamemodes/funcounterstrike.md`,
  `docs/specs/npc-behavior/npc-behavior.md`,
  `docs/architecture/player-npc-systems/npc-movement.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/specs/dev-loop/quality-gates.md`,
  `docs/specs/debug-logging/debug-logging.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/regressions/README.md`,
  `docs/regressions/gameplay/README.md`,
  `docs/specs/hotreload/hotreload.md`,
  `docs/handoffs/hot-reload-agent-focus.md`,
  `docs/changelog/2026-11-18/20261118_091404-npc-a-star-frontier-heap.md`,
  `docs/operations/task-completion/task-completion.md`, and
  `docs/architecture/time-and-formatting/time-and-formatting.md`.
- Spec behavior review: mismatch found in objective weighting; config aligned
  to the FCS priority, while arrival and completed objective behavior remain
  unverified.
- No confirmed regression record added; the observed path failures need a
  focused owner diagnosis before attribution.
