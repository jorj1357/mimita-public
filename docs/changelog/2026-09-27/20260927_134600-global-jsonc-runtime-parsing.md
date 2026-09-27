# Make authored runtime JSON comments work consistently

Time: 2026-09-27 13:46:00 EST  
Result: PASS_WITH_BUILD_PENDING

## Root cause

`config/aimbody.json` was still loaded by `src/entities/aimbody-config.cpp`
with strict `file >> j`, while movement config used comment-enabled parsing.
When the commented-out `"enabled": false` line was present, aimbody parsing
failed and hot reload did not apply the file normally. The loader had already
reset `enabled` to true before parsing, so the failure looked like aimbody was
enabled while its limb map was empty; the animation path therefore had nothing
to apply.

The active value in the user's file is `"enabled": true`; the commented false
line is ignored and cannot override it.

## Fix

Local authored JSON readers now use nlohmann/json comment mode:

`json::parse(file, nullptr, true, true)`

This accepts `//` and `/* ... */` comments while preserving quoted strings.
The shared `src/utils/json-comments.h` and Python `devscripts/jsonc.py`
boundaries remain available for common readers and repository tools.

The change covers aimbody, movement-adjacent config, weapons, rendering,
effects, input, UI, notifications, replay, avatar, gameplay, collision,
networking, gamemode, role, map-pool, and other local config readers. External
HTTP response parsing remains strict because those payloads are protocol data,
not authored config files.

Aimbody now also commits its defaults and limb values only after a successful
comment-tolerant parse. A malformed or partially-written hot-reload cannot
leave `enabled=true` paired with an empty limb map.

## Validation

- `config/aimbody.json` loaded with the repository JSONC reader and resolved
  `enabled=true`.
- `src/entities/aimbody-config.cpp` now loads `//` comments and preserves the
  previous valid settings until a new parse succeeds.
- Build 40 exposed three missing `json` aliases in newly converted config
  readers; those calls now use the fully-qualified `nlohmann::json::parse`.
- Python compilation passed for JSONC and dev-loop tooling.
- `git diff --check` passed.

## Remaining evidence

The full C++ rebuild and live aimbody acceptance are pending because duplicate
pre-existing dev-loop and MiMITA processes are still running and locking build
outputs. No running process was terminated by this session. Build 40's first
compile failure is fixed at source level; a clean compile is still required.
