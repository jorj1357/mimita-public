# Gameplay report intake

Recorded the user’s gameplay report verbatim once in shared context `ctx-804dd80e9f28`; attachment reference: `C:/Users/guita/AppData/Local/Temp/codex-clipboard-b66342fc-a4fc-4810-93ce-36bd3b9218ad.png`. The screenshot is supporting visual evidence only; runtime mode, build and run are unknown.

Existing issues linked: `issue-e0a6bcbacad0` (fullscreen mode 6 full-frame UI stretch) and `issue-3c1894ca50ec` (opaque black/pass-through rendering). The black-rendering issue already contains an unverified proximity-fade/viewmodel state-leak lead.

New unowned issues with claimable investigation tasks:

- `issue-560bf8b3ab41` Bloodtest gore scope, decal cost and performance.
- `issue-479a397848ff` Weapon firing origin and collision consistency.
- `issue-73dd62d8fcae` NPC spawn volleys, wall trapping and actor jitter.
- `issue-c67132298cb4` Bloodtest camera smoothing and viewmodel presentation.
- `issue-79c6ff7cabbe` Bloodtest gore asset orientation and staged corpse response.
- `issue-60e46e12db9b` Vehicle Freeplay controller inputs and presentation.
- `issue-41547751ca1c` Staged Rocket League-style mode acceptance.
- `issue-feef64cab2a7` Metric scale and per-setting configuration-bound review.
- `issue-9845bd0f4fbb` Per-mode camera fade and weapon effect presentation.

All new tasks were verified unassigned and their issues passed readiness checks. The global request to remove bounds was recorded as a proposal for per-setting review, not as an instruction to remove all limits. Performance figures remain unpinned user observations; no causal claim was made. No source/spec changes, builds, tests, or runtime changes were made.
