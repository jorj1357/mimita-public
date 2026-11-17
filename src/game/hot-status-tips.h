// 2026-11-15
// Hot-DLL live feedback for generation builds and file changes.
// Reads generated build JSON through the resident JSON watcher and publishes
// notifications through the existing tip/notification card.
// Does NOT own build state, file polling threads, or a persistent HUD.
#pragma once

#include <string>

namespace mimita::game {

class HotStatusTips {
public:
    static HotStatusTips& instance();
    void update();
    std::string currentGeneration() const;

private:
    HotStatusTips();
    ~HotStatusTips();
    HotStatusTips(const HotStatusTips&) = delete;
    HotStatusTips& operator=(const HotStatusTips&) = delete;

    struct Impl;
    Impl* impl_ = nullptr;
};

void updateHotStatusTips();

}  // namespace mimita::game
