# Live-editing collisions: the day "core" turned out to be editable

Date recorded: 2026-09-18
Status: GOLD REFERENCE / DEVELOPER-OBSERVED MILESTONE
Scope: fixing a core system live in a running match with the JSONL debug stream
as the feedback channel, and why that combination matters for the project vision

## The observation

During a live session the developer watched collisions get fixed while the game
kept running. Not tuned. Fixed. A system that had been assumed to be cold,
fragile, and off-limits turned out to be editable in the same running process,
and the fix landed in live gameplay.

The reaction is the point of this document:

> That spark of happiness is huge, huge, huge. We should pursue that much more.

Two things combined to make that moment possible:

1. **Live editing of something that was believed to be core and cold.**
   Collision is the kind of system most engines bury in the executable and treat
   as untouchable. Here it was moved behind the hot boundary, so the algorithm,
   the constants, the broadphase, and the response policy could all be edited
   and activated without restarting, without reconnecting, and without losing
   the world.

2. **The JSONL debug stream as the AI's direct feedback signal.**
   One `events.jsonl` per run, instead of a scatter of per-category `.txt` files
   that were spammed, scattered, and effectively unreadable to an AI. The AI can
   `rg` the live file while the game is running, see the exact values the code
   produced, change the code, and check the new values live until they hit the
   exact numbers wanted.

## Why each half matters

### Live editing of "core" code

The mental model that broke was:

```text
collision, physics response, movement authority  ->  cold, complex, do not touch
```

The mental model that replaced it was:

```text
one concept, one owner, behind the hot boundary  ->  edit it, save it, watch it
                                                        change in the running match
```

This is more than convenience. It changes what a person or an AI is willing to
attempt. If a system feels cold, you avoid it and work around it. If it is
provably editable live, you fix the actual owner instead of stacking a
workaround on top of it. That is the difference between "close, rebuild,
relaunch, reconnect, hope" and "change it now and see".

The exact thing the developer found surprising is the exact thing worth
generalizing: any behavior that is a *policy* rather than a *process/authority
fact* should be reachable and editable while the process lives.

### The JSONL stream as AI-visible results

The old debugging surface was many `.txt` files, one per category, written and
spammed until no human or AI could realistically read them. The new surface is
one append-only JSONL file per process run:

```text
logs/yyyy-mm-dd/hhmmss/events.jsonl
```

One line, one independently valid event, flushed immediately, openable in
VSCode, searchable with `rg`, streamable with `Get-Content -Wait`, and readable
by an AI while the game is still running.

That is the second half of the loop, and it is what makes the first half useful.
Editing live without clean feedback is guessing. Editing live with a stream the
AI can read is a closed loop:

```text
AI asks or the human reports a problem
-> running game writes the relevant evidence to events.jsonl
-> AI reads the live file (rg / -Wait)
-> AI edits the hot code and saves
-> candidate validates and activates at the next safe tick
-> events.jsonl shows the new values
-> repeat until the numbers are exactly right
```

The AI can tune to an exact value instead of guessing once per build.

### Honest note on maturity

The JSONL logger is not claimed to be perfect yet. The developer's own framing
is fair: "I don't know if it works super well." What is claimed, and what is
already true, is that it is **better** than a billion scattered `.txt` files
that no AI can read. The win is directional and real: fewer files, one format,
readable live, machine-parseable. It improves from here.

## Why this is the roblox-like vision

The long-term vision is often described as "roblox, but you can edit the C++
directly." This milestone is a concrete proof of the second half of that
sentence:

- Roblox gives you a running world you can edit, but you edit inside a fixed
  engine's scripting surface.
- MiMITA is aiming at a running world you can edit at the level of the actual
  gameplay code, so the person editing has far more powerful control over the
  whole system.

Collision was chosen by intuition as "the last thing you'd be able to edit." It
was edited live. That moves the ceiling: if collision can be hot, far more can be
hot, and the roblox-like "edit while it runs" experience gains the depth of
native code.

## The reusable lessons

1. **"Core" is often a belief, not a fact.** Audit the assumption. Most of what
   feels cold is a policy that was never moved behind the boundary.
2. **Editing needs feedback to be worth anything.** A live-editable system
   without a live-readable evidence stream just moves the guessing.
3. **One readable stream beats many scattered files.** The format matters less
   than being single, append-only, flushed, and greppable by the AI that is
   actually doing the work.
4. **Hit exact numbers, not vibes.** Because the AI can see the values it just
   produced, it can iterate to the precise target in one session.
5. **Keep the process alive.** The whole value collapses if the fix requires a
   restart, a reconnect, or a lost world. The invariant still holds:
   if MiMITA.exe is running, it stays running for the session.

## Direction to continue

- Move more genuinely-core behavior behind the hot boundary, one owner at a time,
  starting with the systems next most often assumed to be untouchable.
- Convert important subsystems from ad-hoc logs to structured `debug::logEvent`
  records so the AI's feedback signal gets richer, not noisier.
- Keep `events.jsonl` the single authority; summaries and derived views are fine,
  duplicate `.txt` authorities are not.
- Preserve the general principle from the live-runtime direction: prefer a
  generic runtime mechanism over a new cold seam. Collision proved the payoff.

## Related authoritative files

- `docs/gold/2026-09-12-live-code-hot-reload-journey.md`
- `docs/gold/2026-09-12-live-jsonl-ai-observability.md`
- `docs/architecture/live-development/hot-kernel.md`
- `docs/architecture/live-development/live-development.md`
- `docs/features/live-code-development/live-code-development.md`
- `src/hot-reload/packages/collision/`
- `src/debug/structured-log.*`
- `docs/changelog/2026-09-17/20260917_160000-collision-host-contract-and-touch-logging.md`
- `docs/changelog/2026-09-17/20260917_140000-live-events-jsonl.md`
