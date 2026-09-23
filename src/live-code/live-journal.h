// 09 12 2026
/* purpose
* Own the append-only JSONL live engineering event journal.
* Writes one UTC millisecond timestamped event per line and flushes immediately
* so a crash preserves the forensic record.
* Owns the journal file path, thread safety, and JSON line formatting.
* Does NOT decide gameplay, start compiles, or own notifications.
* Does NOT replace the debug logger; it is the detailed live-code record.
*/
#pragma once

#include <cstdint>
#include <mutex>
#include <string>

class LiveEventJournal {
public:
    static LiveEventJournal& instance();

    // Optional fields attached to a journal event. Empty fields are omitted.
    struct Fields {
        std::uint64_t tick = 0;
        std::uint32_t generation = 0;
        bool hasGeneration = false;
        std::string codeHash;
        std::string file;
        std::string module;
        std::string actorId;
        std::string projectileId;
        std::string packetId;
        std::string result;
        std::string error;
        // Raw JSON object fragment without the outer braces, e.g.
        // "\"bytes\":128,\"status\":\"ok\"". Advanced callers only.
        std::string extra;

        // Append-only server correlation fields (proposal §8). Numeric ids are
        // omitted when zero, so a client line stays compact. `clientTick` and
        // `hotGeneration`/`hotHash`/`serverHash` correlate a server decision
        // with the exact generations that produced it.
        std::uint64_t clientTick = 0;
        std::uint32_t connectionId = 0;
        std::uint32_t requestId = 0;
        std::uint64_t entityId = 0;
        std::uint32_t serverGeneration = 0;
        std::uint32_t hotGeneration = 0;
        std::string serverHash;
        std::string hotHash;
    };

    // Resolve the daily path and open the append stream. Safe to call twice.
    void init();
    void shutdown();
    bool active() const;

    void record(const char* type);
    void record(const char* type, const Fields& fields);

    const std::string& path() const { return mPath; }

private:
    LiveEventJournal() = default;
    LiveEventJournal(const LiveEventJournal&) = delete;
    LiveEventJournal& operator=(const LiveEventJournal&) = delete;

    std::mutex mMutex;
    std::string mPath;
    bool mActive = false;
};
