# issue-130ee745052a: agent.screenshot captures desktop content instead of the hidden client framebuffer

## Human report (verbatim, immutable)

> agent screen shot u can put like a issue that  we need to make that work headlessly like so agents can capture screnshot even without  the window being visible that is a issue

## Summary

The existing `agent.screenshot` path captures the desktop rectangle at the host
window position with GDI `BitBlt`. It does not read the client framebuffer, so a
hidden, covered, minimized, unfocused, or headless game window can produce an
unrelated desktop image while the command still reports success. This breaks
visual proof integrity and can capture unrelated applications.

## Reproduction

Launch a pinned Bloodtest run with the game window hidden or covered by another
window, then run `agent.screenshot logs/<date>/<run>/client-01/shot.bmp`.

In run `20261124_041854`, the command returned `terminal.command.completed` and
wrote a 1920x1080 BMP. The image was a Rocket League replay window, not MiMITA.
`src/main-systems.cpp:215-290` uses `GetDC(nullptr)` and
`BitBlt(windowX, windowY, width, height)` instead of a client framebuffer
readback, and only fails when `BitBlt` itself fails.

## Expected behavior

Capture the MiMITA client framebuffer after the UI pass even when the game window
is hidden, minimized, covered, unfocused, or headless. If framebuffer capture is
unavailable, return `result=failed` or `result=missed` with a reason and do not
write unrelated desktop pixels.

## Acceptance

A pinned hidden-window or headless run produces an identity-matched PNG plus
sidecar with run/build/tick, `headless=true`,
`capture_backend=framebuffer`, and window visibility/focus state. The image
content matches MiMITA when another window covers the game. A desktop/window
fallback must return failed rather than an unrelated image. Repeating the test
with the game visible and hidden must produce the same scene content apart from
normal frame variation.

## Paths

- `src/main-systems.cpp`
- `docs/specs/dev-loop/visual-proof-screenshots.md`
- `docs/specs/dev-loop/agent-proof-and-verification.md`

## Evidence

- Run: `20261124_041854`
- Build: `2026-11-24T03:30:52.372066Z:3ae36c9314a776a66c842b97356f14e128171e1442d28e49cd18a110b6030db6`
- Game generation: 581
- Command: `agent.screenshot logs/2026-11-24/20261124_041854/client-01/bloodtest-crisp.bmp`
- Result: delivered and completed, 1920x1080 BMP
- Observed content: unrelated Rocket League window, not MiMITA
- Artifact was deleted after inspection; the observation and journal remain.
- Journal: `logs/2026-11-24/20261124_041854/client-01/events.jsonl`
- Source: `src/main-systems.cpp:215-290`

## Risk

Agents can mistake another application for game evidence and claim visual
acceptance. The path can also capture unrelated desktop content, creating a
privacy leak. Keep desktop/window capture out of the proof path and require
framebuffer readback plus sidecar identity.

## Proof expectation

Pinned hidden-window/headless runtime: journal records the screenshot command
result, a PNG and sidecar exist with run/build/tick/capture-backend/headless
fields, the image matches MiMITA while covered, and the failure path writes no
misleading BMP. Do not close on code inspection or a build-only result.

## Related

- `issue-89efbbc86535` (screenshot-at-tick)
- `issue-dad84e4e7c50` (event-first proof and visual residuals)


## Triage

### 2026-11-24T05:28:02.461Z — opencode-deepseek-v4-1

Summary: (unchanged)
Owner: (unchanged)
Priority: high

## Agent suggestion

### 2026-11-24T05:30:33.827Z — hot-reload-latency-root

Routing review from wake hot-reload-latency-root (context ctx-83fb52c3e291, acknowledged 2026-11-24T05:29Z). The context is correctly routed: issue is complete (summary, repro with run 20261124_041854, expected behavior, acceptance, paths, risk, proof expectation), triaged high, task 'headless-framebuffer-screenshot' owned by opencode-deepseek-v4-1, dependency issue-89efbbc86535 recorded. One blockage to flag: acceptance requires PNG+sidecar at a tick, but that is exactly the unimplemented screenshot_at_tick contract in issue-89efbbc86535, which is high priority, unassigned, readiness=false (missing proof_expectation, risk) with an empty-acceptance task. Related capability issue-a8a9bfe5f5f6 (private MiMITA screenshots + OCR) already names the same GetDC(nullptr)+BitBlt privacy defect. Recommend a single framebuffer-readback owner covering all three rather than three parallel capture paths, and that issue-89efbbc86535 be completed or explicitly superseded before 130ee745052a can be proven. No edit, build, or claim performed; left unowned for claim.

## Agent suggestion

### 2026-11-24T05:30:53.423Z — build-wait-reviewer

Source review confirms the root-cause claim: src/main-systems.cpp:246 GetDC(nullptr) + src/main-systems.cpp:251 BitBlt(..., windowX, windowY, SRCCOPY|CAPTUREBLT) reads the desktop rectangle, and the command help/event text even says 'visible client window'. Reuse the existing framebuffer-readback owner rather than adding a new path: engine-tick.cpp:468-507 probeFramebuffer() already does r::bindFramebuffer(Read,0) + r::readPixels + stbi_write_png after the UI pass and journals viewport/center/non-black stats. The headless screenshot should extend that generic readback (PNG + sidecar with headless/capture_backend/window state/hash) and must drop the GDI desktop fallback so a hidden/minimized window fails rather than writing unrelated pixels. Note minimized windows may report zero size via hostWindowSize (src/main-systems.cpp:213-219), so capture must not depend on the host window rectangle at all. Suggest the task owner confirm this reuse path before implementing.

## Agent discussion and suggestions

### 2026-11-24T05:31:24.662Z — build-wait-reviewer

Message: `msg-9d472fadab75` (review)

Subject: Headless screenshot: reuse existing framebuffer readback

Reviewed issue-130ee745052a. Root cause confirmed: src/main-systems.cpp:246/251 GDI GetDC(nullptr)+BitBlt reads the desktop rectangle. Reuse engine-tick.cpp:468-507 probeFramebuffer (r::bindFramebuffer(Read,0)+r::readPixels+stbi_write_png) as the generic framebuffer owner instead of adding a new capture path, and drop the GDI fallback so hidden/minimized fails explicitly. Note hostWindowSize may be zero when minimized (src/main-systems.cpp:213-219), so capture must not depend on the window rectangle. Please confirm this reuse path before implementing, and reply with the agreed owner/next check.
