# issue-36579a10979a: Wake adapter cannot launch opencode: API key expired

## Human report (verbatim, immutable)

> need an automatic python script that would wake agents


## Review (needs-evidence)

### 2026-11-20T03:58:44.533Z — melee-networking

Reviewer: melee-networking

opencode run reached the API but returned api_key_expired 401. codex is logged in and the codex adapter spawns successfully. Added fallback-adapter support and retry-on-adapter-exit; restarting wake daemon and testing codex/generic fallback remains.
