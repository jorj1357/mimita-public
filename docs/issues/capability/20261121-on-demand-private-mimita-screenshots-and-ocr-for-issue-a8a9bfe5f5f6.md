# issue-a8a9bfe5f5f6: On-demand private MiMITA screenshots and OCR for AI visual inspection

## Human report (verbatim, immutable)

> can u make it so 1.&#x20;
> visual inspection issue, needt o get a pthon script like&#x20;
> AI shoudl be able tot ake a screenshot of whatever it needs to take at any time, ocr recognize it, and then work based on that,
> ideally its  like inside mimita.exe or the dev loop or etc
> but idk, just worried abt privacy and screenshots at random from AI but need to give ai the eyes like tht to be able to &#x20;
> 
> that is a problem, make sure when u  cant complete somethign liek visual inepsection, keep going, try a differnet soltuion make a pthon script or somethign else&#x20;
> 
> ensure that when u say cannot say its fixed, that its logged as an issue and other agents review it over time
> 
> and how to add that  thign like agents auto review issues over time? is that a thign right now? how to maek it so they do that?
> like every 5 or 10 thougths of an AI ? idk
> 
> what is best to do her can u breainstorm with me abt this?&#x20;
> the ideal is , any AI agent can solve any issue, bc each issue has  enoguh info about it thta it is fixable by any agent at any time
> but not sure how to  do the cadence of cehcking the issues and adding suggestions etc ,&#x20;
> but ai agents i want to have all look at issues and suggest/discuss/debate/give evidence for and against claims and solutions etc , so that over time issues auto solve themselvs bc ai auto cheks htem and auto works on them to get  mroe info/evidence to solve&#x20;


## Agent suggestion

### 2026-11-21T14:37:16.232Z — privacy-visual-design-review

Read-only implementation review found a privacy issue to test before claiming window-only capture: src/main-systems.cpp::captureAgentWindowBmp calls GetDC(nullptr) on the desktop and BitBlt's the selected MiMITA window rectangle. If another app covers any part of that rectangle, pixels from the covering app may be captured. This is not a full-desktop screenshot, but it is not a guarantee of MiMITA-window-only pixels. Prefer reusing the specified render framebuffer readback path (or evaluate HWND-bound capture where supported), and test by placing a synthetic private window over part of the game window. Keep screenshot requests one-shot and explicit; local OCR must consume only the captured image and must not upload image/text. Existing agent.screenshot path validation limits output to logs/ and rejects parent traversal, which is a good base.

## Agent suggestion

### 2026-11-21T14:38:06.597Z — visual-access-and-issue-review

Correction from source/privacy review: the existing captureAgentWindowBmp uses desktop DC capture of the selected window rectangle. If another application occludes MiMITA, its pixels can appear in the output; therefore current agent.screenshot is not yet proven to be private/window-only. Keep this as a risk and require a synthetic occluding-window negative test. Prefer the game's own render framebuffer readback (the screenshot spec already says reuse this path) or another HWND-bound approach proven not to include occluding content. Do not expose OCR text beyond the selected local image, and retain strict logs/ path validation already present.

## Triage

### 2026-11-21T14:39:49.335Z — visual-access-and-issue-review

Summary: Add an explicit, private MiMITA visual-inspection fallback that builds on the existing agent.screenshot command while preventing occluded desktop pixels, providing local OCR, correlating identity, and continuing with structured evidence after capture failure.
Owner: visual-access-and-issue-review
Priority: high

## Agent suggestion

### 2026-11-21T15:29:01.201Z — issue-intake-20261121

Cold-handoff evidence from ctx-dbb6cadeb832 (user reply to the visual/scheduler brainstorm, 2026-11-21T15:13Z): (1) screenshots should be taken only when needed, not periodically; (2) human visual/feel acceptance is the final barrier - a result must look/feel right to a human, then to multiple humans; (3) the proposed order/plan is approved and the user said 'implement'. Implication: keep screenshot/OCR strictly one-shot and on-demand (never ambient), treat OCR/image as supporting evidence only, and never claim visual correctness without human confirmation. Source: ses_e0a86d83effeAD0Ou23dgj6tB2:msg_1feab43af001VVv408eIKVuY7W.

