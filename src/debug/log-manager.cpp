#include "log-manager.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fcntl.h>
#ifdef _WIND32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

static std::string timestamp()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    #ifdef _WIN32
        localtime_s(&tm, &t);
    #else
        localtime_r(&t,&tm);
    #endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buf);
}

static std::string dateDirName()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    #ifdef _WIN32
        localtime_s(&tm, &t);
    #else
        localtime_r(&t,&tm);
    #endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%d-%m-%Y", &tm);
    return std::string(buf);
}

static std::string timeFileName()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    #ifdef _WIN32
        localtime_s(&tm, &t);
    #else
        localtime_r(&t,&tm);
    #endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H-%M-%S", &tm);
    return std::string(buf) + "-log.txt";
}

LogManager& LogManager::instance()
{
    static LogManager mgr;
    return mgr;
}

bool LogManager::createDirectories()
{
    std::error_code ec;
    fs::create_directories("logs/" + dateDirName(), ec);
    return !ec;
}

bool LogManager::openFile()
{
    mPath = "logs/" + dateDirName() + "/" + timeFileName();
    mFile = fopen(mPath.c_str(), "w");
    if (!mFile) {
        printf("[LOGMANAGER] Failed to open log: %s\n", mPath.c_str());
        return false;
    }
    setvbuf(mFile, nullptr, _IOLBF, 1024);
    return true;
}

void LogManager::rotateLogs()
{
    const int MAX_LOGS = 30;
    std::vector<fs::path> logFiles;

    std::error_code ec;
    if (!fs::exists("logs", ec)) return;

    for (auto& entry : fs::recursive_directory_iterator("logs", ec)) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        if (name.size() > 8 && name.substr(name.size() - 8) == "-log.txt")
            logFiles.push_back(entry.path());
    }

    if ((int)logFiles.size() <= MAX_LOGS) return;

    std::sort(logFiles.begin(), logFiles.end(), [](const fs::path& a, const fs::path& b) {
        return fs::last_write_time(a) < fs::last_write_time(b);
    });

    int toDelete = (int)logFiles.size() - MAX_LOGS;
    mRotationDeleted = 0;
    for (int i = 0; i < toDelete; ++i) {
        std::error_code ec2;
        fs::remove(logFiles[i], ec2);
        if (!ec2) {
            printf("[LOG ROTATION] Deleted old log: %s\n", logFiles[i].string().c_str());
            if (mFile)
                fprintf(mFile, "[LOG ROTATION] Deleted old log: %s\n",
                    logFiles[i].string().c_str());
            mRotationDeleted++;
        }
    }
}

void LogManager::writeHeader()
{
    if (!mFile) return;
    fprintf(mFile,
        "==================================================\n"
        "MIMITA RUN LOG\n"
        "Start Time: %s\n"
        "Build: debug\n"
        "==================================================\n",
        timestamp().c_str());

    if (mRotationDeleted > 0) {
        fprintf(mFile, "\n[LOG ROTATION] Removed %d old logs\n", mRotationDeleted);
    }
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
        if (name.size() > 8 && name.substr(name.size() - 8) == "-log.txt")
            count++;
    }
    return count;
}

static void captureThreadFunc(int readFd, LogManager* mgr, std::atomic<bool>& running)
{
    char buf[4096];
    int consoleFd = mgr->savedStdoutFd();
    while (running) {
        int n = (int)read(readFd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            mgr->write(buf, n);
            mgr->flush();
            if (consoleFd >= 0)
                write(consoleFd, buf, n);
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

    rotateLogs();

    if (!openFile())
        return false;

    writeHeader();
    flush();

    // Capture stdout via pipe
    // All printf/Debug::log output goes through stdout; the pipe reader
    // thread writes everything to the log file AND to the original console.
    int pipeFds[2];
    if (pipe(pipeFds) == 0) {
        mPipeRead = pipeFds[0];
        int pipeWrite = pipeFds[1];

        mSavedStdout = dup(fileno(stdout));
        if (mSavedStdout >= 0) {
            dup2(pipeWrite, fileno(stdout));
            close(pipeWrite);

            // Unbuffered stdout so every printf appears in the pipe immediately
            setvbuf(stdout, nullptr, _IONBF, 0);

            mRunning = true;
            mCaptureThread = std::thread(captureThreadFunc, mPipeRead, this, std::ref(mRunning));
        } else {
            close(pipeWrite);
            close(mPipeRead);
            mPipeRead = -1;
        }
    }

    printf("[LOGMANAGER] Logging to: %s\n", mPath.c_str());
    return true;
}

void LogManager::shutdown()
{
    mRunning = false;

    // Restore original stdout first — this closes stdout's copy of the
    // pipe write end, which signals EOF to the reader thread.
    if (mSavedStdout >= 0) {
        fflush(stdout);
        dup2(mSavedStdout, fileno(stdout));
        close(mSavedStdout);
        mSavedStdout = -1;
    }

    // Close the read end to unblock the reader thread
    if (mPipeRead >= 0) {
        close(mPipeRead);
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
}
