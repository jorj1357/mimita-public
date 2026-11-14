# MiMITA escrow tool

Standalone escrow ledger for tooling and terminal use. It is intentionally
outside `src/` so the game build does not compile it. Money is stored as integer
cents; state lives in a JSON store and every state change is appended to a
JSON-lines event log.

## Build

From this directory:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Or with the wrapper:

```powershell
make
make test
```

The only dependency is the repository's vendored `include/nlohmann/json.hpp`.

## Run

Copy the example config and adjust paths:

```powershell
Copy-Item config/escrow.example.json escrow.json
.\build\escrow.exe --config escrow.json init
```

Relative `store_path` and `event_log_path` are resolved from the config file's
directory.

### Commands

```text
escrow --config escrow.json deposit --account alice --amount 100
escrow --config escrow.json create --buyer alice --seller bob --arbiter carol `
  --amount 25.00 --deadline 2026-12-31T00:00:00Z --fee-bps 250 --memo "art trade"
escrow --config escrow.json fund --id 1 --by alice --op-id deposit-1
escrow --config escrow.json release --id 1 --by carol --op-id release-1
escrow --config escrow.json refund --id 1 --by carol
escrow --config escrow.json expire --id 1
escrow --config escrow.json expire-all
escrow --config escrow.json status --id 1 --json
escrow --config escrow.json list --state funded
escrow --config escrow.json balance --account alice
escrow --config escrow.json events --id 1
escrow --config escrow.json pause --by admin
escrow --config escrow.json resume --by admin
```

## Rules

- `create` records a deal without moving money.
- `fund` is buyer-only, requires a positive balance, and rejects a passed
  deadline.
- `release` is arbiter-or-admin-only. The seller receives `amount - fee`; the
  fee account receives `floor(amount * fee_bps / 10000)`.
- `refund` is arbiter-or-admin-only and returns the full amount to the buyer.
- `expire` returns a funded deal to the buyer when `now >= deadline` and the
  deal has a deadline.
- `pause` blocks funding, release, refund, and expiry until `resume`. Only the
  configured admin may pause or resume.
- `--op-id` makes `fund`, `release`, `refund`, and `expire` idempotent. Replaying
  the same action/deal/op-id is a no-op success.

## Storage

`store_path` is a versioned JSON document:

```json
{
  "version": 1,
  "paused": false,
  "next_deal_id": 2,
  "next_event_id": 3,
  "accounts": { "alice": 7500, "bob": 2438, "fees": 62 },
  "deals": { "1": { "state": "released", "...": "..." } },
  "applied_ops": {}
}
```

Writes are atomic: a temporary file is written, then renamed over the store.
`event_log_path` is append-only JSON-lines and is never rewritten by the tool.

The tool does not connect to a payment processor or chain. `deposit` is an
explicit local ledger credit for testing and trusted tooling; production
deployments should replace it with a verified external funding source.

## Tests

`tests/escrow_tests.cpp` covers money/date parsing, happy-path fees, refunds,
expiry, authorization, idempotency, pause/resume, insufficient balance, and
store reload. Run with `make test` or `ctest --test-dir build --output-on-failure`.
