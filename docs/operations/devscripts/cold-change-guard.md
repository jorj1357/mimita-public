# Cold-change guard

`devscripts/cold_change_guard.py` is a preflight ownership check for edits that
would otherwise require an EXE build or process restart.

## What it classifies

The guard reads `build/source-manifest.json` and classifies changed files:

- `HOT`: `game`-owned C++ source. Normal game-DLL replacement path.
- `COLD`: `resident`, `bootstrap`, or `launcher` source. Requires an EXE
  rebuild/restart.
- `MODULE_DLL`: `module:*` source. Separate module DLL; warn while modules are
  being folded.
- `COLD_ABI`: a changed `src/host/abi/**` header. Requires an EXE because the
  host/game boundary changed.
- `HEADER_REVIEW`: another header under `src/`; its dependents must be checked.
- `TOOLING`: build/gate/ownership tooling. Does not by itself require an EXE,
  but the dev-loop/daemon may need restarting to load new Python code.
- `CONFIG`: configuration/data; hot reload depends on the owning system.
- `NON_PRODUCT`: test/vendor content.
- `UNKNOWN`: no owner was found. Treat as blocking until classified.

## Usage

Inspect current changed files without failing:

```text
python devscripts/cold_change_guard.py --report-only
```

Check an explicit edit set:

```text
python devscripts/cold_change_guard.py src/combat/melee-client.cpp
```

Fail on any cold, ABI, header-review, or unknown change:

```text
python devscripts/cold_change_guard.py
```

Write a machine-readable report:

```text
python devscripts/cold_change_guard.py --report-only --json build/hot-reload/cold-change-guard.json
```

Exit status:

- `0`: only hot/config/non-product/tooling changes, or `--report-only`.
- `1`: at least one `COLD`, `COLD_ABI`, `HEADER_REVIEW`, or `UNKNOWN` change.
- `2`: the guard could not read the source manifest or git state.

## Boundary examples

| Change | Expected classification |
|---|---|
| `src/combat/melee-client.cpp` | HOT |
| `src/network/server-packet-handlers.cpp` | HOT |
| `src/perf/perf-frame.cpp` | HOT |
| `src/network/server.cpp` | COLD; resident server pump |
| `src/network/ice/ice-agent.cpp` | COLD; ICE primitive |
| `src/host/abi/game-host-abi.h` | COLD_ABI |
| `src/renderer/renderer.cpp` | COLD; resident device/window |
| `src/hot-reload/module-host.cpp` | COLD; resident loader |
| `build.py` | TOOLING |
| `config/debuglogger.json` | CONFIG |

## Policy

- A `HOT` edit uses the game-DLL path. Do not request an EXE build.
- A `COLD` edit requires an explicit cold change reason and an EXE build.
- A `COLD_ABI` edit requires both an EXE rebuild and a compatibility review.
- A `HEADER_REVIEW` edit requires checking whether cold or game sources include
  that header; do not assume a header is hot.
- A `MODULE_DLL` edit is not part of the final Cutover-1 path; prefer folding it
  into the game generation.
- Report-only audit/census scripts do not override this guard.

For a changed header or an uncertain include edge, use the report-only
[dependency graph](dependency-graph.md) to see hot and cold dependents.

The guard is advisory tooling. The authoritative build/link gates remain
`build_target_gate.py`, `exe_cutover_gate.py`, and the cold-link trial.

## Integration point

Run the guard before editing or saving a group of changes. The dev-loop may call
it as a preflight in a future change, but it must never start an EXE build by
itself.
