// 07 31 2026, 15 30
/* purpose
* Implements the badconn terminal commands for per-client network simulation.
* Lists presets, activates preset N, disables with 0, and reloads config.
* Keeps command output concise and routed through the terminal log.
* Does NOT own packet hooks, config parsing, or the simulator state.
*/

#include "terminal/badconn-commands.h"
#include "devtools/terminal.h"
#include "network/badconn/badconn.h"

#include <cctype>
#include <cstdio>
#include <string>

namespace {

std::string presetSummary(const badconn::BadConnPreset& preset)
{
    std::string summary = preset.id + ": " + preset.name;
    bool first = true;
    const auto append = [&](const std::string& text) {
        if (first)
        {
            summary += " (";
            first = false;
        }
        else
        {
            summary += ", ";
        }
        summary += text;
    };
    if (preset.latency.enabled)
    {
        if (preset.latency.baseMs > 0)
            append("latency " + std::to_string(preset.latency.baseMs) + "ms +-" +
                   std::to_string(preset.latency.jitterMs) + "ms jitter");
        else
            append("latency " + std::to_string(preset.latency.minMs) + "-" +
                   std::to_string(preset.latency.maxMs) + "ms");
    }
    if (preset.loss.enabled)
    {
        std::string lossText = "loss " + std::to_string((int)preset.loss.minPercent) + "-" +
            std::to_string((int)preset.loss.maxPercent) + "%";
        if (preset.loss.burstProbability > 0.0f)
            lossText += " burst" + std::to_string((int)preset.loss.burstPercent) + "%";
        append(lossText);
    }
    if (preset.reorder.enabled)
        append("reorder " + std::to_string((int)preset.reorder.minPercent) + "-" +
               std::to_string((int)preset.reorder.maxPercent) + "%");
    if (preset.blackout.enabled)
        append("blackout " + std::to_string(preset.blackout.minMs) + "-" +
               std::to_string(preset.blackout.maxMs) + "ms");
    if (!first)
        summary += ")";
    return summary;
}

void printStatus()
{
    if (!badconn::active())
    {
        Terminal::instance().addLog(
            "[BADCONN] no preset active — use 'badconn list' to see presets");
        return;
    }
    const badconn::BadConnMetrics& m = badconn::metrics();
    char buffer[256];
    snprintf(buffer, sizeof(buffer),
             "[BADCONN] preset %s '%s' delayed=%llu dropped=%llu reordered=%llu "
             "blackout=%d queues(out=%zu/in=%zu)",
             badconn::activePresetId().c_str(), badconn::activePresetName().c_str(),
             (unsigned long long)m.packetsDelayed,
             (unsigned long long)m.packetsDropped,
             (unsigned long long)m.packetsReordered,
             (int)m.blackoutActive, m.outQueueSize, m.inQueueSize);
    Terminal::instance().addLog(buffer);
}

void listPresets()
{
    const std::vector<badconn::BadConnPreset>& all = badconn::presets();
    if (all.empty())
    {
        Terminal::instance().addLog(
            "[BADCONN] no presets loaded — check " +
            std::string(badconn::configPath()));
        return;
    }
    for (const badconn::BadConnPreset& preset : all)
    {
        const bool isActive =
            badconn::active() && badconn::activePresetId() == preset.id;
        Terminal::instance().addLog((isActive ? "> " : "  ") + presetSummary(preset));
    }
    if (!badconn::active())
        Terminal::instance().addLog(
            "[BADCONN] none active — 'badconn <N>' enables a preset, 'badconn 0' disables");
}

void reloadConfig()
{
    badconn::disable();
    if (badconn::loadConfig(badconn::configPath()))
        Terminal::instance().addLog(
            "[BADCONN] config reloaded — " +
            std::to_string(badconn::presets().size()) + " presets available");
    else
        Terminal::instance().addLog(
            "[BADCONN] config reload FAILED — no presets active");
}

bool isNumeric(const std::string& text)
{
    if (text.empty())
        return false;
    for (const char c : text)
    {
        if (!std::isdigit((unsigned char)c))
            return false;
    }
    return true;
}

void activateById(const std::string& rawId)
{
    if (!isNumeric(rawId))
    {
        Terminal::instance().addLog(
            "[ERROR] badconn: '" + rawId +
            "' not recognized — use 'badconn list'");
        return;
    }
    const std::string id = std::to_string(std::stoul(rawId));
    if (badconn::activatePreset(id))
    {
        Terminal::instance().addLog(
            "[BADCONN] preset " + id + " '" + badconn::activePresetName() +
            "' activated (client-local)");
    }
    else
    {
        Terminal::instance().addLog(
            "[ERROR] badconn: preset '" + id +
            "' not found — use 'badconn list'");
    }
}

} // namespace

void registerBadConnCommands()
{
    Terminal::instance().registerCommand(
        {
            "badconn", "Simulate bad network conditions for this client only",
            "badconn [list|<N>|0|reload]",
            [](const std::vector<std::string>& args) {
                if (args.empty())
                {
                    printStatus();
                    return;
                }
                if (args[0] == "list")
                {
                    listPresets();
                    return;
                }
                if (args[0] == "reload")
                {
                    reloadConfig();
                    return;
                }
                if (args[0] == "0")
                {
                    badconn::disable();
                    Terminal::instance().addLog(
                        "[BADCONN] all impairments disabled");
                    return;
                }
                activateById(args[0]);
            },
        },
        "2026-07-31", CommandCategory::Network);
}
