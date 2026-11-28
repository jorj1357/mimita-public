# issue-a1d9a695459d: Diagnose Payload join crash on Mimita Pipes with unmistakable false-condition reasons

## Human report (verbatim, immutable)

> can u invstigate why crash when joining paload server on mimita pipes map and make it so its super loud obvious why false suggest how and add issue for that


## Agent suggestion

### 2026-11-28T06:30:30.344Z — rocket-league-aerial

Duplicate of canonical issue-c9b65fb9948e (same user report ctx-9543f5ce1f70, same reporter, same title, created 16s earlier). Root cause is now recorded there: Player::requestModelLoad detached-thread use-after-free (player-loader.cpp:1969-1975) proven from run 20261128_054617_01 crash dumps. Please defer/close this duplicate and track work on issue-c9b65fb9948e.

## Agent suggestion

### 2026-11-28T06:31:36.248Z — rocket-league-arena2

Duplicate of canonical issue-c9b65fb9948e for the same user context ctx-9543f5ce1f70. Root cause is resolved (player-loader async model-load use-after-free; the gen-808 detached job read freed d->resolvedPath in tinygltf, ACCESS_VIOLATION read at 0xa). The fix and loud invalid-path diagnostics are already in the working tree with pending build note 9dd132be9fa2 and request build-request-59479dc50ebe. Please consolidate on issue-c9b65fb9948e (owner rocket-league-aerial) and do not create a third issue.

## Deferred

### 2026-11-28T06:35:54.937Z — rocket-league-crashfix

Reason: Duplicate of issue-c9b65fb9948e (same user quote/context ctx-9543f5ce1f70, created 16s earlier by the same intake). Consolidate all reproduction, symbolized stack, and fix evidence under issue-c9b65fb9948e; no separate work here.
Until: (not set)
