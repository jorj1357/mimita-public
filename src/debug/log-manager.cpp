#include "log-manager.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fcntl.h>
#include <io.h>
#include <unistd.h>

namespace fs = std::filesystem;

static std::string timestamp()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &t);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buf);
}

static std::string dateDirName()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &t);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%m-%d-%Y", &tm);
    return std::string(buf);
}

static std::string timeFileName(const std::string& logType)
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &t);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H%M%S", &tm);
    return logType + "_log_" + std::string(buf) + ".txt";
}

LogManager& LogManager::instance()
{
    static LogManager mgr;
    return mgr;
}

std::string LogManager::managedFilePath(const std::string& logType)
{
    return "logs/" + dateDirName() + "/" + timeFileName(logType);
}

bool LogManager::createDirectories()
{
    std::error_code ec;
    fs::create_directories("logs/" + dateDirName(), ec);
    return !ec;
}

bool LogManager::openFile()
{
    mPath = "logs/" + dateDirName() + "/" + timeFileName(mLogType);
    mFile = fopen(mPath.c_str(), "w");
    if (!mFile) {
        printf("[LOGMANAGER] Failed to open log: %s\n", mPath.c_str());
        return false;
    }
    setvbuf(mFile, nullptr, _IOLBF, 1024);
    return true;
}

void LogManager::writeHeader()
{
    if (!mFile) return;
    fprintf(mFile,
        "==================================================\n"
        "MIMITA %s LOG\n"
        "Start Time: %s\n"
        "Build: debug\n"
        "==================================================\n",
        mLogType.c_str(), timestamp().c_str());

}

void LogManager::writeFooter()
{
    if (!mFile) return;
    fprintf(mFile,
        "\n==================================================\n"
        "END OF RUN\n"
        "Time: %s\n"
        "==================================================\n",
        timestamp().c_str());
}

void LogManager::write(const char* text, int len)
{
    if (mFile && len > 0)
        fwrite(text, 1, (size_t)len, mFile);
}

void LogManager::write(const char* text)
{
    if (mFile && text)
        fputs(text, mFile);
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
    if (mFile)
        fflush(mFile);
}

int LogManager::fileCount() const
{
    std::error_code ec;
    if (!fs::exists("logs", ec)) return 0;
    int count = 0;
    for (auto& entry : fs::recursive_directory_iterator("logs", ec)) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        if (name.find("_log_") != std::string::npos) {
            for (auto& ft : {"Server_", "Gameterminal_", "Client_", "Game_"}) {
                if (name.find(ft) == 0) { count++; break; }
            }
        }
    }
    return count;
}

static void captureThreadFunc(int readFd, LogManager* mgr, std::atomic<bool>& running)
{
    char buf[4096];
    int consoleFd = mgr->savedStdoutFd();
    while (running) {
        int n = (int)_read(readFd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            mgr->write(buf, n);
            mgr->flush();
            if (consoleFd >= 0)
                _write(consoleFd, buf, n);
        } else {
            break;
        }
    }
}

bool LogManager::init()
{
    if (mFile) return true;

    if (!createDirectories()) {
        printf("[LOGMANAGER] Failed to create log directories\n");
        return false;
    }

    if (!openFile())
        return false;

    writeHeader();
    flush();

    // Capture stdout via pipe
    // All printf/Debug::log output goes through stdout; the pipe reader
    // thread writes everything to the log file AND to the original console.
    int pipeFds[2];
    if (_pipe(pipeFds, 65536, _O_BINARY) == 0) {
        mPipeRead = pipeFds[0];
        int pipeWrite = pipeFds[1];

        mSavedStdout = _dup(_fileno(stdout));
        if (mSavedStdout >= 0) {
            _dup2(pipeWrite, _fileno(stdout));
            _close(pipeWrite);

            // Unbuffered stdout so every printf appears in the pipe immediately
            setvbuf(stdout, nullptr, _IONBF, 0);

            mRunning = true;
            mCaptureThread = std::thread(captureThreadFunc, mPipeRead, this, std::ref(mRunning));
        } else {
            _close(pipeWrite);
            _close(mPipeRead);
            mPipeRead = -1;
        }
    }

    printf("[LOGMANAGER] Logging to: %s\n", mPath.c_str());
    return true;
}

static void writeLatestLogPath(const std::string& path)
{
    std::error_code ec;
    fs::create_directories("logs", ec);
    FILE* f = fopen("logs/latest-log-path.txt", "w");
    if (f) {
        fprintf(f, "%s\n", path.c_str());
        fclose(f);
    }
}

void LogManager::shutdown()
{
    mRunning = false;

    // Restore original stdout first — this closes stdout's copy of the
    // pipe write end, which signals EOF to the reader thread.
    if (mSavedStdout >= 0) {
        fflush(stdout);
        _dup2(mSavedStdout, _fileno(stdout));
        _close(mSavedStdout);
        mSavedStdout = -1;
    }

    // Close the read end to unblock the reader thread
    if (mPipeRead >= 0) {
        _close(mPipeRead);
        mPipeRead = -1;
    }

    if (mCaptureThread.joinable())
        mCaptureThread.join();

    writeFooter();
    flush();

    if (mFile) {
        fclose(mFile);
        mFile = nullptr;
    }

    writeLatestLogPath(mPath);

    // Print log path clearly at exit
    char absPath[4096];
    if (_fullpath(absPath, mPath.c_str(), sizeof(absPath))) {
        printf("\n");
        printf("==================================================\n");
        printf("MIMITA RUN LOG SAVED\n");
        printf("%s\n", absPath);
        printf("Ctrl+Click the path above to open it.\n");
        printf("==================================================\n");
    }
}
