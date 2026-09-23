// 09 12 2026
/* purpose
* Implements live-code lifecycle notifications, sounds, and journal lines.
* Every message identifies the process side, PID, session, generation, hash,
* file, and result so client and server activations can never be confused.
* Does NOT compile, load, or validate modules; callers own those transitions.
*/
#include "live-code/live-code-events.h"

#include "audio/audio.h"
#include "debug/debug-log.h"
#include "live-code/live-identity.h"
#include "live-code/live-journal.h"
#include "live-code/live-behavior.h"
#include "notifications/notifications.h"
#include "utils/time-format.h"

#include <cctype>
#include <string>

namespace {

std::string upperProcess()
{
    std::string value = LiveIdentity::process();
    for (char& c : value)
        c = (char)std::toupper((unsigned char)c);
    return value;
}

std::string identityBlock()
{
    std::string block = "side=" + upperProcess() +
                        "\npid=" + std::to_string(LiveIdentity::pid());
    if (LiveIdentity::sessionId() != 0)
        block += "\nsession=" + std::to_string(LiveIdentity::sessionId());
    return block;
}

std::string shortHash(const std::string& hash)
{
    return hash.size() > 12 ? hash.substr(0, 12) : hash;
}

} // namespace

namespace LiveCodeEvents {

void notifyEditDetected(const std::string& file)
{
    NotificationSystem::instance().push(
        "LIVE CODE", "edit detected\n" + identityBlock() + "\nfile=" + file, 150, {});
    LiveEventJournal::Fields fields;
    fields.file = file;
    LiveEventJournal::instance().record("edit_detected", fields);
    Debug::log(Debug::Category::General, "[LIVE CODE] edit detected file=%s\n", file.c_str());
}

void notifyCompiling(const std::string& file, std::uint32_t candidateGeneration, int attempt)
{
    std::string message = "compiling generation " + std::to_string(candidateGeneration);
    if (attempt > 1)
        message += " (retry " + std::to_string(attempt) + ")";
    NotificationSystem::instance().push(
        "LIVE CODE", message + "\n" + identityBlock() + "\nfile=" + file, 180, {});
    LiveEventJournal::Fields fields;
    fields.file = file;
    fields.generation = candidateGeneration;
    fields.hasGeneration = true;
    fields.result = attempt > 1 ? "retry" : "started";
    LiveEventJournal::instance().record("compile_started", fields);
}

void notifyCompileFailed(const std::string& file, std::uint32_t candidateGeneration,
                         std::uint32_t activeGeneration, const std::string& error,
                         int attempt)
{
    std::string message =
        "LIVE CODE RETAINED OLD VERSION\n" + identityBlock() +
        "\ncandidateGeneration=" + std::to_string(candidateGeneration) +
        "\nactiveGeneration=" + std::to_string(activeGeneration) +
        "\nfile=" + file +
        "\nattempt=" + std::to_string(attempt) +
        "\nerror=" + error;
    NotificationSystem::instance().pushCritical("LIVE CODE", message, 0);
    if (!LiveBehavior::emitAudioFact("live.failure", "live/failure",
                                     static_cast<const float*>(nullptr), 0, false,
                                     1.0f, 1.0f, 0.7f, 1.0f))
        playSound("live/failure", 0.7f);

    LiveEventJournal::Fields fields;
    fields.file = file;
    fields.generation = candidateGeneration;
    fields.hasGeneration = true;
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
               "[LIVE CODE] compile failed gen=%u active=%u attempt=%d file=%s error=%s\n",
               candidateGeneration, activeGeneration, attempt, file.c_str(), error.c_str());
}

void notifyCandidateReady(std::uint32_t generation, const std::string& codeHash)
{
    NotificationSystem::instance().pushImportant(
        "LIVE CODE",
        "candidate ready: generation " + std::to_string(generation) + "\n" + identityBlock(),
        180);
    LiveEventJournal::Fields fields;
    fields.generation = generation;
    fields.hasGeneration = true;
    fields.codeHash = codeHash;
    fields.result = "ready";
    LiveEventJournal::instance().record("validation_result", fields);
}

