# Melee Hit Feedback and Hitbox Accuracy

Time created: 2026-10-18T17:42:50Z
Time last updated: 2026-10-18T17:42:50Z

Status: ATTEMPTED FIX (1)

Related specifications:

- `docs/specs/gamemodes/supersmashbrosmelee.md`, sections 58–59

Related changelog:
`docs/changelog/2026-10-18/20261018_174250-melee-hit-feedback-aim.md`

## Regression occurrence 1

### Expected behavior

A server-confirmed Melee contact produces one restrained impact sphere,
directional burst, and spatial hit sound for the attacker, target, and observers.
The effect is presentation only; Melee percent and knockback remain the gameplay
result. The configured per-move sphere size and tick window match the tested
server geometry.

### Actual behavior

The authoritative hit functions updated percent/impulse and emitted `melee.hit`
to the server journal, but did not queue a network impact event. Clients
therefore had no confirmed hit position/direction to send through the existing
entity-impact and audio presentation path.

The JSON already supported per-action and per-active-tick hitbox size, limb,
offset, and active window, and `weapons.json` was polled for live reload. The
server nevertheless enforced a 0.2 m minimum radius while local visualization
used 0.01 m. Also, the local visual follows the sampled animated limb transform,
while server contact currently estimates limb centers from actor-root offsets.
Those transforms can differ, so size tuning alone cannot guarantee that a
visible overlap is an authoritative contact.

### Fix and remaining work

Confirmed contacts now queue a reliable impact presentation event and reuse the
existing hit sphere, directional burst, and spatial hit sound. The event cannot
apply health damage or a second impulse. Sound presentation uses global
`weapon_hitfx.json` settings and applies a modest volume scale. Server
radius floor now matches the local 0.01 m display floor. Melee mode sets its
aim-FOV input to `none`; RMB remains the configured secondary attack. The
existing exclusive authored-pose path already suppresses physical arm aiming.

The server-versus-rendered limb-center mismatch remains open and is the next
known accuracy issue. Do not claim full visible-hitbox/contact parity from the
build alone.

### Evidence and limits

Debug build 2597 compiled four touched translation units; build 2598 compiled
one follow-up replay-path change; build 2599 compiled one follow-up audio-config
change. Build 2599 passed `--versioninfo` and identified
`.dev/builds/2599/mimita.exe`; its journal path is
`logs/10-18-2026/20261018_134239/events.jsonl`.

The current dev-loop client remains on build 2594 and its managed server is
reported dead. No live Melee contact was run on build 2599, so visible effects,
sound, hot tuning, aim behavior, and multiplayer acceptance remain unverified.
