# Comment-tolerant config parsing and single dev-loop ownership

Time: 2026-09-27 12:30:00 EST  
Result: BLOCKED_PENDING_PROCESS_CLEANUP

## Changes made

- Added `src/utils/json-comments.h`, a shared nlohmann/json parser boundary
  using comment-tolerant parsing for runtime JSON configuration.
- Updated the gamemode registry, role registry, community server config, and
  allowed-map pool loaders to accept `//` and `/* ... */` comments.
- Restored a small comment header in `config/gamemodes/retrograd.json` to keep
  the original config intent while exercising the new parser behavior.
- Added a per-checkout lock to `devscripts/dev-loop.py` so future launches
  reject a second daemon for `C:\mimita-v9`.

## Diagnosis

Two independent issues were confirmed:

1. The server startup crash was caused by the commented `retrograd.json` file
   being read by a strict JSON path. The runtime probe passed when comments
   were removed, and the new parser is intended to preserve those comments.
2. Two existing dev-loop daemon pairs were already running. They each
   launched build 33, producing two room codes (`LD4SDFR` and `DC7J2UC`) and
   multiple clients. They then continued racing through builds 34 and 35.

## Validation

- `python -m py_compile devscripts/dev-loop.py`: passed.
- JSON parse of Counter-Strike and Retrograd configs: passed.
- A build was attempted, but could not complete because the existing running
  MiMITA/dev-loop processes locked the build output and triggered concurrent
  builds. No running process was terminated by this session.

## Required next step

Close every existing dev-loop window with `Q`, allow its child MiMITA windows
to close, confirm no `py.exe`, `python.exe`, or `mimita.exe` remains for this
checkout, then start exactly one loop. The C++ parser needs one clean build
after that cleanup.
