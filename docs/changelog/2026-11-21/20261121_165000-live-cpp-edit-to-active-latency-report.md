# Live C++ edit-to-active latency report

## Work completed

- Appended the latest user request verbatim to `issue-136d4553ff92` and kept
  that issue as the investigation owner.
- Reviewed root dev-loop build-attempt summaries and JSONL for successful,
  failed, and slow same-day DLL attempts.
- Checked current source ownership for `src/npc/npc.cpp` and reviewed the
  current run manifest and live server journal read-only.
- Wrote the evidence report at
  `docs/reports/20261121_165000-live-cpp-edit-to-active-latency.md`.
- Appended evidence-backed findings and a prioritized continuation proposal to
  the existing latency issue.

## Evidence summary

- One successful game-DLL attempt took 620.249 s, compiled 170 units, skipped
  472, and recorded 418.840 s compile, 86.354 s object scan, and 34.736 s link.
- Other same-day attempts took 338.688 s and 810.257 s before failing at link,
  while a 2,309.492 s attempt failed in changed-source preflight.
- The latest candidate attempt failed packet-registry codegen after 41.218 s,
  before any game C++ compilation.
- The live run journal records generation-420 NPC events, but its executable
  build is marked stale and mismatched, with a server only and no client. It
  cannot prove that current source or a new edit became active.

## Limitations

This was a read-only investigation. No new build, test run, source edit, or
additional game process was started. Existing successful/failed build attempts
and live journal events are evidence about the observed pipeline and runtime,
not a controlled measurement of a fresh edit. No 0.1 s, 1 s, or 5 s target is
claimed as met. The next required proof is a current pinned DLL/EXE test in the
same process with one NPC behavior edit and its owner event/visible result.
