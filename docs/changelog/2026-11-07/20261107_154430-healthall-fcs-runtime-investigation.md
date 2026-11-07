# Healthall and Fun Counter Strike runtime investigation

- Time: 2026-11-07T15:44:30Z (UTC).
- Branch: `20261011zt`; no deployment. Git status could not be read because the repository index reports `bad signature 0x00000000`.
- Result: FCS mode-47 loadout corrected and verified in a fresh two-client run. Healthall global 500/500 assignment is verified for every live actor, but combat still kills at lethal body-part destruction; this behavior remains unfixed pending a different approach.

## Findings and changes

- `devscripts/dev-launch-modes.json`: launch mode 47 now declares `weapon_set: auto`, so the `npc-navigation` profile's Captain Falcon set 12 is not passed as an explicit override.
- `docs/specs/gamemodes/funcounterstrike.md`: requires effective set 30 / `funcounterstrike` on every FCS launcher and rejects Captain Falcon IDs; specifies server/client spawn journal acceptance.
- `docs/regressions/gameplay/funcounterstrike-launch-loadout-REG-2026-11-05.md`: appended the second occurrence, the reason the earlier launch-mode-45-only fix did not cover mode 47, and fresh proof.
- `docs/regressions/gameplay/healthall-body-part-lethal-REG-2026-11-07.md`: records the newly proven second healthall issue and leaves the behavior decision open.
- Earlier in this task, `src/network/server-packet-chat.cpp` and network client owners received structured diagnostics for per-actor `healthall` assignment, client HP state, and damage confirmation. Those diagnostics exposed the distinction between global actor HP and the separate lethal body-part pools. No damage/death behavior change was made for this newly identified issue.

## Build and runtime evidence

- Canonical dev-loop build succeeded and was current: `2026-11-07T15:40:16.360512Z:032addb5b338935afeda24da592c38af785dc5e33316a71888a53fb24b1adb77`; status reported source-current, source generation 1.
- Run `20261107_154026_01` launched launch mode 47 on `dust2cyberiav4`; server and both clients matched that build.
- FCS server `gamemode.match.configured` sequence 24 records weapon set 30 / `gamemode_default`. Player `actor.loadout.spawned` sequences 139 and 144 record roles `fun_terrorist` and `fun_counter_terrorist`, set ID 30/key `funcounterstrike`, the 13-item inventory, and `revolver` equipped. Client spawn packets report community set ID 30. NPC shots use configured FCS IDs including revolver, shotgun, rocket_launcher, frag, smoke, and fire; no Captain Falcon IDs appear in fresh-run loadout/fire evidence.
- Server command result sequence 7160 accepts `healthall 500`; sequences 7161–7180 show all 2 players and 18 NPCs at current/max 500 (NPC mirrors also 500), and summary sequence 7181 records those counts. Both clients record 500/500 applied.
- Server damage sequence 7977 kills player 1 with a 15-point hitscan hit at 405 global HP because the torso part reaches 0 and is marked destroyed. Client 01 sequence 3640 independently confirms 405→0 from damage 15. Server sequences 8860 and 73810 show separate grenade explosions for 457 and 1000 damage.
- Run stopped after evidence collection. Journals: `logs/2026-11-07/20261107_154026_01/{server-01,client-01,client-02}/events.jsonl`. No visual or human gameplay acceptance claimed.

## Worktree recovery incident

During this task, an unsuccessful patch operation zero-filled `devscripts/dev-launch-modes.json` and `docs/specs/gamemodes/funcounterstrike.md`; their committed versions were restored and the requested edits reapplied. The worktree also contained zero-filled `src/procedural/infinite-world-runtime.cpp` and `config/maps/connected_world.json`; to restore a successful build and valid config, their committed versions were restored. This may have discarded uncommitted connected-world edits in those files. Other dirty work was left untouched. The Git index remains corrupt, so a complete diff audit was unavailable.
