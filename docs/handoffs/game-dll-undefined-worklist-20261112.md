# Game-DLL undefined-symbol worklist (2026-11-12)

**Status:** generated from the whole-game linker log. This is a worklist,
not a runtime or correctness proof.

- Link log: `C:/mimita-v9/build/game/progress.log`
- Undefined references: **33**
- Unique symbols: **33**
- Machine table: `C:/mimita-v9/docs/handoffs/game-dll-undefined-worklist-20261112.csv`

## By family

| Family | Unique | Refs |
|---|---:|---:|
| process globals | 30 | 30 |
| replay save jobs | 2 | 2 |
| LogManager | 1 | 1 |

## By strategy

| Strategy | Unique |
|---|---:|
| CAP_ENTITY/CAP_WORLD handle | 30 |
| CAP_JOBS lease | 2 |
| typed-family-bridge | 1 |

## Top 25

| Refs | Family | Signature | Symbol |
|---:|---|---|---|
| 1 | LogManager | scalar | `LogManager::instance()` |
| 1 | process globals | scalar | `gAiLaunchMode` |
| 1 | process globals | scalar | `gNetDebugEntities` |
| 1 | process globals | scalar | `gUnlockCursorOnLaunch` |
| 1 | process globals | scalar | `gpActiveGameMode[abi:cxx11]` |
| 1 | process globals | scalar | `gpActiveMapPath[abi:cxx11]` |
| 1 | process globals | scalar | `gpBindPrev` |
| 1 | process globals | scalar | `gpCamera` |
| 1 | process globals | scalar | `gpChatHistory` |
| 1 | process globals | scalar | `gpCommandBinds[abi:cxx11]` |
| 1 | process globals | scalar | `gpDeathPosition` |
| 1 | process globals | scalar | `gpDuelConfig` |
| 1 | process globals | scalar | `gpEditorMode` |
| 1 | process globals | scalar | `gpFreecamEnabled` |
| 1 | process globals | scalar | `gpGameState` |
| 1 | process globals | scalar | `gpMpContext` |
| 1 | process globals | scalar | `gpNpcSystem` |
| 1 | process globals | scalar | `gpPlayer` |
| 1 | process globals | scalar | `gpReplayActorModels[abi:cxx11]` |
| 1 | process globals | scalar | `gpReplayBrowser` |
| 1 | process globals | scalar | `gpReplayCameraMgr` |
| 1 | process globals | scalar | `gpReplayChatStates[abi:cxx11]` |
| 1 | process globals | scalar | `gpReplayClipsCache[abi:cxx11]` |
| 1 | process globals | scalar | `gpReplayFactory` |
| 1 | process globals | scalar | `gpReplayPlayer` |
