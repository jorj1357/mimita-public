// 09 12 2026
/* purpose
* Provide the single shared implementation of the repository time standard.
* Generates UTC ISO 8601 timestamps, date folders, compact filenames, and
* monotonic elapsed milliseconds for logs, journals, changelogs, and evidence.
* Does NOT open files, choose retention, or know about any specific subsystem.
* Does NOT depend on the debug logger, so any owner may use it safely.
*/
#pragma once

#include <cstdint>
#include <string>

namespace MiMitaTime {

// Canonical machine time: "2026-09-12T19:32:00.123Z" (UTC, millisecond).
std::string utcIso8601Millis();

// Second precision variant used by chat and analytics:
// "2026-09-12T19:32:00Z" (UTC).
std::string utcIso8601Seconds();

// Date-based folder: "yyyy-mm-dd".
std::string utcDateFolder();

// Compact generated filename stamp: "yyyymmdd_hhmmss".
std::string utcCompactStamp();

// Monotonic elapsed milliseconds from a steady clock. Never goes backwards.
std::uint64_t monotonicMillis();

} // namespace MiMitaTime
