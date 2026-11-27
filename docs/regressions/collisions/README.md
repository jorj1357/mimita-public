# Collisions regressions

Domain regression index. Each linked record owns its chronology and evidence.

Entries: 22
---

## Evidence-backed recurring problems and fixes

| Recurring pattern | Evidence-backed response |
|---|---|
| Contact geometry is missing, misidentified, or bypassed. | Preserve solid identity and trace map geometry through broadphase and contact response. |
| Bounded searches or body-collider work dominate hot queries. | Measure the owner; keep cached broadphase and bound or remove redundant work. |

---

## Entry 1 — Source Air Movement and Human Understanding

- Recorded: 2026-09-22T20:54:22Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/movement/movement.md
- Detail: [source-air-movement-REG-2026-09-22.md](source-air-movement-REG-2026-09-22.md)

## Entry 2 — Limb contact and touch-reset movement regression

- Recorded: 2026-09-22T23:04:52Z
- Status: UNRESOLVED
- Summary: docs/specs/movement/movement.md
- Detail: [limb-contact-ability-REG-2026-09-22.md](limb-contact-ability-REG-2026-09-22.md)

## Entry 3 — Dash / down-dash press-buffer cooldown

- Recorded: 2026-09-23T03:27:04Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/movement/movement.md
- Detail: [dash-down-dash-REG-2026-09-23.md](dash-down-dash-REG-2026-09-23.md)

## Entry 4 — Force Punch NPC contact damage regression

- Recorded: 2026-09-26T22:00:00Z
- Status: ATTEMPTED FIX (1)
- Summary: Force Punch displays its sphere but does not damage NPCs. Increasing config/weapons.json forcepunch.customparams.hitboxRadius to 3.5 does not change that result.
- Detail: [force-punch-npc-REG-2026-09-26.md](force-punch-npc-REG-2026-09-26.md)

## Entry 5 — Walkable Slope and Edge Snag

- Recorded: 2026-09-29T12:43:57Z
- Status: ATTEMPTED FIX (6) — NOT FIXED
- Summary: Current status update (2026-10-24T20:35:50Z): ATTEMPTED FIX (8) — partial code-level correction; uphill route still stops at vertical map face triangle 2440; geometry intent and human acceptance unresolved. The original header status is retained as historical text.
- Detail: [slope-edge-snag-REG-2026-09-29.md](slope-edge-snag-REG-2026-09-29.md)

## Entry 6 — Destructible Crater Penetration

- Recorded: 2026-10-01T13:43:29Z
- Status: ATTEMPTED FIX (5)
- Summary: docs/specs/destructible-world/destructible-world.md
- Detail: [destructible-crater-penetration-REG-2026-09-30.md](destructible-crater-penetration-REG-2026-09-30.md)

## Entry 7 — Physical Objects Collision and Settling

- Recorded: 2026-10-01T17:37:13Z
- Status: ATTEMPT 14 REVERTED; ROCKET LAUNCHER BOX RESTORED; LIVE RETEST PENDING
- Summary: Attempt 14's capsule collider was followed by a human report that the rocket launcher stuck inside walls; its box collider was restored and made visible. Exact contact cause and live retest remain open. Earlier performance/queue baseline was human-confirmed GOOD at 2026-10-01T22:16:15Z; Attempt 11 full-auto regression was reverted in Attempt 12.
- Detail: [physical-objects-collision-REG-2026-10-01.md](physical-objects-collision-REG-2026-10-01.md)

## Entry 8 — Block seam caused a forward speed collapse

- Recorded: 2026-10-31T11:50:24Z
- Status: IMPLEMENTED IN SOURCE; RUNTIME SEAM PASS; HUMAN FEEL PENDING
- Summary: docs/specs/movement/movement.md
- Detail: [block-seam-speed-REG-2026-10-31.md](block-seam-speed-REG-2026-10-31.md)

## Entry 9 — Down-dash onto a slope sometimes failed to bounce

- Recorded: 2026-10-31T11:50:24Z
- Status: UNRESOLVED
- Summary: docs/specs/movement/movement.md
- Detail: [down-dash-slope-REG-2026-10-31.md](down-dash-slope-REG-2026-10-31.md)

## Entry 10 — NPC final animated pose entered static geometry

- Recorded: 2026-10-31T11:50:24Z
- Status: IMPLEMENTED IN SOURCE; SELFTEST PASS; RUNTIME VISUAL PENDING
- Summary: docs/specs/npc-behavior/npc-behavior.md
- Detail: [npc-final-pose-REG-2026-10-31.md](npc-final-pose-REG-2026-10-31.md)

## Entry 11 — Actor support was merged into static-world floor contact

- Recorded: 2026-11-01T19:49:06Z
- Status: IMPLEMENTED IN SOURCE; FOCUSED SELFTEST PASS; RUNTIME VISUAL PENDING
- Summary: docs/specs/npc-behavior/npc-behavior.md, docs/architecture/collision/collision.md
- Detail: [actor-support-world-REG-2026-11-01.md](actor-support-world-REG-2026-11-01.md)

## Entry 12 — NPC root could remain fully inside a closed block

- Recorded: 2026-11-01T21:40:00Z
- Status: IMPLEMENTED IN SOURCE; RUNTIME RECOVERY VALIDATION PENDING
- Summary: docs/specs/npc-behavior/npc-behavior.md, docs/architecture/collision/collision.md
- Detail: [npc-closed-block-REG-2026-11-01.md](npc-closed-block-REG-2026-11-01.md)

## Entry 13 — NPC wall containment reset loop

