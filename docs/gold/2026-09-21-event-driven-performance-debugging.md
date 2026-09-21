// 2026-09-21T02:54:48Z
/* purpose
* preserve the confirmed event-driven hot-reload and performance-debugging lesson
* explain how JSONL evidence connected a visible stutter to repeated frame-thread work
* show how a lightweight AI agent can make a meaningful, verifiable performance improvement
* record the path-identity lesson without treating a copy/paste mismatch as a code failure
* this file does NOT claim live-code editing has been fully accepted after the fix
* this file does NOT claim a controlled benchmark of AI models
*/

# Gold behavior: JSONL evidence led to an event-driven performance fix

- Timestamp: `2026-09-21T02:54:48Z`
- Scope: hot-reload polling, frame-time evidence, and bounded performance logging
- Human result: the observed frame-time stuttering was resolved; ordinary play now feels smooth
- Regression: `docs/regressions/2026-09-21/repeated-background-work-REG.md`
- Changelog: `docs/changelog/2026-09-20/20260920_224012-event-driven-hot-reload-and-log-bounding.md`

## What made the diagnosis possible

The important evidence was not the FPS counter by itself. The JSONL identified the work inside
the frame:

- `Setup::HotReloadDLL` was repeatedly expensive.
- `Setup::ConfigPolling` was repeatedly present.
- The session contained approximately 23,877 performance spikes.
- The worst observed frame was approximately 823.778 ms.
- The log grew to approximately 246 MB because repeated spike reports were written in full.

The path lesson mattered too. A pasted path appeared to contain `\\_`, but Windows stored the
directory as `20260921_011056`. Checking the exact path and then searching nearby paths found the
real client log. The screenshot's `versioninfo` output then proved which executable, process, and
JSONL file were actually active.

## The reasoning chain

1. The visible symptom was frame stutter.
2. The JSONL named the expensive scope instead of leaving the cause as a guess about VSync.
3. Source inspection showed that `engineTickSetup()` called `pollAndAdvance()` every frame.
4. The polling path periodically performed manifest work, cold-boundary checks, and source hashing
   even when no source had changed.
5. The repository already had a Windows filesystem watcher and a background hot-reload worker.
6. The smallest reusable fix was to let the watcher wake the worker and make the frame path consume
   state instead of doing the expensive detection work itself.
7. Repeated ordinary spikes were changed to use bounded logger aggregation, while severe spikes
   remained immediate errors.
8. A new timestamped executable was built without closing or replacing the existing process.

## Why this is reusable

This is the preferred pattern for future performance work:

```text
visible problem
-> authoritative JSONL path
-> exact process/generation identity
-> ranked timed owner
-> source-level call path
-> smallest owner-correct change
-> new build or hot activation
-> same measurement again
```

The lesson is not “turn off behavior until FPS rises.” The lesson is “remove repeated work that
does not change the result.” Gameplay, collision, animation, audio, and live editing remain
enabled; unchanged source simply stops causing expensive frame-thread checks.

## AI collaboration lesson

The human reported that a lightweight GPT-5.6 Luna-style agent was able to make meaningful progress
when it used the JSONL, inspected the real active path, traced the owner, and made a narrow change.
This is recorded as a human experience and workflow lesson, not a controlled model benchmark.
The capability came from the combination of:

- clear evidence;
- exact runtime identity;
- existing reusable architecture;
- a focused change;
- build proof; and
- human confirmation of the visible result.

That workflow gives the human more direct control because the distance between noticing a problem,
changing the owner, and observing the result becomes shorter and easier to verify.

## Remaining acceptance

The next test is live code editing during smooth gameplay. It must confirm that a real source edit
still produces one watcher event burst, one build, one generation, and one safe activation without
reintroducing frame stutter.
