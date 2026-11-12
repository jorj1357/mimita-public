// 2026-11-12
// Purpose: game-DLL-side StructuredLogger bridge for CAP_LOG.
//
// The resident EXE owns the real logger and events.jsonl sink. This bridge
// defines the StructuredLogger symbols inside the game generation and forwards
// every record to HostServicesV1::get_capability(HostCapabilityId::Log).
// It must never open a second journal file or own logging state.

#include <cstdarg>
#include <cstdio>
#include <string>

#include "debug/structured-log.h"
#include "host/abi/game-host-abi.h"
#include "host/abi/log-capability.h"

namespace mimita::game {

void initializeHostLog(const mimita::host::HostServicesV1* services);

namespace log_bridge {
const mimita::host::LogCapabilityV1* gLog = nullptr;
void* gHostUserData = nullptr;
char gFormatBuffer[mimita::host::kMaxLogPayloadBytes];

void emitEvent(std::int32_t category, std::int32_t level, const char* eventName,
               const char* correlationId, const char* reason, std::uint32_t tick,
               const nlohmann::json& fields, const char* sourceFile,
               std::int32_t sourceLine, const char* functionName) {
    if (!gLog || !gLog->event) {
        return;
    }
    std::string fieldsJson = fields.dump();
    if (fieldsJson.size() > mimita::host::kMaxLogPayloadBytes) {
        fieldsJson.resize(mimita::host::kMaxLogPayloadBytes);
    }
    gLog->event(gHostUserData, category, level, eventName ? eventName : "",
                correlationId ? correlationId : "", reason ? reason : "", tick,
                fieldsJson.c_str(), sourceFile, sourceLine, functionName);
}
}  // namespace log_bridge

void initializeHostLog(const mimita::host::HostServicesV1* services) {
    mimita::game::log_bridge::gLog = nullptr;
    mimita::game::log_bridge::gHostUserData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) {
        return;
    }
    auto* capability = static_cast<const mimita::host::LogCapabilityV1*>(
        services->get_capability(services->hostUserData,
                                 static_cast<std::uint32_t>(
                                     mimita::host::HostCapabilityId::Log),
                                 mimita::host::kLogCapabilityVersion));
    if (capability && capability->version == mimita::host::kLogCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::LogCapabilityV1)) {
        mimita::game::log_bridge::gLog = capability;
    }
}

}  // namespace mimita::game

StructuredLogger& StructuredLogger::instance() {
    static StructuredLogger logger;
    return logger;
}

StructuredLogger::~StructuredLogger() = default;

void StructuredLogger::init() {}

void StructuredLogger::shutdown() {}

void StructuredLogger::pollConfig() {}

void StructuredLogger::write(const Entry& entry) {
    nlohmann::json fields = nlohmann::json::object();
    if (!entry.message.empty()) {
        fields["message"] = entry.message;
    }
    for (std::size_t index = 0; index < entry.numericKeys.size(); ++index) {
        nlohmann::json numeric = nlohmann::json::object();
        if (index < entry.numericExpected.size()) {
            numeric["expected"] = entry.numericExpected[index];
        }
        if (index < entry.numericActual.size()) {
            numeric["actual"] = entry.numericActual[index];
        }
        fields[entry.numericKeys[index]] = numeric;
    }
    if (entry.tolerance != 0.0) {
        fields["tolerance"] = entry.tolerance;
    }
    mimita::game::log_bridge::emitEvent(static_cast<std::int32_t>(entry.category),
                            static_cast<std::int32_t>(entry.level),
                            entry.eventId.c_str(), entry.correlationId.c_str(),
                            entry.reason.c_str(), entry.tick, fields,
                            entry.sourceFile.empty() ? nullptr : entry.sourceFile.c_str(),
                            entry.sourceLine,
                            entry.functionName.empty() ? nullptr : entry.functionName.c_str());
}

void StructuredLogger::writeEvent(StructuredCategory category, StructuredLevel level,
                                  const std::string& eventName,
                                  const std::string& correlationId,
                                  const std::string& reason, uint32_t tick,
                                  const nlohmann::json& fields, const char* sourceFile,
                                  int sourceLine, const char* functionName) {
    mimita::game::log_bridge::emitEvent(static_cast<std::int32_t>(category),
                            static_cast<std::int32_t>(level), eventName.c_str(),
                            correlationId.c_str(), reason.c_str(), tick, fields,
                            sourceFile, sourceLine, functionName);
}