- Recorded: 2026-11-03T18:52:13Z
- Status: ATTEMPTED FIX (2)
- Summary: devscripts/scenarios/npcwallcontainmentv1.json fails after template NPC body collision was introduced. Over the 30 second soak the run recorded 1,955 npc.pose.reset events and 116 npc.pose.penetration events. Most resets are novalidpose; final penetration ranges from 0.03 m to
- Detail: [npc-wall-containment-REG-2026-11-03.md](npc-wall-containment-REG-2026-11-03.md)

## Entry 14 — Coolplace limb overlap rolled the whole actor back

- Recorded: unknown
- Status: SOURCE FIX BUILT AND LAUNCHED; LIVE FEEL REVIEW PENDING
- Summary: - Recorded: 2026-10-25T00:14:05Z - Status: SOURCE FIX BUILT AND LAUNCHED; LIVE FEEL REVIEW PENDING
- Detail: [coolplace-limb-rollback-REG-2026-10-25.md](coolplace-limb-rollback-REG-2026-10-25.md)

## Entry 15 — Flat-floor back-face made a straight dash reflect

- Recorded: unknown
- Status: FIXED IN SOURCE; LIVE PLAYBACK HAS ZERO ROLLBACKS; HUMAN FEEL REVIEW PENDING
- Summary: - Recorded: 2026-10-25T14:25:00Z - Time last updated: 2026-10-31T11:50:24Z - Status: FIXED IN SOURCE; LIVE PLAYBACK HAS ZERO ROLLBACKS; HUMAN FEEL REVIEW PENDING - Occurrence 2: SUSPECTED RECURRENCE (2026-10-31); cause unconfirmed
- Detail: [flat-floor-backed-REG-2026-10-25.md](flat-floor-backed-REG-2026-10-25.md)

## Entry 16 — NPC template body colliders caused phantom overlap and floating

- Recorded: 2026-11-03T19:09:00Z
- Status: ATTEMPTED FIX (1) — source changed; runtime review pending
- Summary: Uncommitted headless NPC body-part colliders did not follow the actor root, producing 100 m-scale penetration, 8000 m/s part sweeps, reset loops, and up to three containment searches per NPC per tick.
- Detail: [npc-template-body-REG-2026-11-03.md](npc-template-body-REG-2026-11-03.md)

## Entry 17 — tdmtest3 headless NPC world had no floor

- Recorded: 2026-11-03T17:44:09Z
- Status: ATTEMPTED FIX (1) — solid-id fallback in source; runtime review pending
- Summary: The human tdmtest3 recording shows every NPC at z 50-288 with velocity z -175 and ground=false while the player is at z 2328; the headless server NPC world had no usable support geometry.
- Detail: [tdmtest3-npc-world-REG-2026-11-03.md](tdmtest3-npc-world-REG-2026-11-03.md)

## Entry 18 — Actor snags on connected surfaces

- Recorded: 2026-11-03T19:09:00Z
- Status: UNRESOLVED
- Summary: Live tdmtest3 evidence still contains 2037 speed collapses, 3577 bounce responses, 1330 last-valid rollbacks, and 7 progress stalls; the first divergent contact has not yet been traced.
- Detail: [surface-snagging-issue-REG-2026-11-03.md](surface-snagging-issue-REG-2026-11-03.md)

Correction 2026-11-03: the snag recording was recovered to
`devscripts/scenarios/user/scenario-20261103_195642-tdmsnag.jsonl`. The first
grounded collapses are ticks 8929 and 9534 on surfaces 263/311. Bounce now
applies to walkable slope impacts; runtime replay is still pending.

## Entry 19 — NPC world dropped union triangles and solid identity

- Recorded: 2026-11-04T01:00:00Z
- Status: ATTEMPTED FIX (1) — source changed; runtime review pending
- Summary: Union output used the same sentinel as removed solids, so 6,654 of 51,879 atdm triangles were dropped from the NPC world; solid IDs were not copied, disabling closed-solid containment. NPCs fell through spheres and could not be detected inside blocks.
- Detail: [tdmtest3-npc-world-REG-2026-11-03.md](tdmtest3-npc-world-REG-2026-11-03.md) occurrence 2

## Entry 20 — NPCs embed in FunWorld3 cylinders

- Recorded: 2026-11-08 (human report; exact time not supplied)
- Status: REPORTED / NOT YET REPRODUCED
- Detail: [funworld3-cylinder-embedding-REG-2026-11-08.md](funworld3-cylinder-embedding-REG-2026-11-08.md)

## Entry 21 — World triangle candidate scan

- Recorded: 2026-11-17T16:00:00Z
- Status: REPORTED / NEEDS CURRENT TRACE
- Summary: User reports collision queries checking far more world triangles than the local query needs. Existing records already cover uncached world gathers, full candidate scans, and missing entity broadphase. The current task is to prove whether `appendChunkTrianglesForAABB` is using the world tree or a fallback and to bound candidate counts.
- Detail: [world-triangle-candidate-scan-REG-2026-11-17.md](world-triangle-candidate-scan-REG-2026-11-17.md)

## Entry 22 — Static map collision double-scaled on the server

- Recorded: 2026-11-27T05:34:55Z
- Status: ATTEMPTED FIX (1) — arena2 runtime evidence; full fuzz proof and multi-map check pending
- Summary: The headless builder applied `blockTransform` (which scales a unit cube by `block.size`) to collision geometry already built at `size * 0.5` or in metres. Arena2's goal ceilings became 34 m x 318 m phantom flags at z=6.80; cars spawned at z=7.14, the ball was launched 18-24 m/s by flat floor seams, and the settle fuzz reported 41 side rests. Fixed with `blockPlacement` (no scale), a single solid arena2 floor/ceiling, reference ball friction/restitution, and analytic sphere-world contacts.
- Detail: [static-map-double-scale-REG-2026-11-27.md](static-map-double-scale-REG-2026-11-27.md)
