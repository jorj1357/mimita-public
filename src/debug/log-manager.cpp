// 09 17 2026
/* purpose
* Implements the stdout->events.jsonl legacy bridge and the console mirror.
* Raw printf output becomes one LEGACY JSONL record per line; there is no
* separate .txt run log and no per-category file.
* Does NOT own events.jsonl (StructuredLogger does) and does not format events.
*/

#include "log-manager.h"
#include "structured-log.h"

#include <cstring>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <vector>

LogManager& LogManager::instance()
{
    static LogManager mgr;
    return mgr;
}

std::string LogManager::managedFilePath(const std::string& /*logType*/)
{
    // Every managed log type now shares the one authoritative event stream.
    return debug::eventsPath();
}

std::string LogManager::path() const
{
    return debug::eventsPath();
}

bool LogManager::createDirectories()
{
    // Directory creation is owned by StructuredLogger::init(); nothing to do.
    return true;
}

void LogManager::cleanupOldFormat()
{
    // Old-format directory cleanup is no longer performed here; the JSONL run
    // folders are self-describing and bounded.
}

void LogManager::write(const char* text, int len)
{
    if (!text || len <= 0)
        return;
    // Route raw byte output through the same one-record-per-line bridge so it
    // lands in events.jsonl.
    std::string chunk(text, (size_t)len);
    size_t start = 0;
    while (start <= chunk.size()) {
        size_t nl = chunk.find('\n', start);
        const std::string line = (nl == std::string::npos)
            ? chunk.substr(start)
            : chunk.substr(start, nl - start);
        if (!line.empty())
            emitLegacyLine(line);
        if (nl == std::string::npos)
            break;
        start = nl + 1;
    }
}

void LogManager::write(const char* text)
{
    if (!text)
        return;
    write(text, (int)std::strlen(text));
}

void LogManager::writeConsole(const char* text, int len)
{
    if (!text || len <= 0) return;
    if (mSavedStdout >= 0)
        _write(mSavedStdout, text, len);
    else
        fwrite(text, 1, (size_t)len, stdout);
}

void LogManager::flush()
{
    debug::flushEvents();
}

void LogManager::emitLegacyLine(const std::string& line)
{
    // One clean record per message: raw output that did not already go through
    // a structured call becomes a single LEGACY event.
    debug::Event ev;
    ev.category = "LEGACY";
    ev.name = "legacy.stdout";
    ev.level = debug::Level::Debug;
    ev.message = line;
    ev.sourceFile = "log-manager.cpp";
    ev.functionName = "emitLegacyLine";
    ev.aggregationKey = "LEGACY:legacy.stdout";
    debug::logEvent(ev);
}

static void captureThreadFunc(int readFd, LogManager* mgr, std::atomic<bool>& running)
{
    char buf[4096];
    int consoleFd = mgr->savedStdoutFd();
    std::string pending;
    while (running) {
        int n = (int)_read(readFd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            pending += buf;
            size_t start = 0;
            for (;;) {
                size_t nl = pending.find('\n', start);
                if (nl == std::string::npos)
                    break;
                std::string line = pending.substr(start, nl - start);
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (!line.empty())
                    mgr->emitLegacyLine(line);
                start = nl + 1;
            }
            pending.erase(0, start);
            if (consoleFd >= 0)
                _write(consoleFd, buf, n);
        } else {
            break;
        }
    }
    if (!pending.empty())
        mgr->emitLegacyLine(pending);
}

bool LogManager::init()
{
    if (mRunning.load())
        return true;

    // Capture stdout via a pipe so raw printf output lands in events.jsonl.
    // StructuredLogger already owns the file and flushes.
    int pipeFds[2];
    if (_pipe(pipeFds, 65536, _O_BINARY) == 0) {
        mPipeRead = pipeFds[0];
        int pipeWrite = pipeFds[1];

        mSavedStdout = _dup(_fileno(stdout));
        if (mSavedStdout >= 0) {
            _dup2(pipeWrite, _fileno(stdout));
            _close(pipeWrite);

            setvbuf(stdout, nullptr, _IONBF, 0);

            mRunning = true;
            mCaptureThread = std::thread(captureThreadFunc, mPipeRead, this, std::ref(mRunning));
        } else {
            _close(pipeWrite);
            _close(mPipeRead);
            mPipeRead = -1;
        }
    }

    return true;
}

void LogManager::shutdown()
{
    mRunning = false;

    // Restore original stdout first so the pipe write end closes (EOF).
    if (mSavedStdout >= 0) {
        fflush(stdout);
        _dup2(mSavedStdout, _fileno(stdout));
        _close(mSavedStdout);
        mSavedStdout = -1;
    }

    if (mPipeRead >= 0) {
        _close(mPipeRead);
        mPipeRead = -1;
    }

    if (mCaptureThread.joinable())
        mCaptureThread.join();
}
