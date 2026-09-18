// 09 17 2026
/* purpose
* Compatibility shim around the single authoritative JSONL event stream.
* The legacy run .txt file is gone. This class now only captures raw stdout
* (printf and other direct writes) and forwards each line into events.jsonl as a
* LEGACY event, while optionally mirroring to the real console.
* Does NOT own the events.jsonl file; StructuredLogger does.
*/

#pragma once

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>

class LogManager {
public:
    static LogManager& instance();

    void setLogType(const char* type) { mLogType = type ? type : "Game"; }
    bool init();
    void shutdown();

    // Legacy sinks: no separate file. Writes route through the stdout path.
    void write(const char* text, int len);
    void write(const char* text);
    void writeConsole(const char* text, int len);

    void flush();

    // Returns the authoritative events.jsonl path for this run, so callers that
    // used per-type .txt paths now point at the live stream.
    static std::string managedFilePath(const std::string& logType);

    std::string path() const;
    int fileCount() const { return 0; }
    int savedStdoutFd() const { return mSavedStdout; }

    // Emit one captured legacy line into events.jsonl. Public so the capture
    // thread and any direct writer share one path.
    void emitLegacyLine(const std::string& line);

private:
    LogManager() = default;
    ~LogManager() { shutdown(); }

    bool createDirectories();
    void cleanupOldFormat();

    std::string mLogType{"Game"};

    // stdout capture
    int mSavedStdout = -1;
    int mPipeRead = -1;
    std::thread mCaptureThread;
    std::atomic<bool> mRunning{false};
};
