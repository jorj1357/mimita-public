// 2026-11-15
// Live hot-reload feedback for the running game generation.
// File discovery runs in the resident JSON watcher; JSON parsing runs on the
// resident job executor so the gameplay thread only consumes a status snapshot.

#include "game/hot-status-tips.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "game/job-bridge-module.h"
#include "game/resident-state-module.h"
#include "entities/player.h"
#include "network/multiplayer-context.h"
#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "notifications/notifications.h"
#include "utils/json-file-watch.h"

namespace mimita::game {
namespace {

struct BuildStatus {
    std::string generation;
    std::string phase;
    std::string result;
    std::string generatedUtc;
    std::string publishedUtc;
    std::string failedPath;
    std::vector<std::string> files;
    bool hasReady = false;
};

nlohmann::json readJson(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) return {};
    try {
        nlohmann::json value;
        file >> value;
        return value;
    } catch (const std::exception&) {
        return {};
    }
}

std::string stringValue(const nlohmann::json& value, const char* key) {
    return value.contains(key) && value[key].is_string()
        ? value[key].get<std::string>() : std::string();
}

void appendUnique(std::vector<std::string>& out, const std::string& value) {
    if (value.empty()) return;
    for (const std::string& existing : out) {
        if (existing == value) return;
    }
    out.push_back(value);
}

BuildStatus readStatus() {
    BuildStatus status;
    const nlohmann::json current = readJson("build/game/current.json");
    const nlohmann::json progress = readJson("build/game/progress.json");
    const nlohmann::json ready = readJson("build/game/ready.json");
    const nlohmann::json change = readJson("build/hot-reload/change-report.json");

    if (current.contains("generation")) {
        status.generation = std::to_string(current["generation"].get<std::uint64_t>());
    }
    status.generatedUtc = stringValue(current, "generated_utc");
    if (ready.contains("generation")) {
        const std::string readyGeneration =
            std::to_string(ready["generation"].get<std::uint64_t>());
        if (status.generation.empty()) status.generation = readyGeneration;
        status.hasReady = true;
    }
    status.publishedUtc = stringValue(ready, "published_utc");
    status.phase = stringValue(progress, "phase");
    status.result = stringValue(progress, "result");
    if (status.result.empty()) status.result = stringValue(current, "status");
    if (status.generatedUtc.empty()) status.generatedUtc = stringValue(progress, "updated_utc");

    auto collect = [&](const nlohmann::json& source) {
        if (!source.contains("change_report") ||
            !source["change_report"].is_object()) {
            return;
        }
        const auto& report = source["change_report"];
        for (const char* key : {"changed", "added", "removed",
                                "changed_headers", "added_headers",
                                "removed_headers", "changed_owners"}) {
            if (!report.contains(key) || !report[key].is_array()) continue;
            for (const auto& item : report[key]) {
                if (item.is_string()) appendUnique(status.files, item.get<std::string>());
            }
        }
    };
    collect(current);
    collect(change);
    if (status.files.empty() && change.contains("changed") &&
        change["changed"].is_array()) {
        for (const auto& item : change["changed"]) {
            if (item.is_string()) appendUnique(status.files, item.get<std::string>());
        }
    }
    return status;
}

std::string filesText(const std::vector<std::string>& files) {
    if (files.empty()) return "no changed source files reported";
    std::string text = "files:\n";
    const std::size_t count = std::min<std::size_t>(files.size(), 6);
    for (std::size_t i = 0; i < count; ++i) text += "  " + files[i] + "\n";
    if (files.size() > count) {
        text += "  +" + std::to_string(files.size() - count) + " more";
    } else if (!text.empty() && text.back() == '\n') {
        text.pop_back();
    }
    return text;
}

std::string tickText() {
    std::string text = "ticks: ";
    if (const auto* context = mpContextPtr()) {
        text += "server " + std::to_string(context->latestServerTick);
    } else {
        text += "server unavailable";
    }
    if (const Player* player = playerPtr()) {
        text += ", client " + std::to_string(player->movementSimulationTick);
    } else {
        text += ", client unavailable";
    }
    return text;
}

}  // namespace

struct HotStatusTips::Impl {
    JsonFileWatch watch{{"build/game", "build/hot-reload"},
                        std::chrono::milliseconds(250)};
    std::mutex mutex;
    BuildStatus pending;
    bool hasPending = false;
    std::atomic<bool> parsing{false};
    std::string lastKey;
    std::string lastReadyGeneration;
    std::string currentGeneration;
    bool firstPass = true;
};

HotStatusTips& HotStatusTips::instance() {
    static HotStatusTips tips;
    return tips;
}

HotStatusTips::HotStatusTips() : impl_(new Impl()) {}
HotStatusTips::~HotStatusTips() { delete impl_; }

