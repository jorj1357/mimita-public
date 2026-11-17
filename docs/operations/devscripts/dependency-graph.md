# Dependency graph

As recorded: 2026-11-17  
Owner: development diagnostics  
Authority: report-only

`devscripts/dependency_graph.py` reads compiler-generated `.d` files and reports
which translation units include which repository headers. It answers build,
ownership, and hot-boundary questions without compiling or requesting a build.

## Why it exists

- Show direct and transitive header fan-in before changing a shared header.
- Show the affected source set for a changed header or source.
- Find hot game sources that directly depend on resident, bootstrap, cold-ABI,
  or vendor paths.
- Identify stale `.d` files whose source is newer than the last compile.
- Find dead or unexpectedly broad source/header families.

It does not prove runtime behavior, hot activation, or Cutover-1 completion.
A successful hot edit still needs a game-DLL build, same-PID activation, the
owner event in `events.jsonl`, and human review where behavior is visible.

## Inputs

- Compiler `.d` files under the latest `build/game/objects/<generation>/`
  directory and `build/obj-debug` / `build/obj-release`.
- `build/source-manifest.json` for the source revision and owners.
- Explicit `.d` files or directories when passed as arguments.

By default the report focuses on `src/` dependencies. `--all` scans every
`.d` file under `build/` and also records `include/`, `external/`, `tools/`,
and `tests/` dependencies.

## Usage

```text
python devscripts/dependency_graph.py
python devscripts/dependency_graph.py --write
python devscripts/dependency_graph.py --top 50
python devscripts/dependency_graph.py --focus src/debug/structured-log.h
python devscripts/dependency_graph.py --dependents src/host/abi/transport-session-capability.h
python devscripts/dependency_graph.py --affected src/network/packets.h
python devscripts/dependency_graph.py --all --json build/reports/dependency-graph-all.json
```

- `--write` writes `build/reports/dependency-graph.json`.
- `--json PATH` writes a custom report path.
- `--focus PATH` prints direct dependents.
- `--dependents PATH` prints direct and transitive dependents.
- `--affected PATH` prints the sources that would rebuild because of that path.
- `--all` widens the scan and includes external dependencies.

## Reading the report

- `sources` maps each source to its newest parsed `.d` file, owner, and direct
  dependencies.
- `reverse` maps each dependency to the sources that include it directly.
- `fan_in` ranks headers by direct dependent count.
- `hot_to_cold_edges` lists `game` sources whose direct dependencies resolve to
  `resident`, `bootstrap`, `launcher`, `cold_abi`, or `vendor`.
- `stale_sources` lists sources whose current file timestamp is newer than the
  `.d` file used for the graph. Rebuild before treating that graph as current.
- `owner_inferred` marks a dependency owner inferred from a sibling source or
  path rule rather than an exact `build/source-manifest.json` source entry.

## Limits

- `.d` files describe the last compile, not the current tree.
- The newest `.d` per source wins when debug, release, and game builds disagree.
- Headers are not all listed in the source-owner manifest. Header ownership is
  therefore inferred from a sibling source or path rule; confirm with
  `cold_change_guard.py` before editing.
- A hot-to-cold include edge is a review signal, not automatically a cold
  change. The header may be a stable ABI or logging include. The changed
  behavior owner still decides hotness.
- The report is not a Cutover gate. Use the cold guard, source manifest, build
  target gate, EXE cutover gate, and cold-link trial for authoritative gates.

## Related documents

- [Development script inventory](script-inventory.md)
- [Cold-change guard](cold-change-guard.md)
- [Hot reload specification](../../specs/hotreload/hotreload.md)
- [Hotness matrix](../../specs/hotreload/hotness-matrix.md)
- [Networking specification](../../specs/networking/networking.md)
