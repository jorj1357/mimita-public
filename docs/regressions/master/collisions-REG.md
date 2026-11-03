# Collisions regressions

Append-only master ledger. Each entry links to the detailed dated record that owns the full evidence.

Entries: 15

---

## Entry 1 — Source Air Movement and Human Understanding

- Recorded: 2026-09-22T20:54:22Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/movement/movement.md
- Detail: `docs/regressions/2026-09-22/source-air-movement-human-understanding-REG.md`

## Entry 2 — Limb contact and touch-reset movement regression

- Recorded: 2026-09-22T23:04:52Z
- Status: UNRESOLVED
- Summary: docs/specs/movement/movement.md
- Detail: `docs/regressions/2026-09-22/limb-contact-ability-reset-REG.md`

## Entry 3 — Dash / down-dash press-buffer cooldown

- Recorded: 2026-09-23T03:27:04Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/movement/movement.md
- Detail: `docs/regressions/2026-09-23/dash-down-dash-press-buffer-cooldown-REG.md`

## Entry 4 — Force Punch NPC contact damage regression

- Recorded: 2026-09-26T22:00:00Z
- Status: ATTEMPTED FIX (1)
- Summary: Force Punch displays its sphere but does not damage NPCs. Increasing config/weapons.json forcepunch.customparams.hitboxRadius to 3.5 does not change that result.
- Detail: `docs/regressions/2026-09-26/force-punch-npc-contact-REG.md`

## Entry 5 — Walkable Slope and Edge Snag

- Recorded: 2026-09-29T12:43:57Z
- Status: ATTEMPTED FIX (6) — NOT FIXED
- Summary: Current status update (2026-10-24T20:35:50Z): ATTEMPTED FIX (8) — partial code-level correction; uphill route still stops at vertical map face triangle 2440; geometry intent and human acceptance unresolved. The original header status is retained as historical text.
- Detail: `docs/regressions/2026-09-29/slope-edge-snag-REG.md`

## Entry 6 — Destructible Crater Penetration

- Recorded: 2026-10-01T13:43:29Z
- Status: ATTEMPTED FIX (5)
- Summary: docs/specs/destructible-world/destructible-world.md
- Detail: `docs/regressions/2026-09-30/destructible-crater-penetration-REG.md`

## Entry 7 — Physical Objects Collision and Settling

- Recorded: 2026-10-01T17:37:13Z
- Status: ATTEMPTED FIX (13)
- Summary: Performance/queue baseline: human-confirmed GOOD at 2026-10-01T22:16:15Z (Attempt 10: sustained 2 fps gone; no noticeable dip from normal shooting). Attempt 11 regressed full-auto (interval-8 batch-of-24); reverted in Attempt 12.
- Detail: `docs/regressions/2026-10-01/physical-objects-collision-REG.md`

## Entry 8 — Block seam caused a forward speed collapse

- Recorded: 2026-10-31T11:50:24Z
- Status: IMPLEMENTED IN SOURCE; RUNTIME SEAM PASS; HUMAN FEEL PENDING
- Summary: docs/specs/movement/movement.md
- Detail: `docs/regressions/2026-10-31/block-seam-speed-collapse-REG.md`

## Entry 9 — Down-dash onto a slope sometimes failed to bounce

- Recorded: 2026-10-31T11:50:24Z
- Status: UNRESOLVED
- Summary: docs/specs/movement/movement.md
- Detail: `docs/regressions/2026-10-31/down-dash-slope-bounce-REG.md`

## Entry 10 — NPC final animated pose entered static geometry

- Recorded: 2026-10-31T11:50:24Z
- Status: IMPLEMENTED IN SOURCE; SELFTEST PASS; RUNTIME VISUAL PENDING
- Summary: docs/specs/npc-behavior/npc-behavior.md
- Detail: `docs/regressions/2026-10-31/npc-final-pose-penetration-REG.md`

## Entry 11 — Actor support was merged into static-world floor contact

