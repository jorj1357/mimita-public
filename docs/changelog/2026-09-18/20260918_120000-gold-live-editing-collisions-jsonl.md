# Gold record: live-editing collisions and JSONL observability

- EST timestamp: 2026-09-18 12:00:00 -04:00
- Branch: `8292026stash`
- Result: `PASS`

## Goal

Record, as a gold reference, the developer-observed milestone that collisions
were fixed live in a running match, and that the new single `events.jsonl`
debug stream is what let an AI see the direct results of its own code and iterate
to exact values live. Capture the "this is huge, pursue it much more" direction
and the roblox-like vision it proves out.

## Changes

- `docs/gold/2026-09-18-live-editing-collisions-and-jsonl-observability.md` (new)
  - States the observation in the developer's own framing: a system believed to
    be cold, core, and complex (collision) was edited and fixed live, and that
    spark of happiness is huge and should be pursued much more.
  - Explains why the two halves matter together: live editing of "core" code,
    plus one readable JSONL stream instead of many spammed `.txt` files an AI
    cannot read.
  - Honest note that the JSONL logger is not claimed to be perfect yet, only
    better and directional.
  - Connects it to the vision: like roblox for editing a running world, but with
    direct C++ control over the whole system, so the developer/AI has far more
    power.
  - Reusable lessons (core is often a belief; editing needs feedback; one stream
    beats many files; hit exact numbers; keep the process alive) and a
    direction-to-continue list.
- `docs/gold/2026-09-12-live-jsonl-ai-observability.md`
  - Added the new gold doc to its related-files list.
- `docs/gold/2026-09-12-live-code-hot-reload-journey.md`
  - Added the new gold doc to its related list.

## Evidence

- Documentation-only change; no code, build, or runtime behavior changed.
- `git diff --check` passed (exit 0).

## Human review

- The observation itself is developer-reported and is recorded as the human
  acceptance of the milestone. No additional runtime evidence is claimed here.
