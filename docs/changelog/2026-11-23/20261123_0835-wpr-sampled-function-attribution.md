# WPR sampled function attribution

- Agent: `resource-efficiency-goal`
- Issue: `issue-ed3ac775283d`
- Run: `20261123_055730_01`
- Runtime under investigation: server PID 8804, client PID 3600 during trace; generation 546 DLL; exact active mode at trace time remains unverified.

## Evidence

The retained full CPU ETL `.dev/perf-captures/mimita-cpu-10ms.etl` spans 36.433 seconds (2026-11-23 07:02:51.327–07:03:27.760 UTC). The ETL reports a 10,000 ns (10 ms) sample interval. A bounded xperf raw export of its first ten seconds contained 1,360 sampled-profile records for server PID 8804; 327 records (24.04%) had instruction pointers inside `game-g546.dll`. The 327 game-DLL instruction samples were symbolized using the DLL's embedded GCC DWARF data and `addr2line`.

| Symbolized function | Samples | Share of 327 game-DLL samples | Location |
| --- | ---: | ---: | --- |
| `std::__detail::_Mod_range_hashing::operator()(...) const` | 8 | 2.45% | GCC `bits/hashtable_policy.h:585` |
| `std::hash<int>::operator()(int)` | 5 | 1.53% | GCC `bits/functional_hash.h:157` |
| `AabbTree::overlaps(AABB const&, AABB const&)` | 4 | 1.22% | `src/physics/movement/collision-aabb-tree.h:118` |

All other resolved function rows in the top twenty had four or fewer samples apiece. The current `collision-aabb-tree.h` SHA-256 matches the immutable generation-546 build snapshot (`50C3A016A64D70E2A9632C872FE71A8A30DEEB08631ADCD0BA46F73BFBA7DCEC`), validating that source location for this function.

These are instruction-sample shares within the 327 sampled frames that landed in the game DLL during one ten-second slice. They are not whole-process CPU percentages or inclusive stack costs. The counts are small and diffuse; they do not justify optimizing a function yet. Runtime mode at the trace start has not been matched to the journal, so workload representativeness is unknown.

## Method and follow-up

The full trace contains kernel sampled-profile events and retains its 10 ms period. Exporting all raw events even for ten seconds created a temporary 1.44 GB text file; it was parsed in a streaming pass and removed afterward. The ETL and smaller derived reports remain in `.dev/perf-captures/`. GCC DWARF symbolization is viable offline even though the generic xperf function report does not decode the MinGW symbols.

Next: align ETL timestamps to server/client journals, reduce future exports to PID and sampled-profile records before materializing data, collect matched workload windows, and aggregate over a larger denominator before selecting source changes. Live capture is paused during the other agent's controlled one-TU PCH A/B and cleanup.

No source code or build inputs were changed by this investigation.
