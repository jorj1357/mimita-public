This document is superseded. Current state-domain rules are in [hotreload.md](hotreload.md). Historical content is archived at `docs/archive/hotreload/state-domains-2026-11-14.md`.

## First domain capability (2026-11-14)

- `EntityDomainCapabilityV1` (`src/host/abi/entity-domain-capability.h`) exposes
  generated `player_core` and `player_session` records by stable entity handle.
- `HostServerEntityAdapter` implements read/write for both domains with field
  masks; write treats an empty mask as all fields.
- `src/game/entity-domain-bridge-module.cpp` is the generation-side bridge.
- The capability is wired in `HostRuntime::getCapabilityThunk` and initialized
  from `game-module.cpp`.
- Remaining domains and handler migrations are pending. The wide
  `EntitySnapshotV1` is still required by unmigrated handlers.
