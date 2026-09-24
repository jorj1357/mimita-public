// 09 24 2026
/* purpose
* Hot logging policy boundary. Declares the one hot provider that owns debug
* logging policy: category/level filtering, event naming, field selection,
* schema/version awareness, sampling, throttling, aggregation, destination
* routing, and JSONL body construction.
* The cold kernel keeps only the stable envelope, the safe append mechanism, and
* the crash-safe flush; `capLogEvent` consults this provider through the
* overridable `log.event` capability and falls back when it declines.
* Does NOT own the file handle, process lifetime, or gameplay state.
*/
#pragma once

#if defined(MIMITA_GAME_DLL)

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaHotLogging {

// log.event override. Fills `out` with a decision plus a prebuilt JSON body
// fragment (no outer braces) valid only for the duration of the call.
void MIMITA_GAME_CALL provideLogEvent(void* host, const GameLogEventV1* event,
                                      GameLogRecordV1* out);

// Per-tick flush. Reloads config when it changes, emits aggregation summaries,
// and reports a malformed config once, all through the `log.append` mechanism.
void MIMITA_GAME_CALL flushTick(void* host, std::uint64_t tick, float dt);

} // namespace MimitaHotLogging

#endif // MIMITA_GAME_DLL