void notifyActivated(std::uint32_t generation, const std::string& codeHash)
{
    std::string message = "LIVE CODE ACTIVE\n" + identityBlock() +
        "\ngeneration=" + std::to_string(generation) +
        "\nhash=" + shortHash(codeHash) +
        "\nactivatedUtc=" + MiMitaTime::utcIso8601Millis();
    if (LiveIdentity::simulationTick() != 0)
        message += "\ntick=" + std::to_string(LiveIdentity::simulationTick());
    NotificationSystem::instance().pushImportant("LIVE CODE", message, 240);
    if (!LiveBehavior::emitAudioFact("live.success", "live/success",
                                     static_cast<const float*>(nullptr), 0, false,
                                     1.0f, 1.0f, 0.8f, 1.0f))
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
               "[LIVE CODE] activated side=%s pid=%d generation=%u hash=%s\n",
               LiveIdentity::process(), LiveIdentity::pid(), generation, codeHash.c_str());
}

void notifyGenerationMismatch(std::uint32_t localGeneration,
                              std::uint32_t remoteGeneration,
                              bool remoteKnown,
                              std::uint32_t remotePhase,
                              const std::string& localHash,
                              std::uint64_t remoteHash)
{
    std::string message = "CLIENT ONLY\n" + identityBlock() +
        "\nclientGeneration=" + std::to_string(localGeneration);
    if (remoteKnown) {
        const char* phase = remotePhase == 3 ? "active" :
            (remotePhase == 2 ? "switch-pending" :
             (remotePhase == 1 ? "ready-candidate" : "candidate/status"));
        message += "\nSERVER GENERATION " + std::to_string(remoteGeneration) +
                   " phase=" + phase;
    }
    else
        message += "\ndedicated server generation independent";
    NotificationSystem::instance().pushImportant("LIVE CODE", message, 300);
    LiveEventJournal::Fields fields;
    fields.generation = localGeneration;
    fields.hasGeneration = true;
    fields.result = "client_only";
    fields.codeHash = localHash;
    fields.extra = std::string("\"remote_generation\":") +
        std::to_string(remoteGeneration) +
        ",\"remote_known\":" + (remoteKnown ? std::string("true") : std::string("false")) +
        ",\"remote_phase\":" + std::to_string(remotePhase) +
        ",\"remote_hash\":" + std::to_string(remoteHash);
    LiveEventJournal::instance().record("generation_mismatch", fields);
}

void notifyGenerationConverged(std::uint32_t generation,
                               const std::string& codeHash)
{
    NotificationSystem::instance().pushImportant(
        "LIVE CODE", "GENERATION SYNCED\n" + identityBlock() +
            "\ngeneration=" + std::to_string(generation), 180);
    LiveEventJournal::Fields fields;
    fields.generation = generation;
    fields.hasGeneration = true;
    fields.codeHash = codeHash;
    fields.result = "converged";
    LiveEventJournal::instance().record("generation_converged", fields);
}

void notifyRollbackActivated(std::uint32_t generation, const std::string& codeHash)
{
    NotificationSystem::instance().pushImportant(
        "LIVE CODE",
        "rollback activated\n" + identityBlock() +
            "\ngeneration=" + std::to_string(generation),
        240);
    if (!LiveBehavior::emitAudioFact("live.success", "live/success",
                                     static_cast<const float*>(nullptr), 0, false,
                                     1.0f, 1.0f, 0.8f, 1.0f))
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
        "LIVE CODE",
        "LIVE CODE RETAINED OLD VERSION\n" + identityBlock() +
            "\ncandidateGeneration=" + std::to_string(generation) +
            "\nerror=" + error,
        0);
    if (!LiveBehavior::emitAudioFact("live.failure", "live/failure",
                                     static_cast<const float*>(nullptr), 0, false,
                                     1.0f, 1.0f, 0.7f, 1.0f))
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
        "COLD RESTART PENDING\nseverity=critical\n" + identityBlock() +
            "\nfile=" + file +
            "\nreason=EXE-owned kernel change\nMiMITA.exe was not restarted",
        0);
    if (!LiveBehavior::emitAudioFact("live.failure", "live/failure",
                                     static_cast<const float*>(nullptr), 0, false,
                                     1.0f, 1.0f, 0.7f, 1.0f))
        playSound("live/failure", 0.7f);
}

void notifyColdRestartPending(const std::string& file)
{
    NotificationSystem::instance().pushImportant(
        "LIVE CODE",
        "COLD RESTART PENDING\nseverity=critical\n" + identityBlock() +
            "\nfile=" + file +
            "\nreason=EXE-owned kernel change\nMiMITA.exe was not restarted",
        600);
    LiveEventJournal::Fields fields;
    fields.file = file;
    fields.result = "pending";
    LiveEventJournal::instance().record("cold_restart_pending", fields);
}

} // namespace LiveCodeEvents
