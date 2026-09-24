// 09 12 2026
/* purpose
* Implements the append-only JSONL live event journal.
* Formats one JSON object per line with UTC millisecond time and monotonic ms,
* writes under logs/features/live-code/yyyy-mm-dd/, and flushes every line.
* Does NOT start compiles, choose candidates, or own notifications.
*/
#include "live-code/live-journal.h"
#include "live-code/live-identity.h"
#include "debug/structured-log.h"
#include "utils/time-format.h"

#include <cstdio>
#include <filesystem>

namespace {

std::string escapeJson(const std::string& value)
{
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8]{};
                std::snprintf(buf, sizeof(buf), "\\u%04x", c & 0xff);
                out += buf;
            } else {
                out += c;
            }
            break;
        }
    }
    return out;
}

void appendString(std::string& out, bool& first, const char* key, const std::string& value)
{
    if (value.empty())
        return;
    if (!first)
        out += ",";
    first = false;
    out += '"';
    out += key;
    out += "\":\"";
    out += escapeJson(value);
    out += '"';
}

}

LiveEventJournal& LiveEventJournal::instance()
{
    static LiveEventJournal journal;
    return journal;
}

void LiveEventJournal::init()
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (mActive)
        return;

    // Compatibility facade: live-code diagnostics share the authoritative
    // StructuredLogger stream. This preserves existing call sites while
    // eliminating the second logs/features/live-code file family.
    mPath = debug::eventsPath();
    if (mPath.empty()) {
        mActive = false;
        return;
    }
    mActive = true;
}

void LiveEventJournal::shutdown()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mActive = false;
}

bool LiveEventJournal::active() const
{
    return mActive;
}

void LiveEventJournal::record(const char* type)
{
    record(type, Fields());
}

void LiveEventJournal::record(const char* type, const Fields& fields)
{
    if (!type || !*type)
        return;

    std::lock_guard<std::mutex> lock(mMutex);
    if (!mActive)
        return;

    debug::Event event;
    event.category = "NETWORK";
    event.name = type;
    event.level = debug::Level::Info;
    event.simulationTick = fields.tick;
    event.clientTick = fields.clientTick;
    event.message = fields.error.empty() ? fields.result : fields.error;
    event.fields["live_code_event"] = true;
    if (LiveIdentity::sessionId() != 0) event.fields["session_id"] = LiveIdentity::sessionId();
    if (fields.hasGeneration) event.fields["generation"] = fields.generation;
    if (!fields.codeHash.empty()) event.fields["code_hash"] = fields.codeHash;
    if (!fields.file.empty()) event.fields["file"] = fields.file;
    if (!fields.module.empty()) event.fields["module"] = fields.module;
    if (!fields.actorId.empty()) event.fields["actor_id"] = fields.actorId;
    if (!fields.projectileId.empty()) event.fields["projectile_id"] = fields.projectileId;
    if (!fields.packetId.empty()) event.fields["packet_id"] = fields.packetId;
    if (!fields.result.empty()) event.fields["result"] = fields.result;
    if (!fields.error.empty()) event.fields["error"] = fields.error;
    if (fields.connectionId != 0) event.fields["connection_id"] = fields.connectionId;
    if (fields.requestId != 0) event.fields["request_id"] = fields.requestId;
    if (fields.entityId != 0) event.fields["entity_id"] = fields.entityId;
    if (fields.serverGeneration != 0) event.fields["server_generation"] = fields.serverGeneration;
    if (fields.hotGeneration != 0) event.fields["hot_generation"] = fields.hotGeneration;
    if (!fields.serverHash.empty()) event.fields["server_hash"] = fields.serverHash;
    if (!fields.hotHash.empty()) event.fields["hot_hash"] = fields.hotHash;
    debug::logEvent(event);
}
