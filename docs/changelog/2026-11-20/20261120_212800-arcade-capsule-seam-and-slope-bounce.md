# Arcade capsule seam response and slope bounce

## Task

- Work session: `work-e4c40fa6473b`
- Session ID: work-e4c40fa6473b
- Agent: `arcade-collision-elite-proof`
- Issue: `issue-da337d1fe2f3`
- User report: Arcade collision catches on surfaces, does not reliably bounce or slide, and must keep the actor out of world geometry using capsule-vs-world and capsule-vs-capsule collision.

## Findings

- Arcade intentionally bypasses the actor-triangle solver and body/weapon world pass. Its static-world movement path is the root capsule; actor pairs use the shared capsule-pair solver.
- The Arcade capsule TOI path did not use `smoothSeamNormalAt`, although the shared triangle collision path does. A raw triangle edge normal could therefore be used for the Arcade sweep's slide and bounce response at a joined seam.
- The Arcade sweep classified every walkable surface (`normal.z >= MAX_WALKABLE_SLOPE_DOT`, 0.80) as grounded support and projected velocity before bounce response. The shared response and movement specification reserve project-only response for an already-supporting near-flat floor; walkable slopes must bounce. This directly explains missing bounce on grounded ramps.
- The same broad support classification also omitted the outward rebound component from remaining same-tick displacement on slopes.
- Regression target is the reported Dust2CyberiaV4 ramp point `(-685.651, -22.853, 2351.122)` and human recording `scenario-20261031_034240-dustcoll.jsonl`.

## Changes

- In `src/physics/movement/physics-collision-glb-sweep-slide.cpp`, route Arcade TOI contacts through the existing seam-normal cache, orienting the smoothed normal to the incoming side before using it for slide and bounce.
- Restrict project-only support handling in both TOI response and residual movement to near-flat contacts (`normal.z > 0.995`). Walkable slopes now use the configured bounce response; flat supporting floors remain project-only.
- The movement spec says: “A flat floor that is already supporting the actor is treated as support, not a new impact” and “Slopes and walls use the configured bounce normal.” This patch aligns Arcade with that behavior (`docs/specs/movement/movement.md`, “Consistent bounce on physical impacts”).

## Build and runtime evidence

- Root dev-loop candidate DLL generation 415, sourcegen 41, attempt `game-dll-20261120T205850979297Z-7cdcb2`, compiled and published successfully. It contains the Arcade seam-normal change.
- Root dev-loop candidate DLL generation 416, sourcegen 42, attempt `game-dll-20261120T211735017299Z-db53e8`, revision `4d4c42863bdf0f0388a13bf2fa92944af3bee127d115e1ca043355125223b0fc`, compiled and published successfully. DLL SHA-256: `8B470E4249101BA96D6BC32A4A3B1133E09366BC6C476B4BF9D504F95394A17D`.
- Build notes attached to generation 415: `cf40861d09ab` (seam response). Notes attached to generation 416: `eb158c7548b8` (seam API) and `b3e4fea8256f` (slope bounce policy).
- No gameplay result is claimed. Existing pinned run `20261120_202228` used EXE sourcegen 38 and DLL generation 412, then failed before `logger.started`; its server `events.jsonl` is empty. The current EXE build request reused the same sourcegen-38 artifact, and the shared runtime has no client, so repeating the collision run would not provide new evidence.
- Dust2CyberiaV4 ramp, seam sliding, bounce, actor-pair contact, penetration, and human visual feel remain unverified in a healthy pinned runtime.

## Validation

- The changed translation unit compiled in both candidate attempts; generation 416 linked and published.
- `git diff --check` still reports an existing trailing-space line in the same file (the dated collision comment at line 428); it was not changed as part of this work.

## State

- Issue remains open and runtime proof remains outstanding.
- Branch: `checkpoint/one-checkout-condensation-20261111`
- HEAD before changelog: `6d92afa35b434790e1b42ebb9667be69a216cde2`
