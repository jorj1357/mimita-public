# Current DLL wait evidence

## Change

Appended measured queue-wait evidence for the active root DLL attempt to existing build-friction issue `issue-e84cfa28c7f7` and notified `game-dll-build` with attempt ID, source generation, completed count, and oldest translation-unit queue wait.

## Evidence

- Root attempt `game-dll-20261120T182726149737Z-df8100` remains active at source generation 29, generation 409. Timing journal reached 610/642 completed translation units by 18:59:58Z; the oldest queued translation unit reported 1,696,981 ms of queue wait.
- Its attempt context has no attached build notes and does not include `src/network/reliable-gameplay-events.cpp`; it cannot prove the saved connection-removal diagnostic.
- Source generation remains 31. The artifact/runtime build identity is still the prior build; no pinned runtime test was started.
- Existing online TDM test runs were not touched.

## Next step

Poll this same attempt to terminal publication or failure, then verify the queued current-source build includes the diagnostic and its build note before starting an isolated TDM lifecycle run.
