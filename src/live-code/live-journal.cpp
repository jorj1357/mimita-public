// 09 12 2026
/* purpose
* Implements the append-only JSONL live event journal.
* Formats one JSON object per line with UTC millisecond time and monotonic ms,
* writes under logs/features/live-code/yyyy-mm-dd/, and flushes every line.
* Does NOT start compiles, choose candidates, or own notifications.
*/
#include "live-code/live-journal.h"
#include "utils/time-format.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

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

    const std::filesystem::path dir =
        std::filesystem::path("logs") / "features" / "live-code" / MiMitaTime::utcDateFolder();
    std::error_code error;
    std::filesystem::create_directories(dir, error);

    mPath = (dir / ("live_events_" + MiMitaTime::utcCompactStamp() + ".jsonl")).string();

    std::ofstream probe(mPath, std::ios::app);
    if (!probe.is_open()) {
        mActive = false;
        return;
    }
    probe.close();
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

    const std::string ts = MiMitaTime::utcIso8601Millis();
    const std::uint64_t mono = MiMitaTime::monotonicMillis();

    std::string line;
    line.reserve(256);
    line += '{';
    bool first = true;
    appendString(line, first, "ts_utc", ts);

    if (!first) line += ",";
    first = false;
    line += "\"mono_ms\":";
    line += std::to_string(mono);

    if (!first) line += ",";
    line += "\"type\":\"";
    line += escapeJson(type);
    line += '"';

    if (fields.tick != 0) {
        line += ",\"tick\":";
        line += std::to_string(fields.tick);
    }
    if (fields.hasGeneration) {
        line += ",\"generation\":";
        line += std::to_string(fields.generation);
    }
    appendString(line, first, "code_hash", fields.codeHash);
    appendString(line, first, "file", fields.file);
    appendString(line, first, "module", fields.module);
    appendString(line, first, "actor_id", fields.actorId);
    appendString(line, first, "projectile_id", fields.projectileId);
    appendString(line, first, "packet_id", fields.packetId);
    appendString(line, first, "result", fields.result);
    appendString(line, first, "error", fields.error);
    if (!fields.extra.empty())
    {
        line += ",";
        line += fields.extra;
    }
    line += '}';

    std::lock_guard<std::mutex> lock(mMutex);
    if (!mActive)
        return;

    std::ofstream out(mPath, std::ios::app);
    if (!out.is_open())
        return;
    out << line << '\n';
    out.flush();
}
