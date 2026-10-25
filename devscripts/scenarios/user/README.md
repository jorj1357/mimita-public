# User input recordings

Local-only per-tick input recordings live here. They are gitignored by default.

```text
scenario_startrecord coolplace_dash
# play normally: walk, dash, shoot, reload, switch weapon
scenario_endrecord
```

This writes `scenario-YYYYMMDD_HHMMSS-coolplace_dash.jsonl` in this folder.

Play it back:

```text
scenario_play devscripts/scenarios/user/scenario-YYYYMMDD_HHMMSS-coolplace_dash.jsonl
```

Or reference it from a dev-loop scenario with an `input_playback` step.

To keep a recording as a checked-in regression, move it out of `user/` into
`devscripts/scenarios/` and add a small scenario JSON that references it.