bool StructuredLogger::writeEventChecked(StructuredCategory category, StructuredLevel level,
                                         const std::string& eventName,
                                         const std::string& correlationId,
                                         const std::string& reason, uint32_t tick,
                                         const nlohmann::json& fields,
                                         const char* sourceFile, int sourceLine,
                                         const char* functionName) {
    writeEvent(category, level, eventName, correlationId, reason, tick, fields,
               sourceFile, sourceLine, functionName);
    return true;
}

bool StructuredLogger::pinCurrentRun(const std::string&, uint32_t) {
    return true;
}

bool StructuredLogger::unpinCurrentRun() {
    return true;
}

bool StructuredLogger::tryWriteEvent(StructuredCategory category, StructuredLevel level,
                                     const std::string& eventName,
                                     const std::string& correlationId,
                                     const std::string& reason, uint32_t tick,
                                     const nlohmann::json& fields, const char* sourceFile,
                                     int sourceLine, const char* functionName) {
    writeEvent(category, level, eventName, correlationId, reason, tick, fields,
               sourceFile, sourceLine, functionName);
    return true;
}

void StructuredLogger::writeFormatted(StructuredCategory category, StructuredLevel level,
                                      const char* sourceFile, int sourceLine,
                                      const char* functionName, const char* format, ...) {
    va_list args;
    va_start(args, format);
    writeVFormatted(category, level, sourceFile, sourceLine, functionName, format, args);
    va_end(args);
}

void StructuredLogger::writeVFormatted(StructuredCategory category, StructuredLevel level,
                                       const char* sourceFile, int sourceLine,
                                       const char* functionName, const char* format,
                                       va_list args) {
    if (!format) {
        return;
    }
    vsnprintf(mimita::game::log_bridge::gFormatBuffer, sizeof(mimita::game::log_bridge::gFormatBuffer), format, args);
    mimita::game::log_bridge::emitEvent(
        static_cast<std::int32_t>(category), static_cast<std::int32_t>(level),
        "log.formatted", "", mimita::game::log_bridge::gFormatBuffer, 0,
        nlohmann::json{{"message", mimita::game::log_bridge::gFormatBuffer}}, sourceFile,
        sourceLine, functionName);
}

void StructuredLogger::flushEvents() {}

void StructuredLogger::tick() {}

void StructuredLogger::assertNear(const std::string& eventId,
                                  const std::string& correlationId,
                                  const std::string& reason, StructuredCategory category,
                                  const std::string& sourceFile, int sourceLine,
                                  const std::string& functionName, const std::string& key,
                                  double expected, double actual, double tolerance,
                                  uint32_t tick, uint32_t) {
    nlohmann::json fields = nlohmann::json{
        {"key", key}, {"expected", expected}, {"actual", actual},
        {"tolerance", tolerance}};
    mimita::game::log_bridge::emitEvent(
        static_cast<std::int32_t>(category),
        static_cast<std::int32_t>(StructuredLevel::Important), eventId.c_str(),
        correlationId.c_str(), reason.c_str(), tick, fields,
        sourceFile.empty() ? nullptr : sourceFile.c_str(), sourceLine,
        functionName.empty() ? nullptr : functionName.c_str());
}

bool StructuredLogger::shouldLog(StructuredCategory category, StructuredLevel level) const {
    if (!mimita::game::log_bridge::gLog || !mimita::game::log_bridge::gLog->should_log) {
        return false;
    }
    return mimita::game::log_bridge::gLog->should_log(
               mimita::game::log_bridge::gHostUserData, static_cast<std::int32_t>(category),
               static_cast<std::int32_t>(level)) != 0;
}

void StructuredLogger::beginScenarioCapture() {}

void StructuredLogger::endScenarioCapture() {}

bool StructuredLogger::scenarioCaptureActive() const {
    return false;
}

uint64_t StructuredLogger::eventsBytes() const {
    return 0;
}

StructuredLogger::IoStats StructuredLogger::ioStats(bool) {
    return IoStats{};
}
