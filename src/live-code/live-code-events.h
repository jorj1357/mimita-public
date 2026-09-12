// 09 12 2026
/* purpose
* Own the user-visible and recordable lifecycle events of live-code reloads.
* Shows notifications, plays sounds, and writes journal lines for edit,
* compile, failure, candidate-ready, activation, and rollback transitions.
* Does NOT compile code, load modules, or own the hot-reload state machine.
* Does NOT own the notification or audio backends it drives.
*/
#pragma once

#include <cstdint>
#include <string>

namespace LiveCodeEvents {

// Called when a source change is detected and a build is queued.
void notifyEditDetected(const std::string& file);

// Called when the background build worker actually starts.
void notifyCompiling(const std::string& file);

// Called when compilation fails. The previous generation stays active.
void notifyCompileFailed(const std::string& file, const std::string& error);

// Called when a candidate passed API/ABI and deterministic self-test.
void notifyCandidateReady(std::uint32_t generation, const std::string& codeHash);

// Called when a candidate becomes the active function table.
void notifyActivated(std::uint32_t generation, const std::string& codeHash);

// Called when the previous generation is reactivated.
void notifyRollbackActivated(std::uint32_t generation, const std::string& codeHash);

// Record that a notification was emitted for a live-code event.
void notifyValidationFailed(std::uint32_t generation, const std::string& error);

} // namespace LiveCodeEvents