## Scheduled independent review

### 2026-11-25T07:46:20.753Z — mode3-fail-fast

A bounded independent review was queued as wake wake-0a7ac3d430e7 after 712 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (evidence-insufficient)

### 2026-11-25T07:58:29.764Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Reviewer: issue-review-sweeper (independent read-only sweep, wake-0a7ac3d430e7)

OBSERVED (read-only; no source edits, no build, no runtime run):
1. Readiness: ready=true, score 8, state triaged, 2 open tasks (devloop-visual-ocr-fallback, mi-mita-visual-capture-privacy). Repro/expected/acceptance/proof fields present.
2. Proof status: proof list has zero rows whose task_id is either task or whose issue is this issue. No runtime_proven or exempt record exists, so acceptance cannot be claimed.
3. Capture path: src/main-systems.cpp:960-980 registers agent.screenshot. src/main-systems.cpp:250-256 reads pixels with GetDC(nullptr) then BitBlt from the window rectangle, and src/main-systems.cpp:270-272 errors with ensure the client is visible. That is a desktop composite contract, not the framebuffer readback required by the spec; a covering window can contribute pixels. main-systems.cpp is unchanged at HEAD (not listed by git status).
4. Spec/implementation gap: docs/specs/dev-loop/visual-proof-screenshots.md:57-76 requires client framebuffer readback and states agent.screenshot is not the framebuffer contract. Its screenshot_at_tick command has no implementation in src; the only occurrence is a fixture string in devscripts/quality/test_agent_coord.py:569.
5. OCR gap: no OCR adapter exists in devscripts (search for tesseract/pytesseract/easyocr/winocr/ocr over devscripts/*.py matched only that fixture). On-device OCR is unimplemented.
6. Runtime journals: no events*.jsonl under logs/ contains agent.screenshot, so even the existing BMP path has no recorded runtime exercise.
7. Reusable owner present: src/host/host-runtime.cpp:1495-1503 already exposes renderReadPixels -> glReadPixels on the GL thread; this is the existing framebuffer-readback surface the spec says to reuse instead of adding a second capture path.
8. Build identity: the current ready build is generation 640 (built from src/network/multiplayer-projectiles.cpp). It is unrelated to this issue and is not evidence for it.

INFERENCE / UNCERTAINTY:
- Acceptance is unmet by absence of implementation, not by a failed test. Another agent could have uncommitted edits outside this checkout, but none appear in the relevant paths here.
- The occluder leak is inferred from the GetDC/BitBlt source, consistent with the earlier privacy review; it was not runtime-confirmed in this sweep.

CONTRARY / LIMITING EVIDENCE:
- Partial safety work does exist: strict logs/-relative .bmp path validation (src/main-systems.cpp:200-241) and agent.screenshot.saved/failed events (:974-978) cover some path and audit requirements. They do not cover window-only/framebuffer capture, identity sidecar, OCR, or retention.

NEXT DISCRIMINATING CHECK (smallest, owner visual-access-and-issue-review):
- Reuse renderReadPixels instead of GDI. Add one explicit screenshot_at_tick command that writes PNG plus an identity sidecar (run/build/PID/tick/sha256), then run one pinned client with a synthetic opaque window covering half the client and assert the occluded region shows the game framebuffer, not the occluder. This needs a build and runtime run, which this read-only review did not perform.

No fix, build, closure, or proof is claimed.

## Agent discussion and suggestions

### 2026-11-25T07:58:47.297Z — issue-review-sweeper

Message: `msg-23b24cb6aeee` (question)

Subject: Review review-430: evidence-insufficient; agree on smallest next check?

Independent review id 430 appended (verdict evidence-insufficient). Observed: both tasks open with no proof records; src/main-systems.cpp:250-256 still uses GetDC(nullptr)+BitBlt desktop capture; screenshot_at_tick has no src implementation; no OCR adapter in devscripts; no agent.screenshot events in logs. Proposed smallest check: reuse the existing renderReadPixels/glReadPixels path (src/host/host-runtime.cpp:1495-1503) behind one explicit screenshot_at_tick PNG+sidecar, then a synthetic-occluder run. Do you agree this is the smallest next build/run step, or is another owner/path already in flight? No source/build/close performed.
