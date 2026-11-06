# Player Trail Camera Visibility

Time created: 2026-10-26 (reported in chat; exact wall time unavailable)
Time last updated: 2026-10-26

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/graphics/graphics-presentation.md`

Related changelog:
`docs/changelog/2026-10-26/20261026_003225-duel-outline-team-identity.md`

---

## Regression Occurrence 1

### Observed

The user reported that trails appear to behave opposite to the camera view: they are absent when in the user's field of view and appear when outside it. The current journal's earlier `player_visuals.trail` event uses `state=drawn` when geometry is only appended to a CPU batch; that event does not establish a GPU draw occurred.

### Expected Behavior

Configured player trails remain visible from the camera-facing side when the actor trail geometry is in view.

### Actual Behavior

Trail visibility was reported as inverted relative to camera/FOV. A previous diagnostic could report `drawn` before the batch was submitted.

### Cause Assessment

Source inspection found camera-facing ribbon vertices built from `cross(direction, cam.front)`, and the trail batch inherited face-culling state. This winding/culling interaction is a plausible explanation for angle-dependent disappearance. The precise visual cause is not confirmed by a current-build journal event or visual observation.

### Attempted Fix 1

Change: reverse the camera cross-product ordering, disable `GL_CULL_FACE` around `drawPlayerTrailBatch` and restore its previous state, rename the actor event to `batched`, and add a once-per-second event after the GPU submission call.

Build: 3357 succeeded and the dev loop reported it current.

Runtime evidence at initial review: no `player_visuals.trail.batch` event was available because the event used a disabled logging category. Follow-up evidence is recorded below.

Human visual acceptance: pending.
## Follow-up Evidence

The `player_visuals.trail.batch` event was initially absent because its new submission log used the `RENDERING` category while `config/debuglogger.json` sets that category to `off`. Changed the confirmation event to the enabled `GENERAL` category used by the existing trail-state diagnostic.

Build 3360 runtime evidence: client PID 30292 emitted `player_visuals.trail.batch` repeatedly, including `2026-10-26T00:43:50.711Z` with `draw_submitted=true`, `face_culling=false`, and `vertex_count=72`. This confirms geometry reached `glDrawArrays` with face culling disabled. Build 3363 contains the same trail code, but its new client has not logged a trail batch. Human visual review remains pending.
