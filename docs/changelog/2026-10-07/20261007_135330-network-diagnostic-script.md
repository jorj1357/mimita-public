# Network diagnostic script

- Timestamp: `2026-10-07T13:53:30Z` (display timezone: America/New_York)
- Branch/commit: current checkout; `00dc25ebe0f4da8d338440024c3a542369c98905`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Change

Added:

- `tools/test-mimita-network.bat`
- `tools/test-mimita-network.ps1`

The batch file launches the PowerShell diagnostic from the repository root.
The diagnostic checks configured DNS servers, A-record resolution for
`mimita.fun` and the historical `turn.stun.mimita.fun` name, compares the
local resolver with Cloudflare/Google public resolvers, HTTPS website
reachability, coordinator API reachability, non-mutating account lookup
reachability, ICMP ping, TCP 443, TCP 3478, and a real UDP STUN binding
request to `107.191.48.226:3478`. It never sends a password or performs a
real sign-in.

## Validation

- PowerShell parser: `PASS`.
- Real local run: website HTTP 200; coordinator HTTP 200; account lookup HTTP
  200; TCP 443 `PASS`; TCP 3478 `PASS`; UDP STUN reply `PASS`.
- The same run showed `turn.stun.mimita.fun` does not exist in DNS. This is
  evidence about the current resolver and hostname, not proof that the current
  game executable uses that hostname.

## Review and limits

No existing source changes were modified. The script is not a substitute for
testing on the affected user's network. A failed ping alone is inconclusive;
DNS, HTTPS, and UDP STUN results are the useful evidence. TURN allocation and
two-client ICE joining still require a real credentialed runtime test.

Documents/skills applied: `docs/ROUTER.md`,
`docs/workflows/runtime-scenario-validation.md`,
`docs/skills/spec-behavior-review-v1.md`,
`docs/skills/logging-checker-v1.md`,
`docs/operations/task-completion/task-completion.md`, and
`docs/architecture/time-and-formatting/time-and-formatting.md`.
