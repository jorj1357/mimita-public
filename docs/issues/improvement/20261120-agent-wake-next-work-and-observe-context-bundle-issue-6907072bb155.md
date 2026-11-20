# issue-6907072bb155: Agent wake, next-work, and observe context bundle

## Human report (verbatim, immutable)

> 


## Task linked

### 2026-11-20T03:58:44.344Z — melee-networking

Task: wake-runner (step 0)

## Agent suggestion

### 2026-11-20T14:43:17.731Z — launch-doctor

Bounded agent-wake test (launch-doctor, wake-688015d2ca4c) evidence: 9 launches succeeded including this session (begin/sync/observe/context ack all worked). Dominant failure across 28 requests in the 14:28-14:32 burst is the already-tracked expired OpenCode API key (401 api_key_expired in .dev/agent-wake/logs/2026-11-20T14-28-*.log, surfaced as 'adapter process exited') = issue-36579a10979a. Distinct second defect: request-store write race when ~40 wakes are enqueued at once; wake-0a87545e8551 failed with [WinError 5] renaming wake-0a87545e8551.json.tmp -> wake-0a87545e8551.json. Suggest retrying os.replace on transient Windows sharing violations and/or serializing request writes. Confidence: high for key-expiry attribution, medium for race reproduction.

## Agent suggestion

### 2026-11-20T15:55:23.623Z — wake-selftest

Bounded wake test found: 'end' and 'activity set' resolve identity only from --agent/MIMITA_AGENT_ID and ignore the work session created by 'begin --agent X'. In the test shell MIMITA_AGENT_ID=melee-parity while begin used --agent wake-selftest, so end deactivated melee-parity instead of wake-selftest. Suggest end/activity default to the begin-created session and warn on identity mismatch.
