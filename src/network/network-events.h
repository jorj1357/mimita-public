// 2026-11-17
// Purpose: canonical semantic networking event emission from hot game code.
// The resident transport may emit raw host.* events; this helper emits the
// followable network.* chain with one common envelope.
// Does NOT own reconnect/health policy or packet schemas.
// Does NOT open a second journal or retain raw reconnect tokens.

#pragma once

#include <cstdint>
#include <cstring>
#include <string>

#include <nlohmann/json.hpp>

#include "debug/structured-log.h"

namespace MimitaNet {
namespace NetworkEvents {

struct Context
{
    std::uint32_t connectionId = 0;
    std::uint32_t attemptId = 0;
    std::uint32_t playerId = 0;
    const char* role = "client";
    const char* transport = "udp";
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
    if (!context.serverAddress.empty())
        fields["server_address"] = context.serverAddress;
    if (!context.roomCode.empty())
        fields["room_code"] = context.roomCode;

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


