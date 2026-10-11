# Sandbox all-weapons equip mapping

Date: 2026-10-11 (America/New_York)

## Outcome

Sandbox now selects the existing All weapons set (`weapon_set_id: 5`). The set
keeps wildcard permission for future definitions but also lists every current
weapon explicitly so logical `equipslot(n)` requests can resolve to native
weapon slots.

The live terminal now also supports `equipweapon <weapon_id>`. It resolves the
weapon identity through the active set, equips the native definition locally,
and lets the normal network path translate it back to the logical slot. This
allows `equipweapon heavy_shotgun` without changing the stable 1–9 hotbar.

## Mapping

The stable sandbox slots remain unchanged. The Heavy Shotgun is logical slot
19 and native definition slot 24. Use `equipslot19` in sandbox; the normal
Juggernaut role remains restricted to its own logical slot 1.

## Evidence

- Config diff checked clean with `git diff --check`.
- `python devscripts/dev-loop.py --status --json` observed the existing loop;
  its source change produced successful build 1995, then the controlled launch
  produced build 1996 with matching client/server binaries.
- The existing loop had been started with an older explicit `--weapon-set 1`
  snapshot, so live acceptance of the new all-weapons permission and command
  remains unverified in this session. The profile is now configured for set 5
  for the next fresh dev-loop launch.
- `AI-fix-live.md` and `AGENTS.md` now require this dev-loop/runtime-journal
  evidence flow for future live fixes.
