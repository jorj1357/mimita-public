// 2026-11-17
// Purpose: canonical semantic networking event emission from hot game code.
// The resident transport may emit raw host.* events; this helper emits the
// followable network.* chain with one common envelope.
// Does NOT own reconnect/health policy or packet schemas.
// Does NOT open a second journal or retain raw reconnect tokens.

#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include <nlohmann/json.hpp>

#include "config/networking-config.h"
#include "debug/structured-log.h"

namespace MimitaNet {
namespace NetworkEvents {

struct Context
{
    std::uint32_t connectionId = 0;
    std::uint32_t attemptId = 0;
    std::uint32_t playerId = 0;
    std::uint64_t generation = 0;
    std::uint64_t oldGeneration = 0;
    std::uint64_t configRevision = 0;
    const char* role = "client";
    const char* transport = "udp";
    std::string localEndpoint;
    std::string serverAddress;
    std::string roomCode;
};

inline const char* transportName(const char* value)
{
    return value && value[0] ? value : "none";
}

inline bool isFailureResult(const char* result)
{
    if (!result)
        return false;
    return std::strcmp(result, "failed") == 0 ||
           std::strcmp(result, "rejected") == 0 ||
           std::strcmp(result, "terminal") == 0 ||
           std::strcmp(result, "expired") == 0;
}

inline bool allowEvent(const NetworkingConfigData& config)
{
    if (!config.observability.enabled)
        return false;
    const std::uint32_t limit = config.observability.maxEventsPerSecond;
    if (limit == 0)
        return false;

    using Clock = std::chrono::steady_clock;
    static thread_local Clock::time_point windowStart = Clock::now();
    static thread_local std::uint32_t emittedThisWindow = 0;
    const Clock::time_point now = Clock::now();
    if (now - windowStart >= std::chrono::seconds(1))
    {
        windowStart = now;
        emittedThisWindow = 0;
    }
    if (emittedThisWindow >= limit)
        return false;
    ++emittedThisWindow;
    return true;
}

inline void emit(const Context& context,
                 const char* eventName,
                 const char* stage,
                 const char* result,
                 const char* reasonCode,
                 const char* nextExpectedEvent,
                 std::uint32_t tick,
                 nlohmann::json fields,
                 const char* sourceFile,
                 int sourceLine,
                 const char* functionName)
{
    if (!fields.is_object())
        fields = nlohmann::json::object();

    const NetworkingConfigData& config = NetworkingConfig::instance().data();
    if (!allowEvent(config))
        return;

    const std::string correlation = context.attemptId > 0
        ? "reconnect:" + std::to_string(context.attemptId)
        : "conn:" + std::to_string(context.connectionId);

    fields["connection_id"] = context.connectionId;
    fields["attempt_id"] = context.attemptId;
    fields["local_player_id"] = context.playerId;
    fields["role"] = context.role ? context.role : "unknown";
    fields["transport"] = transportName(context.transport);
    fields["stage"] = stage ? stage : "";
    fields["result"] = result ? result : "";
    fields["reason_code"] = reasonCode ? reasonCode : "";
    fields["next_expected_event"] = nextExpectedEvent ? nextExpectedEvent : "";
    fields["config_revision"] = context.configRevision != 0
        ? context.configRevision
        : NetworkingConfig::instance().revision();
    if (context.generation != 0)
        fields["generation"] = context.generation;
    if (context.oldGeneration != 0)
        fields["old_generation"] = context.oldGeneration;
    if (!context.localEndpoint.empty())
        fields["local_endpoint"] = context.localEndpoint;
    if (!context.serverAddress.empty())
    {
        fields["server_address"] = context.serverAddress;
        fields["remote_endpoint"] = context.serverAddress;
    }
    if (!context.roomCode.empty())
        fields["room_code"] = context.roomCode;
    if (const char* buildId = std::getenv("MIMITA_BUILD_ID"))
    {
        if (buildId[0] != '\0')
            fields["build_id"] = buildId;
    }

    const bool failed = isFailureResult(result);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network,
        failed ? StructuredLevel::Errors : StructuredLevel::Important,
        eventName ? eventName : "network.event",
        correlation,
        reasonCode ? reasonCode : "network event",
        tick,
        fields,
        sourceFile ? sourceFile : __FILE__,
        sourceLine > 0 ? sourceLine : __LINE__,
        functionName ? functionName : __FUNCTION__);
}

} // namespace NetworkEvents
} // namespace MimitaNet



