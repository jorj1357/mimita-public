# Juggernaut Duel slot contract investigation

- Timestamp: 2026-10-16T04:36:14Z (2026-10-16 00:36:14 America/New_York, EDT)
- Branch/HEAD: `20261011zt` / `3a345200`
- Result: `SLOT_NAMESPACE_MISMATCH_DOCUMENTED; CODE_FIX_AND_LIVE_RECHECK_PENDING`

## Outcome

Expanded `docs/specs/gamemodes/juggernaut-duel.md` with the reported Jorj
networking attempt at 2026-10-16 00:34, explicitly marked as not independently
verified; a distinction between logical weapon-set slot, native weapon slot,
and stable weapon identity; the client-to-server equip and attack path; why
this mismatch rejects before authoritative tracing; and a bounded sequence to
finish the mode.

## Evidence and conclusion

- Supplied `logs/2026-10-16/20261016_042126/events.jsonl` records 321
  `weapon.attack.slot_mismatch` events for Duel weapons, including LMG logical
  slot 2 versus expected 9 with actor set 11 and mode set 1, and Heavy Shotgun
  requested slot 1 versus expected native fallback slot 24.
- `mpSendAttackRequest` in `src/network/multiplayer-packets.cpp` sends both
  `weaponDefNetworkId` and `equippedSlot`.
- `applyEquipIntentFromInput` in `src/network/server-packets.cpp` validates an
  equip selection against player ownership and converts logical to native
  slot. `handleAttackRequest` in `src/network/server-attack.cpp` compares the
  submitted slot with the server-computed expected slot and returns on
  mismatch before shot tracing.
- `serverCommunityWeaponLogicalSlot(const ServerPlayer&, ...)` in
  `src/network/server-gamemode.cpp` selects the participant weapon set only
  for objective-round mode; Duel score-limit mode falls back to the global
  mode set. Thus the observed mismatch is a wrong-namespace/set comparison,
  not evidence that the client hit geometry missed.
- Recommendation recorded: keep authoritative ownership/equipped checks;
  resolve expected logical position using the same role set that granted the
  weapon, and convert to native slot at the equipment boundary. Do not accept
  arbitrary weapon IDs or simply ignore all slot state.

## Validation and limits

- Spec was checked for trailing whitespace; `git diff --check` reported no
  whitespace errors (only existing line-ending warnings for unrelated avatar
  files).
- No implementation code, build, or tests were changed/run for this
  documentation investigation.
- No live mode switch or process replacement was performed. The earlier
  read-only version check identified the active client and journal, but a
  post-fix Duel replay, accepted damage, replication, multiplayer, and full
  round-cycle evidence remain pending.
- Existing worktree edits were preserved.
