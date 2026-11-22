# FCS NPC-on-NPC kill and bomb-plant evidence review

## Task

- Agent: issue-review-sweeper
- Context: ctx-c00ad9484fa1 (high priority)
- Issue: issue-be1adb51b6ac — Fun Counter Strike NPC objective behavior regressed
- Scope: independent review and evidence recording only; no source edits, no
  commit.

## What was verified

Run `20261122_023327_fcs-proof`, launch mode 45 (fun counter strike), map
`dust2cyberiav4`, game DLL generation 441, sha256
`c469fa8223ee9b2d3be143e52f84b702a7601fbc1dd2c2977b6e203aa3aa3cd2`.
Server journal:
`logs/2026-11-22/20261122_023327_fcs-proof/server-01/events.jsonl`.

- t=871.552 `objective.plant-start`, operator 100018, `operator_kind=npc`,
  site B, mode hold.
- t=882.928 `objective.planted`, operator 100018, site B,
  `explosion_seconds=40`.
- t=929.200 `objective.exploded`, site B.
- t=929.201 `round.result`, round 9, `winner_team=1`, `reason=2`,
  `wins_ct=8`, `wins_t=1` (bomb detonation decided the round).
- 22 `kill.reward` events with `killer_is_npc=true` and
  `victim_is_npc=true`, all cross-team (team 0 vs team 1), e.g.
  100005 (team 0) chainsaw to 100018 (team 1), and 100007 (team 0)
  swordsword to 100012 (team 1).
- 32 `npc.death`, 542 `server.damage.applied`.
- CT NPC 100005 issued `objective.defuse-start` three times
  (`operator_kind=npc`) and was interrupted.

Team mapping: planter 100018 and carrier 100012 are team 1 (T); attackers
100005/100007 are team 0 (CT); `objective.carried` carrier 100012 has
`allowed_team=1`.

## Evidence recorded

`proof record issue-be1adb51b6ac` created issue evidence
`evidence-8572793982d1` with required events `objective.planted`,
`objective.exploded`, `kill.reward`, `round.result`,
`server.damage.applied`, generation 441. Verification result: `proven`.

An issue review (verdict `evidence`) was appended at 03:26Z correcting the
03:03Z review, which had sampled the run before the plant happened.

## Caveats

- The journal was compacted/rotated; the retained window starts near t=849 but
  still contains the complete plant chain.
- This proves generation 441. Source advanced during the review (generation 443
  observed, generation 444/446 build attempts in progress), so a longer pinned
  run on the current published generation is still required before claiming the
  latest build or the CT-displacement part of the original acceptance.
- No human visual or gameplay review was performed.

## Files touched

- `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md`
  (generated append-only review entries only)
- Coordination brain `.dev/agent-coord/brain.db` (issue evidence and review)
- This changelog

No source, config, packet, or build files were edited.

## Next step

Owner `fun-cs-npc-objective` was given the exact gen441 chain and asked to run
the current published generation long enough to capture a plant plus
cross-team kills, then compare CT displacement from `ct_spawn`.
