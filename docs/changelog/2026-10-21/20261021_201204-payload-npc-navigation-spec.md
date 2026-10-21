// 2026-10-21T20:12:04Z

# Task

- Task ID: Payload NPC navigation and networking clarity
- Summary: Expand Payload's NPC movement and recovery contract, identify map-authored navigation hints as undecided, and state that local play still uses the client-server gameplay path.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-10-21T20:12:04Z, UTC (America/New_York display context)
- Branch: `20261011zt`
- Base commit: `bf9529d348c9661a3c343c94a449476a78c8237c`
- Final commit: `bf9529d348c9661a3c343c94a449476a78c8237c` (no commit created)

# Pre-existing changes

- `docs/specs/weapons/weapons.md` was already modified before this task and was left untouched.
- The Payload draft already contained the previous expanded working specification and the other work documented in `20261021_200029-payload-spec-update.md`; those existing changes were preserved.

# Documents and review

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/operations/task-completion/task-completion.md`, `docs/specs/gamemodes/payload.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/architecture/player-npc-systems/npc-movement.md`, `docs/specs/entity-editor/entity-editor.md`, and `docs/specs/networking/networking.md`.
- Router-listed `docs/doc-review-09-03-2026.md` is absent from this checkout.
- Memory consulted: NPC recovery and server-authoritative actor guidance; current repository source/docs were checked for this task.
- Focused review result: the architecture document already describes goal-to-route-to-movement ownership and bounded recovery; Payload lacked concrete failure distinctions and acceptance cases tied to its map and cart objective. Entity Editor does not currently establish authored escape volumes as a feature, so the spec leaves that as `NEEDS_SPEC_DECISION`.

# Changes

## `docs/specs/gamemodes/payload.md`

- Before at line 331: `NPCs should navigate irregular maps, not only flat rectangular spaces.`
- Before at line 333: `They should be capable of jumping, using ramps and traversing freeform triangle geometry.`
- After, new §12 at lines 331–385: added role-capable 3D route requirements, a six-stage investigation list (goal, route, steering, action, physics, progress), a progress-based stuck definition, recovery pseudocode, bounded/no-teleport requirements, useful failure evidence, map-authored hints as an explicit unresolved Entity Editor choice, and acceptance scenarios for ramps, narrow spaces, dead ends, temporary obstruction, combat interruption, and objective resumption.
- Before at §14 after line 366: `The server decides cart movement and all match results. Clients may ask to push, but the server checks the actor's team, collision body, and overlap with the configured interaction area before changing the cart.`
- After, new paragraph at line 417: `This client/server rule also applies when one person plays alone on one computer: the game client still sends normal gameplay requests to a local server process, and that server still owns NPC decisions, navigation, movement, cart state, collision, and match results. There must not be a separate offline-only gameplay implementation whose rules differ from multiplayer. Clients may predict presentation where the general networking design permits it, but the server's accepted state is authoritative and is what clients render or reconcile to. See `../networking/networking.md` for the shared connection and authority contract.`
- Reason: make expected NPC behavior and failure handling harder to misread while preserving open implementation and map-authoring decisions. Clarify that a local server is still the server-authoritative architecture.

# Related TODO findings

A focused documentation scan found many unrelated TODO notes throughout `docs/`; they were not edited. The directly relevant remaining notes include:

- `docs/architecture/player-npc-systems/npc-movement.md:22`: `9 8 2026 1019 est jorj todo - explain` — expand the overview to plainly connect the behavior goal, route planner, movement execution, and the stuck-recovery evidence chain now summarized in Payload §12.
- `docs/architecture/vision-mentality/mapping.md:1`: `2026 10 21 1550 jorj todo epxlain` — explain the blockout, playtest, and replace-boxes-with-finished-geometry map workflow already referenced by Payload.
- `docs/specs/entity-editor/entity-editor.md:1`: `Cart area: The cart gets a tiny origin point, and its push/block area is placed using settings measured from that point. Your starting size works out to about 3 m × 2.25 m × 1.5 m. If a defender overlaps that area, the cart stops. The exact shape and how it attaches to the origin are still open choices.` — clarify whether named NPC navigation hints/recovery volumes belong in map data, how they are edited, and that they guide rather than teleport actors. This is a proposed follow-up, not an existing feature.

# Validation and evidence

- `git diff --check -- docs/specs/gamemodes/payload.md` passed with no output.
- Confirmed the referenced networking and Entity Editor documents exist.
- Reviewed the patch to ensure pseudocode is labeled as desired behavior and not an implementation claim.
- No tests, build, runtime scenario, or human playtest were run; this was documentation-only and no runtime behavior is claimed.

# Human review still needed

- Decide whether the Entity Editor should support named NPC navigation hints or recovery volumes and define their schema/semantics.
- Human playtesting is needed to establish actual stuck thresholds, recovery timing, and whether the acceptance routes feel good.
- Confirm the local play launch currently follows the intended client-to-local-server path before using this specification as evidence of implementation.