- Recorded: 2026-11-01T19:49:06Z
- Status: IMPLEMENTED IN SOURCE; FOCUSED SELFTEST PASS; RUNTIME VISUAL PENDING
- Summary: docs/specs/npc-behavior/npc-behavior.md, docs/architecture/collision/collision.md
- Detail: `docs/regressions/2026-11-01/actor-support-world-contact-sink-REG.md`

## Entry 12 — NPC root could remain fully inside a closed block

- Recorded: 2026-11-01T21:40:00Z
- Status: IMPLEMENTED IN SOURCE; RUNTIME RECOVERY VALIDATION PENDING
- Summary: docs/specs/npc-behavior/npc-behavior.md, docs/architecture/collision/collision.md
- Detail: `docs/regressions/2026-11-01/npc-closed-block-interior-REG.md`

## Entry 13 — NPC wall containment reset loop

- Recorded: 2026-11-03T18:52:13Z
- Status: ATTEMPTED FIX (2)
- Summary: devscripts/scenarios/npcwallcontainmentv1.json fails after template NPC body collision was introduced. Over the 30 second soak the run recorded 1,955 npc.pose.reset events and 116 npc.pose.penetration events. Most resets are novalidpose; final penetration ranges from 0.03 m to
- Detail: `docs/regressions/2026-11-03/npc-wall-containment-REG.md`

## Entry 14 — Coolplace limb overlap rolled the whole actor back

- Recorded: unknown
- Status: SOURCE FIX BUILT AND LAUNCHED; LIVE FEEL REVIEW PENDING
- Summary: - Recorded: 2026-10-25T00:14:05Z - Status: SOURCE FIX BUILT AND LAUNCHED; LIVE FEEL REVIEW PENDING
- Detail: `docs/regressions/2026-10-25/coolplace-limb-rollback-REG.md`

## Entry 15 — Flat-floor back-face made a straight dash reflect

- Recorded: unknown
- Status: FIXED IN SOURCE; LIVE PLAYBACK HAS ZERO ROLLBACKS; HUMAN FEEL REVIEW PENDING
- Summary: - Recorded: 2026-10-25T14:25:00Z - Time last updated: 2026-10-31T11:50:24Z - Status: FIXED IN SOURCE; LIVE PLAYBACK HAS ZERO ROLLBACKS; HUMAN FEEL REVIEW PENDING - Occurrence 2: SUSPECTED RECURRENCE (2026-10-31); cause unconfirmed
- Detail: `docs/regressions/2026-10-25/flat-floor-backed-face-dash-REG.md`

## Entry 16 — NPC template body colliders caused phantom overlap and floating

- Recorded: 2026-11-03T19:09:00Z
- Status: ATTEMPTED FIX (1) — source changed; runtime review pending
- Summary: Uncommitted headless NPC body-part colliders did not follow the actor root, producing 100 m-scale penetration, 8000 m/s part sweeps, reset loops, and up to three containment searches per NPC per tick.
- Detail: `docs/regressions/2026-11-03/npc-template-body-collision-REG.md`

## Entry 17 — tdmtest3 headless NPC world had no floor

- Recorded: 2026-11-03T17:44:09Z
- Status: ATTEMPTED FIX (1) — solid-id fallback in source; runtime review pending
- Summary: The human tdmtest3 recording shows every NPC at z 50-288 with velocity z -175 and ground=false while the player is at z 2328; the headless server NPC world had no usable support geometry.
- Detail: `docs/regressions/2026-11-03/tdmtest3-npc-world-fall-REG.md`

## Entry 18 — Actor snags on connected surfaces

- Recorded: 2026-11-03T19:09:00Z
- Status: UNRESOLVED
- Summary: Live tdmtest3 evidence still contains 2037 speed collapses, 3577 bounce responses, 1330 last-valid rollbacks, and 7 progress stalls; the first divergent contact has not yet been traced.
- Detail: `docs/regressions/2026-11-03/surface-snagging-REG.md`
