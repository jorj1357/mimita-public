#include "perf/perf.h"

#include "devtools/terminal.h"
#include "video/frame-pacer.h"

extern FramePacer gFramePacer;

void registerPerfCommands()
{
    auto& t = Terminal::instance();

    // Umbrella toggle: enable/disable all perf overlays
    t.registerCommand({
        "perf",
        "Toggle all performance overlays (report, graph, NPC, memory, spikes)",
        "perf [0|1]",
        [](const std::vector<std::string>& args) {
            PerfState& s = Perf::state();
            bool val = args.empty() ? !s.showPerfReport : args[0] != "0";
            s.showPerfReport = val;
            s.showGraph = val;
            s.showNpcPerf = val;
            s.showMemory = val;
            s.showSpikes = val;
            Terminal::instance().addLog(std::string("[PERF] perf=") + (val ? "ON" : "OFF"));
        }
    });

    // NPC Profiling overlay
    t.registerCommand({
        "perf_npc",
        "Toggle NPC performance breakdown (update, combat, pathfinding, collision, render)",
        "perf_npc [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty())
                Perf::toggleNpcPerf();
            else
                Perf::state().showNpcPerf = args[0] == "1";
            Terminal::instance().addLog(std::string("[PERF] perf_npc=") +
                (Perf::state().showNpcPerf ? "ON" : "OFF"));
        }
    });

    // Memory / Long session overlay
    t.registerCommand({
        "perf_memory",
        "Toggle memory tracking overlay (entity counts, replay MB, effects, audio, runtime)",
        "perf_memory [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty())
                Perf::toggleMemory();
            else
                Perf::state().showMemory = args[0] == "1";
            Terminal::instance().addLog(std::string("[PERF] perf_memory=") +
                (Perf::state().showMemory ? "ON" : "OFF"));
        }
    });

    // Spike detection
    t.registerCommand({
        "perf_spikes",
        "Toggle frame spike detection: logs and displays spikes with subsystem breakdown",
        "perf_spikes [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty())
                Perf::toggleSpikes();
            else
                Perf::state().showSpikes = args[0] == "1";
            Terminal::instance().addLog(std::string("[PERF] perf_spikes=") +
                (Perf::state().showSpikes ? "ON" : "OFF"));
        }
    });

    // Part 2: Performance report
    t.registerCommand({
        "perf_report",
        "Toggle detailed performance overlay (FPS, CPU, GPU, draw calls, entities, effects)",
        "perf_report [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty())
                Perf::togglePerfReport();
            else
                Perf::state().showPerfReport = args[0] == "1";
            Terminal::instance().addLog(std::string("[PERF] perf_report=") +
                (Perf::state().showPerfReport ? "ON" : "OFF"));
        }
    });

    // Part 5: Performance graph
    t.registerCommand({
        "perf_graph",
        "Toggle real-time frame time graph (last 300 frames, shows 16/8/4ms lines)",
        "perf_graph [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty())
                Perf::toggleGraph();
            else
                Perf::state().showGraph = args[0] == "1";
            Terminal::instance().addLog(std::string("[PERF] perf_graph=") +
                (Perf::state().showGraph ? "ON" : "OFF"));
        }
    });

    // Part 4: Frame hitch debug
    t.registerCommand({
        "frame_hitch_debug",
        "Log any frame exceeding 1.5x target frame budget with subsystem breakdown",
        "frame_hitch_debug [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty())
                gFramePacer.setHitchDebug(!gFramePacer.hitchDebug());
            else
                gFramePacer.setHitchDebug(args[0] == "1");
            Terminal::instance().addLog(std::string("[PERF] frame_hitch_debug=") +
                (gFramePacer.hitchDebug() ? "ON" : "OFF"));
        }
    });

    // Part 6: Performance presets
    t.registerCommand({
        "perf_low",
        "Set performance preset to Low: reduce blood, particles, effects, disable debug/post effects",
        "perf_low",
        [](const std::vector<std::string>&) {
            Perf::setPreset(1);
            Terminal::instance().addLog("[PERF] preset=Low");
        }
    });

    t.registerCommand({
        "perf_medium",
        "Set performance preset to Medium: moderate quality reduction",
        "perf_medium",
        [](const std::vector<std::string>&) {
            Perf::setPreset(2);
            Terminal::instance().addLog("[PERF] preset=Medium");
        }
    });

    t.registerCommand({
        "perf_high",
        "Set performance preset to High: full quality",
        "perf_high",
        [](const std::vector<std::string>&) {
            Perf::setPreset(3);
            Terminal::instance().addLog("[PERF] preset=High");
        }
    });

    // Part 7: Benchmark
    t.registerCommand({
        "perf_benchmark",
        "Run automatic benchmark for 60 seconds, output report to benchmark_report.txt",
        "perf_benchmark [seconds]",
        [](const std::vector<std::string>& args) {
            double secs = 60.0;
            if (!args.empty()) secs = std::atof(args[0].c_str());
            if (secs < 1.0) secs = 60.0;
            Perf::startBenchmark(secs);
            Terminal::instance().addLog(std::string("[PERF] Benchmark started for ") +
                std::to_string((int)secs) + " seconds");
        }
    });

    // Part 8: Stress test
    t.registerCommand({
        "perf_stress",
        "Run stress test: gradually spawn NPCs (20, 50, 100) and measure performance",
        "perf_stress [npcs]",
        [](const std::vector<std::string>& args) {
            int npcs = 100;
            if (!args.empty()) npcs = std::atoi(args[0].c_str());
            if (npcs < 1) npcs = 100;
            Perf::startStress(npcs);
            Terminal::instance().addLog(std::string("[PERF] Stress test started with ") +
                std::to_string(npcs) + " NPCs target");
        }
    });

    // Part 9: Combat stress test
    t.registerCommand({
        "perf_combat_test",
        "Run combat stress test: spawn NPCs and simulate combat with effects",
        "perf_combat_test",
        [](const std::vector<std::string>&) {
            Perf::startCombatTest();
            Terminal::instance().addLog("[PERF] Combat stress test started");
        }
    });

    // Part 10: Allocation audit
    t.registerCommand({
        "perf_alloc_audit",
        "Toggle allocation tracking: reports allocations per frame (goal: 0 during combat)",
        "perf_alloc_audit [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty())
                Perf::toggleAllocAudit();
            else
                Perf::state().allocAudit = args[0] == "1";
            Terminal::instance().addLog(std::string("[PERF] allocation_audit=") +
                (Perf::state().allocAudit ? "ON" : "OFF"));
        }
    });

    // Part 11: Audio audit
    t.registerCommand({
        "perf_audio_audit",
        "Toggle audio hitch audit: warns on sound loads during gameplay",
        "perf_audio_audit [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty())
                Perf::toggleAudioAudit();
            else
                Perf::state().audioAudit = args[0] == "1";
            Terminal::instance().addLog(std::string("[PERF] audio_audit=") +
                (Perf::state().audioAudit ? "ON" : "OFF"));
        }
    });

    // Part 14: Render stats
    t.registerCommand({
        "perf_render_stats",
        "Toggle render stats overlay: draw calls, triangles, expensive renderers",
        "perf_render_stats [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty())
                Perf::toggleRenderStats();
            else
                Perf::state().renderStats = args[0] == "1";
            Terminal::instance().addLog(std::string("[PERF] render_stats=") +
                (Perf::state().renderStats ? "ON" : "OFF"));
        }
    });

    // Part 16: Suggestions
    t.registerCommand({
        "perf_suggestions",
        "Analyze runtime stats and output ranked optimization opportunities",
        "perf_suggestions",
        [](const std::vector<std::string>&) {
            Perf::printSuggestions();
            Terminal::instance().addLog("[PERF] Suggestions printed to console");
        }
    });

    // Part 17: Export
    t.registerCommand({
        "perf_export",
        "Export complete benchmark report to benchmark_report.txt",
        "perf_export [path]",
        [](const std::vector<std::string>& args) {
            const char* path = "benchmark_report.txt";
            if (!args.empty()) path = args[0].c_str();
            Perf::exportReport(path);
            Terminal::instance().addLog(std::string("[PERF] Report exported to ") + path);
        }
    });
}
