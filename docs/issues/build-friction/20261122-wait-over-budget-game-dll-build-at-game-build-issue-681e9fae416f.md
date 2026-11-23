# issue-681e9fae416f: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (pass-with-caveats)

### 2026-11-22T22:53:52.421Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait game-dll-build:2026-11-22T22:24:13.556Z (issue-681e9fae416f). Verdict: pass-with-caveats - the wait was genuine and the one-issue/one-wake hook fired exactly once, but the recorded reason and build identity are invalid (both already tracked; no new duplicate filed).

Direct observations (logs/2026-11-22/20261122_220630/dev-loop/events.jsonl):
- Active attempt game-dll-20261122T222413577092Z-1031f3, source_generation 13, revision 777456d6a47b9c27d64aeda6a860864573a03d058b192fa5b6db5d455f8a2b28, request auto-13. Changed path src/npc/npc-avatar.cpp (notes d76abfdd3f13 and b08eeca4579b by codex-networking-stall-fix).
- Started 22:24:17.863Z; src/npc/npc-avatar.cpp compiled at 22:25:14 after 54.714s (changed_source_preflight 54.785s). Attempt completed 22:25:57.140Z, success true, total 103.54s (link 38.445s). The 60.484s budget tripped during that single-TU compile, about 43s before completion. This is normal incremental cost, not a stall.
- Exactly one issue for this wait_id and exactly one reviewer wake: journal line 79 dev-loop.wait.reviewer-wake queued=true; line 80 dev-loop.wait.over_budget created=true. .dev/agent-wake/events.jsonl has one wake.queued for wake-c3d2731ff7b5 at 22:25:15.086Z; codex launched 22:25:23.121Z, fell back to opencode, which claimed/launched 22:43:24.9Z (delivery latency, not a duplicate).

Acceptance review (build_friction.py:187-190):
- wait_id, stage, elapsed, evidence present; reviewer wake exactly once: PASS.
- reason: INVALID. Stored reason is a JSON tail fragment (for example ...tree SSA incremental...progress_write_ms:2.354...), not human text. Cause: dev-loop.py:2832-2867 sets game_build_detail = line.rstrip()[-240:] for every worker line, so the long GAME_FILE compiler_timing payload tail becomes the wait reason at dev-loop.py:3539.
- build identity: WRONG. Evidence records build_id=2026-11-22T21:32:48.195707Z and generation=536, the previously published build; the in-flight attempt was generation 537 / game-dll-20261122T222413577092Z-1031f3. Cause: dev-loop.py:3543-3544 uses self.latest_build and self.game_build_generation, which are only updated on completion.

No source edited, no lease taken. The flat 60s WAIT_ISSUE_SECONDS policy plus the reason and identity defects are already recorded on umbrella issue-136d4553ff92 (and the budget policy on issue-9a4503f6ed68), so this occurrence is logged as another data point rather than a duplicate issue. This healthy 103.54s build still got mislabeled, which reinforces the stage-aware budget and identity/reason fixes.
Confidence: high on timings, attempt identity, and the one-issue/one-wake counts.
