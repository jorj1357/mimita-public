# Streamed world frame stalls

- First recorded: 2026-11-06 (exact report time not recorded here).
- Status: UNRESOLVED. A radius-10 movement run and visual review are pending.
- Target: 240 FPS during movement (each frame at or below 4.17 ms) with a
  fixed 60 Hz server tick.

## 2026-11-06 baseline

The live client journal at
`logs/2026-11-06/20261106_232553/client-01/events.jsonl` contains 473
`world-stream.chunk.applied` events across three chunk variants and 245
`world-stream.update` events. The last update reports 25 loaded cells, a
two-cell radius, `whole_world=true`, and `spacing_mode=connector`. Across
1,006 frame windows with at least 50 frames, the highest window average was
3.886 ms, the worst individual frame was 40.409 ms, and 866 windows contained
at least one frame over the 4.17 ms budget. Thus this run did not meet strict
240 FPS acceptance, despite its good window averages.

The worst measured frame window attributed up to 29.08 ms to `Menus` and
10.50 ms to `Swap`; another had 15.51 ms in `Menus` and 10.58 ms in
`Audio::MusicConfigPoll`. These are measured contributors in the same
windows, not proof that chunk loading caused every stall. Chunk install time
and frame timing need correlation in a repeatable radius-10 run.

## Attempted source change

The world-stream runtime now caches the desired cell set and sorted nearby
offsets until the player crosses a cell or the radius changes. This removes
the previous per-frame neighborhood rebuild and sort. Chunk installation
remains capped at two per render setup pass. A successful build or this source
change alone does not establish an FPS improvement.

## Required follow-up

Run Infinite World: Cramped with radius 10, traverse beyond X=250, record
server 60 Hz windows, client frame windows and maximums, chunk install times,
loaded/pending cell counts, and a human visual pass. Append each repeat run's
exact build identity and journal path here. Keep this record unresolved until
the target has measured evidence.
