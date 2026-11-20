# Visual evidence correlation proposal

## Change

Appended an issue proposal to `issue-cf9cd77eb056` asking that AI-facing visual proof capture screenshots at decisive lifecycle and Payload checkpoints, correlate each frame to run/instance/build/PID/time and nearest journal tick, pair it with connection or cart/NPC support telemetry, and report missing evidence explicitly.

## Evidence and limits

- The root-owned DLL attempt `game-dll-20261120T182726149737Z-df8100` is live at source generation 29 and had completed 558/642 translation units at the latest inspection. Current source generation is 31; two automatic requests are queued. No artifact with the reliable-event diagnostic has published yet.
- Dev-loop lists older TDM test runs `20261120_162116` and `20261120_152743` as online. They were not changed or stopped.
- No new build, test run, runtime behavior, or visual acceptance is claimed in this activation.

## Next step

Wait for root dev-loop publication of the current source, then start an isolated TDM run pinned to that artifact and capture lifecycle screenshots aligned with the client/server journals. Continue Payload NPC/cart visual and telemetry work under `issue-cf9cd77eb056`.
