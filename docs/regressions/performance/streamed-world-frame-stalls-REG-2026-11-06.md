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

## 2026-11-07 radius-10 Infinite World: Cramped run

Build identity:
`2026-11-07T18:46:29.140437Z:aeca5bcbff24d6256448714fbd2d4d4c297ff1a51f8033d39cec9e20e3876f4a`.

Journals:

- client: `logs/2026-11-07/20261107_184735/client-01/events.jsonl`
- server: `logs/2026-11-07/20261107_184735/server-01/events.jsonl`

Observed:

- Client profile: `infinite_cramped`, seed `583294`, radius `[10,10,0]`,
  desired `441` cells; loaded reached `441`.
- Client travelled beyond the requested X=250 check to position
  `(370.39, 49.40, 1.60)`, cell `(37, 4, 0)`, still with `desired=441`.
- Server identity descriptor hash `8036895348427122391`, seed `583294`,
  generator version `1`; the client logged the same descriptor hash.
- Server authoritative initial footprint: `441` cells, `5304` collision
  triangles, `277` chunk spawn anchors, radius `[10,10,0]`.
- One client 60-frame window during travel: average `25.49 ms`, maximum
  `131.73 ms`, `80%` of frames over the 4.17 ms 240 FPS budget.
- Server tick windows were normally sub-3 ms but one fixed tick reached
  `130.36 ms` during the run.

This does not meet the 240 FPS target. The long frame and tick maxima still
need attribution; chunk install/removal during travel is a prime suspect but
was not isolated in this run. Human visual acceptance was not performed.

## Required follow-up

Run Infinite World: Cramped with radius 10, traverse beyond X=250, record
server 60 Hz windows, client frame windows and maximums, chunk install times,
loaded/pending cell counts, and a human visual pass. Append each repeat run's
exact build identity and journal path here. Keep this record unresolved until
the target has measured evidence.
