# Game-DLL undefined-symbol worklist (2026-11-12)

**Status:** generated from the whole-game linker log. This is a worklist,
not a runtime or correctness proof.

- Link log: `C:/mimita-v9/build/game/progress.log`
- Undefined references: **554**
- Unique symbols: **300**
- Machine table: `C:/mimita-v9/docs/handoffs/game-dll-undefined-worklist-20261112.csv`

## By family

| Family | Unique | Refs |
|---|---:|---:|
| GLAD device | 87 | 87 |
| ModuleHost policy | 54 | 76 |
| process globals | 36 | 36 |
| audio helpers | 26 | 66 |
| MimitaNet (session/server/world-rebuild) | 19 | 94 |
| duel queue | 13 | 43 |
| ICE transport | 13 | 59 |
| persistence | 11 | 17 |
| Renderer device | 8 | 9 |
| other | 7 | 13 |
| HotReloadSystem | 5 | 18 |
| self-tests | 5 | 5 |
| crosshair/GUI | 4 | 4 |
| LogManager | 3 | 10 |
| json watch jobs | 3 | 6 |
| replay save jobs | 2 | 6 |
| direct host calls | 2 | 3 |
| crash handler | 2 | 2 |

## By strategy

| Strategy | Unique |
|---|---:|
| typed-family-bridge | 112 |
| CAP_RENDER command stream | 95 |
| CAP_ENTITY/CAP_WORLD handle | 36 |
| split: transport vs session/state | 19 |
| keep resident; CAP_TRANSPORT signaling | 13 |
| CAP_JOBS lease | 5 |
| exclude or test owner | 5 |
| CAP_GUI | 4 |
| classify | 4 |
| do not bridge; redesign bounded | 3 |
| use HostServices, not HostRuntime | 2 |
| resident bridge | 2 |

## Top 25

| Refs | Family | Signature | Symbol |
|---:|---|---|---|
| 28 | duel queue | scalar | `DuelQueue::instance()` |
| 19 | MimitaNet (session/server/world-rebuild) | scalar | `MimitaNet::serverGameOverrides()` |
| 17 | MimitaNet (session/server/world-rebuild) | scalar | `MimitaNet::netShutdown()` |
| 16 | ICE transport | scalar | `IceAgent::state() const` |
| 14 | MimitaNet (session/server/world-rebuild) | scalar | `MimitaNet::isServerHost()` |
| 14 | MimitaNet (session/server/world-rebuild) | reference | `MimitaNet::serverSendToPlayer(unsigned long long, MimitaNet::ServerPlayer const&, void const*, unsigned long long)` |
| 9 | HotReloadSystem | scalar | `HotReloadSystem::instance()` |
| 7 | ICE transport | scalar | `IceAgent::~IceAgent()` |
| 7 | LogManager | scalar | `LogManager::instance()` |
| 6 | ICE transport | complex-ownership | `IceAgent::pollEvents(std::vector<IceEvent, std::allocator<IceEvent> >&)` |
| 6 | MimitaNet (session/server/world-rebuild) | scalar | `MimitaNet::netStartup()` |
| 6 | ModuleHost policy | scalar | `ModuleHost::worldgenResolve(mimita_worldgen::ResolveV1*)` |
| 6 | audio helpers | complex-ownership | `audioOwnerKey(unsigned int, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)` |
| 6 | audio helpers | complex-ownership | `playSound(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, float)` |
| 6 | persistence | scalar | `PersistenceQueue::instance()` |
| 5 | MimitaNet (session/server/world-rebuild) | complex-ownership | `MimitaNet::startListenServer(MimitaNet::ListenServerState&, unsigned short, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, MimitaNet::ServerLaunchSettings const*)` |
| 5 | ModuleHost policy | scalar | `ModuleHost::invokeEvent(unsigned int, void*, unsigned int, unsigned long long)` |
| 5 | audio helpers | scalar | `seekReplayMusicPreview(float)` |
| 5 | audio helpers | scalar | `setReplayMusicPreviewVolume(float)` |
| 5 | audio helpers | scalar | `stopReplayMusicPreview()` |
| 4 | HotReloadSystem | scalar | `HotReloadSystem::gameMemory()` |
| 4 | ICE transport | scalar | `IceAgent::IceAgent()` |
| 4 | ICE transport | scalar | `IceAgent::gatherCandidates()` |
| 4 | ICE transport | reference | `IceAgent::initialize(IceConfiguration const&)` |
| 4 | ICE transport | scalar | `IceAgent::localSdp[abi:cxx11]() const` |
