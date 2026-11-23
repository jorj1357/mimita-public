# Offline WPR symbol resolution

## Session-specific evidence

This activation continued the active resource-efficiency investigation. I did
not start WPR, builds, or game processes. I resolved the retained 10 ms ETL
stack report against the matching generation-546 DLL, then appended the results
to `issue-ed3ac775283d`.

## Exact source changes

- No source code or configuration files changed.
- Created this changelog; there was no prior text. The file records the
symbolization method, results, limits, and next evidence needed.
- Appended a new `Agent suggestion` entry to
`docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md`.

## Artifacts and method

- ETL: `.dev/perf-captures/mimita-cpu-10ms.etl`, 2026-11-23
  07:02:51.327–07:03:27.760 UTC, 36.433 s, 10 ms sample interval, zero lost
  buffers/events.
- Workload aligned by existing runtime evidence: 12 logical CPUs, Juggernaut
  Horde, `dust2cyberiav4`, 67 NPCs, 1 player, generation 546.
- Input reports:
  `.dev/perf-captures/mimita-cpu-10ms-client-symbols.html` and
  `.dev/perf-captures/mimita-cpu-10ms-server-symbols.html`.
- Symbolizer: `C:\important\msys64\mingw64\bin\addr2line.exe -f -C`;
  binary: `build/game/generation-546/game-g546.dll`. The report offsets were
  resolved as RVAs after adding PE image base `0x180000000`.
- Result: symbols resolved for 807/867 client and 462/492 server exclusive
  game-DLL sample hits.
- ETW module totals in this capture: client PID 3600 used 18.72% whole-host
  CPU; server PID 8804 used 10.39%; combined 29.11%. These are module/process
  totals; function hit shares are not CPU milliseconds or percentages of total
  process CPU.
- Highest individual source hit shares remained small: client UIVertex
  allocator construction 38/867 (4.4% of game-DLL exclusive hits), Textured
  Particle vertex construction 28/867 (3.2%); audio volume factor 18/867
  (2.1%) and resampler 28/867 across two adjacent lines (3.2%). Server glm
  vec4 construction 13/492 (2.6%) and `AabbTree::overlaps` at
  `src/physics/movement/collision-aabb-tree.h:118` 10/492 (2.0%). No one
  function is demonstrated as the cause of the pair's CPU usage.

## Validation and limits

- Read-only symbol mapping completed against the retained DLL. No runtime
  behavior or performance improvement was tested.
- The capture provides sampled stack hits, not bounded per-function CPU-ms.
  The prior 1 ms capture included material apparent ETW stack-walk overhead;
  it is not comparable to this 10 ms capture or to an uninstrumented baseline.
- Next discriminating evidence: a matched CPU-light process/module baseline and
  an opt-in bounded per-thread CPU-time profiler that attributes CPU-ms to
  source functions, with its overhead checked against a no-profiler run.
- Runtime journal at the later current time may have stopped at the 7 GB quota;
  current mode and workload are unknown. No changes were made to address that.
