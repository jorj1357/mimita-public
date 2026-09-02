# Hot-Reloadable JSON Config Files

THIS should DEFINE EVERY SINGLE .JSON HOT RELOADABLE ELEMENT IN THE ENTIRE REPO as of 9 2 2026
right now its missing some things like hitfx.json, etc 

# everything should be hot reloadable over time
so u dont have to rebuild the exe every time u want to edit UI or weapon values etc 

To apply changes: save the JSON file, wait ~1 second for hot-reload, changes appear immediately.

| `config/weaponsets.json` | Weapon set configurations | Weapon lists |
| `config/onlinemodes.json` | Community server modes | Mode definitions |
| `config/killfeed.json` | Killfeed display mode and weapon verbs | `mode` ("hud" or "chat"), weapon verbs |
| `config/gamemodes/duel.json` | Duel mode settings | `goal_value`, `time_limit_seconds` |
| `config/gamemodes/tdm.json` | TDM mode settings | `goal_value`, `time_limit_seconds`, `countdown_seconds` |
| `config/gamemodes/ffa.json` | FFA mode settings | `goal_value`, `time_limit_seconds`, `countdown_seconds` |
| `config/gui/pause-menu.json` | ESC pause menu | Button text, positions, colors |
| `config/gui/help-menu.json` | Help menu content and layout | Text, positions, colors |
| `config/gui/duel-queue-hud.json` | Duel queue/waiting screen | All elements |
| `config/gui/duel-match-hud.json` | Network duel match HUD (countdown, scoreboard) | `fontSize`, `textColor`, positions |
| `config/gui/duel-hud.json` | Local duel HUD (countdown, score, timer) | `fontSize`, `textColor`, positions |
| `config/gui/match-hud.json` | Match countdown, intermission text, recording indicator | `fontSize`, `textColor`, `x`, `y` positions |

These files are hot-reloaded automatically according to some internal ms/timer not sure as of 9 2 2026. Edit them while the game is running, save it, then it will update after like 250 ms or 500 ms or 1000 ms idk 9 2 2026.
