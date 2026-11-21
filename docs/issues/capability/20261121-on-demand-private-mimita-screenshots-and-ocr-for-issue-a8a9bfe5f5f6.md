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
