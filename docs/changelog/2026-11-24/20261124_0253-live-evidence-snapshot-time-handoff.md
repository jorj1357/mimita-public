# Live evidence, snapshot, and time/order documentation handoff

## Scope

This was a documentation and read-only tooling session. It did not implement
session snapshots, time overrides, or the primitive-state engine. It recorded
the Juggernaut Horde human acceptance, added a working read-only live evidence
observer, and wrote the handoff and specifications for the next agents.

## Gold behavior: Juggernaut Horde felt overwhelming

- Added `docs/gold/2026-11-24-juggernaut-horde-overwhelming-human-evidence.md`.
- Recorded the user’s report that Juggernaut Horde NPCs were overwhelming and
  “gold behavior.”
- Live session observed through the evidence observer:
  run `20261124_022440_01`, Juggernaut Horde, `dust2cyberiav4`, 67 NPCs / 1
  player, server PID 33892, journal
  `logs/2026-11-24/20261124_022440_01/server-01/events.jsonl`.
- Earlier preserved candidate: run `20261123_195307`, 67 NPCs / 1 player,
  server PID 22184.
- Linked from `docs/features/npc-behavior.md` and appended a human-acceptance
  review to `issue-be1adb51b6ac`.
- Evidence class is human acceptance, not a numeric difficulty/performance
  proof.

## Live evidence observer

- Added `devscripts/evidence-watch.py` as a read-only prototype.
- Verified live:
  - `list` found the active Juggernaut Horde server and a client, mapped PIDs to
    run manifests/journals, and reported two server processes without journals.
  - `show 33892` returned the exact run/build/journal/process identity.
  - `tail 33892 --event network.mode.start` returned the live mode transition,
    including `npc_count: 67` and `participants: 68`.
- It never adopts, stops, or edits a process.
- Known cleanup items are listed in the handoff: bounded index/cache, per-file
  tail offsets, external journal mapping, role verification, capture size cap,
  and optional dashboard/coordination integration.

## Specifications and handoff

Added:

- `docs/handoffs/2026-11-24-live-evidence-snapshot-time-handoff.md`
- `docs/specs/dev-loop/live-evidence-observer.md`
- `docs/specs/simulation/session-snapshot.md`
- `docs/specs/simulation/time-and-ordering.md`
- `docs/specs/simulation/quantum-inspired-and-literal-quantum-research.md`
- `docs/ROUTER.md` routes for the new documents
- `docs/operations/devscripts/script-inventory.md` entry for the observer

The specs record the approved decisions:

- session snapshot first;
- dev/test-only tick-rate and time-scale overrides, default 60 Hz;
- on-demand evidence observer with optional registry;
- 999 Hz as a benchmark;
- discrete/event-driven primitive model first;
- literal-quantum research kept separate.

## Evidence and limits

- No game DLL build or runtime behavior change was made.
- No session snapshot command exists yet.
- No time/tick override exists yet.
- The observer is a prototype; its `list` scan is not yet bounded and `tail`
  re-reads files on each poll.
- The gold note is human acceptance, not a controlled metric proof.
- The Juggernaut Horde live run was left running; the observer did not control
  it.

## Next steps

1. Finish the evidence observer cleanup and registry.
2. Implement the session snapshot first slice from
   `docs/specs/simulation/session-snapshot.md`.
3. Add the central simulation clock and tick-rate benchmark.
4. Then slow motion, event ordering, and the primitive-state toy world.
5. Keep NPC/header work in separate windows from time/snapshot builds.