void HotStatusTips::update() {
    if (!impl_) return;
    static bool commandsRegistered = false;
    if (!commandsRegistered) {
        commandsRegistered = true;
        Terminal::instance().registerCommand({
            "hotreload", "Control hot-reload tips verbosity",
            "hotreload tips <all|minimal|none> | files <on|off> | save | status",
            [](const std::vector<std::string>& args) {
                auto& notifications = NotificationSystem::instance();
                if (args.empty()) {
                    Terminal::instance().addLog(
                        "[HOTRELOAD] tips=" + notifications.hotReloadVerbosity() +
                        " files=" + (notifications.hotReloadShowFiles() ? "on" : "off"));
                    return;
                }
                if (args[0] == "tips" && args.size() >= 2) {
                    notifications.setHotReloadVerbosity(args[1]);
                    Terminal::instance().addLog("[HOTRELOAD] tips=" + args[1]);
                } else if (args[0] == "files" && args.size() >= 2) {
                    notifications.setHotReloadShowFiles(args[1] != "off");
                    Terminal::instance().addLog("[HOTRELOAD] files=" + args[1]);
                } else if (args[0] == "save") {
                    notifications.saveConfigNow();
                    Terminal::instance().addLog("[HOTRELOAD] saved config/notifications.json");
                } else if (args[0] == "status") {
                    Terminal::instance().addLog(
                        "[HOTRELOAD] tips=" + notifications.hotReloadVerbosity() +
                        " files=" + (notifications.hotReloadShowFiles() ? "on" : "off"));
                }
            },
            "2026-11-15", CommandCategory::Debug});
    }
    if (impl_->watch.consumeDirty() && !impl_->parsing.exchange(true)) {
        runDetached(
            [this] {
                BuildStatus status = readStatus();
                {
                    std::lock_guard<std::mutex> lock(impl_->mutex);
                    impl_->pending = std::move(status);
                    impl_->hasPending = true;
                }
                impl_->parsing.store(false, std::memory_order_release);
            },
            "hot-status-tips");
    }

    BuildStatus status;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->hasPending) return;
        status = std::move(impl_->pending);
        impl_->hasPending = false;
    }
    if (status.generation.empty()) return;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->currentGeneration = status.generation;
    }
    const std::string verbosity = NotificationSystem::instance().hotReloadVerbosity();
    if (verbosity == "none") return;

    const std::string key = status.generation + "|" + status.result + "|" +
                            status.phase + "|" + status.generatedUtc + "|" +
                            status.publishedUtc;
    if (key == impl_->lastKey) return;
    impl_->lastKey = key;

    const bool failed = status.result == "failure" || status.result == "failed";
    const std::string stamp = status.generatedUtc.empty()
        ? status.publishedUtc : status.generatedUtc;
    const std::string header =
        "generation " + status.generation +
        (status.phase.empty() ? "" : "  phase " + status.phase) +
        (stamp.empty() ? "" : "\n" + stamp) +
        "\n" + tickText() +
        (NotificationSystem::instance().hotReloadShowFiles()
             ? "\n" + filesText(status.files) : std::string());

    if (failed) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "hot-status.notice", "hot-status", "published hot reload failure tip", 0,
            nlohmann::json{{"generation", status.generation},
                           {"result", status.result},
                           {"phase", status.phase},
                           {"files", status.files.size()}},
            __FILE__, __LINE__, __FUNCTION__);
        NotificationSystem::instance().pushHotReloadFailure(
            "Hot reload failed",
            "previous generation remains active\n" + header +
                "\nfix the reported source and rebuild; do not restart the EXE");
        impl_->firstPass = false;
        return;
    }

    if (status.hasReady && status.generation != impl_->lastReadyGeneration) {
        impl_->lastReadyGeneration = status.generation;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "hot-status.notice", "hot-status", "published hot reload ready tip", 0,
            nlohmann::json{{"generation", status.generation},
                           {"files", status.files.size()}},
            __FILE__, __LINE__, __FUNCTION__);
        NotificationSystem::instance().pushHotReload(
            "Hot reload ready", "generation is published and ready for activation\n" + header);
        impl_->firstPass = false;
        return;
    }

    if (impl_->firstPass) {
        impl_->firstPass = false;
        return;
    }
    if (verbosity == "minimal") return;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "hot-status.notice", "hot-status", "published hot reload detected tip", 0,
        nlohmann::json{{"generation", status.generation},
                       {"files", status.files.size()}},
        __FILE__, __LINE__, __FUNCTION__);
    NotificationSystem::instance().pushHotReload(
        "Hot reload detected", "candidate build observed; activation follows the safe boundary\n" + header);
}

std::string HotStatusTips::currentGeneration() const {
    if (!impl_) return {};
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->currentGeneration;
}

void updateHotStatusTips() { HotStatusTips::instance().update(); }

}  // namespace mimita::game
