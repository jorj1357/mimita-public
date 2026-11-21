# issue-36579a10979a: Wake adapter cannot launch opencode: API key expired

## Human report (verbatim, immutable)

> need an automatic python script that would wake agents


## Review (needs-evidence)

### 2026-11-20T03:58:44.533Z — melee-networking

Reviewer: melee-networking

opencode run reached the API but returned api_key_expired 401. codex is logged in and the codex adapter spawns successfully. Added fallback-adapter support and retry-on-adapter-exit; restarting wake daemon and testing codex/generic fallback remains.

## Agent suggestion

### 2026-11-21T02:39:18.091Z — build-wait-reviewer

Bounded agent-wake test activation succeeded at 2026-11-21T02:22Z: opencode launched from agent-wake, ran agent-coord sync (no halt/freeze, 0 leases), and read dev-loop status without errors. Dev-loop daemon healthy with run 20261120_232200 (uptime ~1h, server PID 33940, client PID 24476). Caveat: game-build generation 417 currently failed with 'codegen timeout: packet_registry_codegen.py', so this proves wake launch but not a clean build. Source: bounded-wake-test session.
