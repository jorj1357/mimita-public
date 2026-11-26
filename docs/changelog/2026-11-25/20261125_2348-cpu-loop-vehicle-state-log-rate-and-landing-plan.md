// 2026-11-25 18:48 EST
/* purpose
* record a read-only CPU/resource investigation plus issue routing for the
* Rocket League handoff continuation
* separate source, runtime, build, and human-acceptance claims
* no code was edited, no build was requested, no process was controlled
*/

# CPU loop: vehicle.state per-tick log cost and landing/air-roll plan

# Task

- Task ID: ses_deaebec82ffeNlJnFBa42ZhGvi
- Owner: cpu-optimization-pass-loop
- Focus: continue docs/handoffs/20261125_rocket-league-mode-handoff.md from the CPU/resource lane
- Status: complete (advisory; no code change)
- Date, time, timezone: 2026-11-25T23:48:00+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: f9d94c5bb3cffd8bb15899cdda30eea3264c4a66
- Final commit: f9d94c5bb3cffd8bb15899cdda30eea3264c4a66 (not committed)

# Requested behavior

- User context ctx-147edc1b01e5: "can u continue this Handoff written: docs/handoffs/20261125_rocket-league-mode-handoff.md"
- User context ctx-3661c17cacfa (arrived during the session): landing not flat, wheels caught under the ground, car settles on a corner, and "air roll in air, left stick pushed right = car turns left ... flip the directions", with a request for events that prove landing/orientation.

# What this session did

- Read-only analysis of the live free-play run 20261125_233748 (server PID 26028, client PID 25532). Did not stop, restart, or command it.
- Appended CPU/resource evidence to issue-ed3ac775283d.
- Appended a source-grounded landing/air-roll plan to issue-e5a086b0a441.
- Linked ctx-3661c17cacfa to issue-e5a086b0a441 and moved ctx-147edc1b01e5 to issue-e5a086b0a441.
- Sent two bounded, issue-linked messages to rocket-league-input and vehicle-ball-contact.
- No source edit, no build request, no dev-loop start, no process control.

# Source evidence

- src/vehicles/vehicle-system.cpp:2517 writes vehicle.state every call inside updateArcadeControls, at StructuredLevel::Important, with no rate gate.
- src/vehicles/vehicle-system.cpp:2610 rate-gates vehicle.telemetry only (tick - lastTelemetryTick >= 30).
- src/vehicles/vehicle-system.cpp:1338-1370 maps R1/powerslide to roll with stickX + directionalRoll; :2362-2366 applies right*aPitch - forward*aRoll + up*aYaw.
- src/vehicles/vehicle-system.cpp:3042-3076 damps angular velocity whenever grounded >= 1 and applies the same damping twice when all four wheels are grounded.

# Runtime evidence (read-only, another agent's run)

- Journal logs/2026-11-25/20261125_233748/server-01/events.jsonl.
- 8,333 vehicle.state lines of 10,854 parseable events (76.8%), 13.43 MB of 19.0 MB parseable bytes (70.7%), avg 1,690 B/line, span 34.85 s = 239.1/s for 2 vehicles (~120 Hz each).
- Passive 10.17 s sample after the client exited (1 idle vehicle): server journal grew 633.3 KB/s; server PID 26028 used 56.2% of one core = 4.69% of the 12-CPU host.
- Observed server journal size 81.79 MB; parseable JSON accounts for only ~19 MB, so segment-size attribution is unknown.
- Caveat: exact CPU share attributable to logging is not isolated; the above is correlation, not cause.

# Build evidence

- None. No build was requested or run by this session.

# Human acceptance

- None claimed.

# Files changed (tracked working tree)

- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md (appended evidence via coordination tool)
- docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md (appended plan via coordination tool)
- docs/changelog/2026-11-25/20261125_2348-cpu-loop-vehicle-state-log-rate-and-landing-plan.md (this file)

# Recommended next step

- Decide owner for a configurable vehicle.state rate gate (default 4 Hz, proof mode per-tick) and measure same-run journal bytes/s plus server PID CPU before/after.
- For landing: add vehicle.airroll.direction, vehicle.landing, per-wheel contact-depth events, then test the damping hypothesis at :3042-3076 one change at a time against a pinned scenario.
