// 09 12 2026
/* purpose
* Implements live-code lifecycle notifications, sounds, and journal lines.
* Owns the mapping from reload transitions to user-visible feedback.
* Does NOT compile, load, or validate modules; callers own those transitions.
*/
#include "live-code/live-code-events.h"

#include "audio/audio.h"
#include "debug/debug-log.h"
#include "live-code/live-journal.h"
#include "notifications/notifications.h"

namespace LiveCodeEvents {

void notifyEditDetected(const std::string& file)
{
    NotificationSystem::instance().push(
        "LIVE CODE", "edit detected: " + file, 150, {});
    LiveEventJournal::Fields fields;
    fields.file = file;
    LiveEventJournal::instance().record("edit_detected", fields);
    Debug::log(Debug::Category::General, "[LIVE CODE] edit detected file=%s\n", file.c_str());
}

void notifyCompiling(const std::string& file)
{
    NotificationSystem::instance().push(
        "LIVE CODE", "compiling " + file + "...", 180, {});
    LiveEventJournal::Fields fields;
    fields.file = file;
    LiveEventJournal::instance().record("compile_started", fields);
}

void notifyCompileFailed(const std::string& file, const std::string& error)
{
    NotificationSystem::instance().pushCritical(
        "LIVE CODE", "compile failed, previous version retained\n" + error, 0);
    playSound("live/failure", 0.7f);
    LiveEventJournal::Fields fields;
    fields.file = file;
    fields.result = "failed";
    fields.error = error;
    LiveEventJournal::instance().record("compile_failure", fields);

    LiveEventJournal::Fields note;
    note.actorId = "compile_failure";
    note.error = error;
    LiveEventJournal::instance().record("notification_emitted", note);
    LiveEventJournal::Fields sound;
    sound.result = "live/failure";
    LiveEventJournal::instance().record("sound_emitted", sound);

    Debug::log(Debug::Category::General,
               "[LIVE CODE] compile failed file=%s error=%s\n", file.c_str(), error.c_str());
}

void notifyCandidateReady(std::uint32_t generation, const std::string& codeHash)
{
    NotificationSystem::instance().pushImportant(
        "LIVE CODE", "candidate ready: generation " + std::to_string(generation), 180);
    LiveEventJournal::Fields fields;
    fields.generation = generation;
    fields.hasGeneration = true;
    fields.codeHash = codeHash;
    fields.result = "ready";
    LiveEventJournal::instance().record("validation_result", fields);
}

void notifyActivated(std::uint32_t generation, const std::string& codeHash)
{
    const std::string message = "new version active: generation " + std::to_string(generation);
    NotificationSystem::instance().pushImportant("LIVE CODE", message, 240);
    playSound("live/success", 0.8f);

    LiveEventJournal::Fields fields;
    fields.generation = generation;
    fields.hasGeneration = true;
    fields.codeHash = codeHash;
    fields.result = "active";
    LiveEventJournal::instance().record("code_activation", fields);
    LiveEventJournal::Fields note;
    note.actorId = "code_activation";
    note.result = message;
    LiveEventJournal::instance().record("notification_emitted", note);
    LiveEventJournal::Fields sound;
    sound.result = "live/success";
    LiveEventJournal::instance().record("sound_emitted", sound);

    Debug::log(Debug::Category::General,
               "[LIVE CODE] activated generation=%u hash=%s\n",
               generation, codeHash.c_str());
}

void notifyRollbackActivated(std::uint32_t generation, const std::string& codeHash)
{
    NotificationSystem::instance().pushImportant(
        "LIVE CODE", "rollback activated: generation " + std::to_string(generation), 240);
    playSound("live/success", 0.8f);

    LiveEventJournal::Fields fields;
    fields.generation = generation;
    fields.hasGeneration = true;
    fields.codeHash = codeHash;
    fields.result = "rollback";
    LiveEventJournal::instance().record("rollback", fields);
    LiveEventJournal::Fields sound;
    sound.result = "live/success";
    LiveEventJournal::instance().record("sound_emitted", sound);
}

void notifyValidationFailed(std::uint32_t generation, const std::string& error)
{
    NotificationSystem::instance().pushCritical(
        "LIVE CODE", "candidate rejected, previous version retained\n" + error, 0);
    playSound("live/failure", 0.7f);
    LiveEventJournal::Fields fields;
    fields.generation = generation;
    fields.hasGeneration = true;
    fields.result = "failed";
    fields.error = error;
    LiveEventJournal::instance().record("validation_result", fields);
    LiveEventJournal::Fields sound;
    sound.result = "live/failure";
    LiveEventJournal::instance().record("sound_emitted", sound);
}

void notifyBoundaryViolation(const std::string& file)
{
    NotificationSystem::instance().pushCritical(
        "LIVE CODE",
        "HOT_RELOAD_BOUNDARY_VIOLATION: " + file +
            "\ncold kernel change cannot activate without relinking; move the behavior behind the hot ABI",
        0);
    playSound("live/failure", 0.7f);
}

} // namespace LiveCodeEvents
