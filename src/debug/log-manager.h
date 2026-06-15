#pragma once

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>

class LogManager {
public:
    static LogManager& instance();

    bool init();
    void shutdown();

    void write(const char* text, int len);
    void write(const char* text);

    void flush();

    std::string path() const { return mPath; }
    int fileCount() const;

private:
    LogManager() = default;
    ~LogManager() { shutdown(); }

    bool openFile();
    bool createDirectories();
    void writeHeader();
    void writeFooter();
    void rotateLogs();

    std::string mPath;
    FILE* mFile = nullptr;

    // stdout capture
    int mSavedStdout = -1;
    int mPipeRead = -1;
    int mPipeWrite = -1;
    std::thread mCaptureThread;
    std::atomic<bool> mRunning{false};

    int mRotationDeleted = 0;
};