# Melee, TDM aggression, and regression evidence issue intake

## Report and findings

- Preserved the complete user message once as coordination context ctx-68b4d5e16a5a.
- Created high severity melee bug issue-adfeaafa1136 for human melee failing against NPCs, with a pinned runtime acceptance plan. Linked its context.
- Added the user's TDM corner and sustained pursuit report plus a diagnostic/fix proposal to existing NPC issue issue-b18e63894585. The proposal extends evidence to long-running elite TDM and correlates hostile target choice, target age, route progress, stuck state, tactical state, weapons, and attack outcomes. It recommends bounded stuck-route recovery and continuous hostile pursuit while a valid target is reachable.
- Added evidence/scope suggestion to existing melee issue issue-503f3e410c01, relating the report while keeping Arcade physics and authoritative damaging contacts separate.
- Created regression evidence improvement issue-37a27bfdc575: retain causal explanation, reproduction, source/config identity, build/DLL/run identity, journal predicate, and make proofs stale when relevant inputs change.

## Live evidence and limits

devscripts/dev-loop.py --status reported a stale published EXE (artifact 2026-11-21T02:26:50.306117Z, SHA-256 8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871) and game-DLL generation 423 building. The selected dashboard launch 20261121_152950_01 was mode 58 / Infinite World, selected that stale artifact, and failed before launch because the MiMITA process scan timed out. Its dev-loop events are at logs/2026-11-21/20261121_152950_01/dev-loop/events.jsonl; it is not a melee test. Status listed no running client/server for that run. Two older external server processes were visible; they were left untouched.

Earlier issue evidence is useful but does not establish today's human-to-NPC failure: issue issue-503f3e410c01 records Arcade server contact-pass source findings and failed launches without gameplay journals; its later gen417 TDM AI evidence reports 145 claims sent and zero accepted (122 friendly-fire, 23 invalid-contact-bounds). The current symptom's first failing step remains unknown until reproduced against a pinned current build with client/server journals.

No gameplay source changed, and no build or behavior proof was claimed. The new runtime work waits for an identifiable Fun Counter Strike run and a functioning pinned launch.
