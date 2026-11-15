This document is superseded. Current hot/cold rules are in [hotreload.md](hotreload.md), and generated source ownership is in `build/source-manifest.json` / `docs/handoffs/hot-reload-ownership-matrix.md`. Historical content is archived at `docs/archive/hotreload/hot-cold-split-2026-11-14.md`.

## Server loop option A (2026-11-14, normative)

The resident server shell owns only the cold mechanism:

- socket create/bind/close and blocking/non-blocking receive,
- send primitives,
- sleep/OS timer,
- fixed-tick scheduler entry and process lifetime,
- startup world/asset construction,
- module host, persistence, auth, logging.

The generation owns every server decision and all gameplay behavior:

- packet handling and validation,
- player/NPC/projectile/vehicle simulation,
- session health, timeout, retry, join/reconnect policy,
- snapshot/publish ordering,
- gamemode and progression,
- ICE peer/session policy,
- shutdown policy.

This is Option A. The resident loop is a mechanical scheduler; it must not
contain gameplay branches, packet switches, or duplicate behavior. If the hot
server API is unavailable, the server refuses to start rather than running the
resident duplicate.

Option B — running the entire receive/send/sleep loop through host callbacks in
the DLL — is deferred. It is only required if measured isolation or build
evidence justifies moving the scheduler itself. Until then, `server.cpp` is a
cold shell and the hot behavior lives in the generation modules.
