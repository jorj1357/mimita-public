# Actor preset directory discovery

- Fixed actor-preset loading so the runtime searches the working directory and the executable's parent directories for `config/actor-presets`.
- The loader now scans every `.json` file case-insensitively, logs each discovered file and loaded preset ID, and rebuilds the name index after loading.
- Valid presets are staged before replacing the active set, so a malformed file does not erase previously valid presets.
- Validation: `python build.py build-only` completed successfully; `config/actor-presets/counter_strike.json` parses and is present in the scanned folder.
- Runtime acceptance remains pending until the newly built executable is launched and `actor_preset_list` is observed in the console.
