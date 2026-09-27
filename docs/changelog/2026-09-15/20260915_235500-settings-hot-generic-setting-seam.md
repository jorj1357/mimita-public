# Settings hot-owned via generic setting seam

Date: 2026-09-15 23:55 EST (UTC 2026-09-16T03:55:00Z) [worktree, uncommitted]
Branch: `8292026stash`
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live appearance is proof debt)

## SUBSYSTEMS MIGRATED
Settings composition + interaction policy (sliders/toggles) hot; generic setting
read/write seam added. Settings screen is claimed by hot by default.

## SETTINGS AUDIT (shipping)
Real `PlayerSettings`: fov, masterVolume, musicVolume, sfxVolume, sensitivity
(float), musicMuted (bool), resolution/graphicsPreset (string), equippedSlot
(int), plus internal tuning. Migrated this pass: fov, master, music, sfx,
sensitivity, musicMuted. Deferred: resolution/graphicsPreset (need a SELECT
widget), and the rest are not user-facing shell settings.

## GENERIC SETTING SEAM
- get: `GAME_CAP_SETTING_GET` (`setting.get`) -> `GameSettingV1{settingId,type,
  floatValue,intValue,ok}`.
- set: `GAME_CAP_SETTING_SET` (`setting.set`) -> applies + returns resulting value.
- value types: FLOAT, INT, BOOL.
- validation: kernel clamps (fov 60..140, volumes 0..1, sensitivity 0.01..1);
  presentation cannot inject invalid engine values.

## WIDGETS
- slider: `GAME_UI_SLIDER` (min/max/step), emits `VALUE_CHANGED`.
- toggle: `GAME_UI_TOGGLE` (value 0/1), emits `VALUE_CHANGED`.
- select/dropdown: not added (deferred until a real shipping setting needs it).

## SETTINGS HOT OWNERSHIP
- shipping coverage: all essential float/bool settings above; UI composition,
  labels, ranges, widget choice, navigation hot.
- claim default? Yes (hot `hot.settings-screen` claims `screen.settings`; hot
  main-menu routes `menu.settings` to the hot screen).
- cold owner status: `drawSettingsMenu` is fallback; `drawMainMenu` yields when
  hot owns any screen. Cold GUI_SETTINGS transition no longer taken for the hot
  path.

## SETTINGS GENERATION SAFETY
- remains open across swap: nav state is the migratable `HotUiNavigationStateV1`
  (screenId/previous), so the settings screen stays open across a generation swap.
- values preserved: settings live in cold `PlayerSettings`, untouched by hot
  reload.
- removed/focused widget: widget ids are logical; a removed id simply is not
  emitted; no pointers retained (focus ids are hashes).

## LOADOUT / SPECTATE / SCOREBOARD / CS
Not migrated. Cold owners unchanged. CS objective state not implemented.

## NEW ABI / PRIMITIVES
- `GAME_UI_SLIDER`, `GAME_UI_TOGGLE` + `GameUiCommandV1.minValue/maxValue/step`.
- `GAME_CAP_SETTING_GET`/`SET` + `GameSettingV1` + `GameSettingType`.
WHY GENERIC? Slider/toggle express any numeric/boolean control; the setting seam
maps logical ids to real config with kernel-side validation, reusable by any hot
UI and any setting. No setting-specific ABI.

## DID ANY HOT WORK REQUIRE KILLING mimita.exe?
No (a lingering selftest process exited on its own; cold build waited).

## COLD-RESTART METRIC
Now NO: weapon presentation, actor overlays, TDM/FFA HUD, UI-interaction,
main-menu, **settings composition + interaction policy** (migrated subset).
Still YES: loadout, spectate, scoreboard, CS HUD, discrete settings select,
audio policy, resource generations.

## LIVE-PROOF DEBT
Interactive settings appearance/behaviour, loadout/spectate/scoreboard/CS, audio,
resource swap. Recorded.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E.

## NEXT LARGEST COLD OWNER
Loadout (generic tool enumeration + hot equip action), then spectate, scoreboard,
CS objective state; then audio policy. A generic SELECT widget is the only new
primitive currently anticipated (resolution/preset).

## Files changed
`src/hot-reload/game-api.h`, `src/live-code/live-behavior.cpp`,
`src/live-code/live-ui.cpp`, `src/hot-reload/modules/ui/settings-screen.cpp` (new),
`src/hot-reload/modules/ui/ui-actions.cpp`, `src/gui/menus/main-menu.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`,
`docs/architecture/live-development/hot-cold-audit.md`.
