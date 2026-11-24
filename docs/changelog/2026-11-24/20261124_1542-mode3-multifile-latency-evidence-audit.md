# Mode 3 multi-file latency evidence audit

## Scope

Read the existing root-dev-loop game-DLL summaries and refreshed the current
runtime/build state. No C++ inputs, build scripts, build requests, game
processes, or runtime settings were changed in this pass.

## Historical build evidence

The audit found 19 completed game-DLL summaries with 10–22 changed source
paths: 16 succeeded and 3 failed. None of the nine successful 15-path attempts
finished within 10 seconds.

- Fastest 15-path attempt: `game-dll-20261123T114204890604Z-28389a`, 15.098 s;
  16 translation units compiled, 627 skipped; changed-source preflight 7.042 s,
  compile metric 7.369 s, LLD link 2.505 s, object-cache scan 0.043 s.
- A 19-path attempt: `game-dll-20261123T132647317966Z-638ff7`, 34.559 s;
  20 translation units compiled, 623 skipped; preflight 17.732 s, compile
  metric 18.620 s, link 5.365 s, cache scan 1.808 s.
- Across 16 successful attempts in this 10–22-path range, medians were
  48.013 s total, 29.089 s compile metric, 6.125 s link, 27.487 s changed-source
  preflight, and 2.095 s object scan. Phase definitions overlap; these medians
  must not be summed.

The known 99.428-second one-file attempt gives a more detailed attribution:
60.521 s compiling the changed source, 22.383 s in LLD over 644 object inputs
(492,383,306 bytes), and 0.055 s object-cache scan. Together these results make
cache-scan tuning secondary to changed translation-unit work and the full-DLL
link for the measured attempts. Header fanout still matters when it causes
extra TUs to compile, but does not explain the dominant measured time here.

## Current runtime and safety state

At the latest read-only status check (2026-11-24 15:38 UTC), root dev-loop had
no active build or pending batch. It reported Bloodtest/slaughterhouse1, host
DLL generation 594, and stale build status. Server PID 25840 was active; client
PID 23360 was alive but its journal had stopped at 15:31:08 UTC and was
99,999,574 bytes. The fresh server journal showed host generation 594, NPC
module generation 593, two NPCs, zero active players, and Bloodtest waiting
for required clients. This is not a verified connected pair and cannot serve
as Juggernaut acceptance evidence. A separate OS memory point at 15:42:51 UTC
showed 2.68 GiB available; no continuous current sample was taken.

## Recommendation and next proof

The measured monolithic path has no demonstrated 10-second 15–20-file result.
The next latency architecture experiment should build and publish one bounded
behavior-domain candidate DLL through the root dev-loop, linking its small
stable ABI/module inputs rather than all 644 game objects. Keep the MinGW
whole-game DLL as fallback. Preserve state across activation through the
existing versioned export/import contract; do not assume arbitrary C++ object
layout changes migrate safely. The ModuleHost owner must confirm the rebind
implementation and source ownership before this agent edits those paths.

Before a 15–20-file candidate timing can count toward the goal, require a fresh
connected Juggernaut Horde server/client pair, 67 NPCs, matching host and NPC
module generation, writable fresh journals, and an automatically sampled host
plus process CPU/RAM sidecar for the entire build. Report build completion and
in-game activation separately. The result must include a changed NPC behavior
event in the live journal; build publication alone is not proof.

The detailed timing distribution and the ModuleHost/runtime coordination
request were appended to issue `issue-136d4553ff92`. This evidence is
historical and does not establish that resource pressure caused the observed
latency.
