// 2026-11-13
// Transitional EXE-side implementation of game::runDetached.
//
// Until ownership cutover, ordinary game sources still run in the EXE and call
// this helper. The hot game DLL provides the resident-executor bridge instead.
// Keeping the fallback resident avoids adding a raw thread to game-owned code.

#include <thread>
#include <utility>

#include "game/job-bridge-module.h"

namespace mimita::game {
bool runDetached(std::function<void()> work, const char*) {
    if (!work) return false;
    std::thread(std::move(work)).detach();
    return true;
}

HostWorker::~HostWorker() {
    release();
}

bool HostWorker::start(std::function<void()> work, const char* tag) {
    if (handle_ || !work) return false;
    auto* thread = new std::thread(std::move(work));
    handle_ = reinterpret_cast<std::uint64_t>(thread);
    (void)tag;
    return true;
}

void HostWorker::wait() {
    if (!handle_) return;
    auto* thread = reinterpret_cast<std::thread*>(handle_);
    if (thread->joinable()) thread->join();
}

void HostWorker::release() {
    if (!handle_) return;
    auto* thread = reinterpret_cast<std::thread*>(handle_);
    if (thread->joinable()) thread->join();
    delete thread;
    handle_ = 0;
}
}  // namespace mimita::game
