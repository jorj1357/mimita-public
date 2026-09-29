# Actor preset runtime diagnostic

- Confirmed the dev loop published build 431 while the visible client remained on build 430 because `auto_restart` was disabled.
- Added an `actor_preset_list` diagnostic showing the resolved preset directory, whether it exists, and how many JSON files were found when no presets load.
- Validation: the dev loop completed build 431 successfully and published it to `.dev/builds/0431`.
- Runtime acceptance remains pending until the client is relaunched as build 431 and the diagnostic/list output is observed.
