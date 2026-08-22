#include <cstdio>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "game/duel.h"

extern DuelManager gDuelManager;

void registerDuelCommands()
{
    Terminal::instance().registerCommand({
        "duel.start",
        "Start duel mode",
        "duel.start [npcCount]",
        [](const std::vector<std::string>& args)
        {
            DuelConfig cfg;

            cfg.numNpcs =
                args.empty()
                ? 3
                : std::clamp(std::stoi(args[0]), 1, 10);

            cfg.killsToWin = 10;
            cfg.duelLengthSeconds = 300;
            cfg.enabled = true;

            DUEL_CONFIG = cfg;

            // Local/offline duel only. Network duels are controlled by DuelQueue + server DuelStatePacket.
            gDuelManager.start(
                DUEL_CONFIG,
                THE_PLAYER,
                THE_NPC_SYSTEM,
                THE_WORLD);

            Terminal::instance().addLog(
                "[DUEL] started");
        }
    });

    Terminal::instance().registerCommand({
        "duel_state",
        "Show current duel end state",
        "duel_state",
        [](const std::vector<std::string>&) {
            const char* stateName = "None";
            switch (gDuelManager.endState()) {
            case DuelEndState::None:          stateName = "None"; break;
            case DuelEndState::VictoryScreen: stateName = "VictoryScreen"; break;
            case DuelEndState::Countdown:     stateName = "Countdown"; break;
            case DuelEndState::FinalKillReplay: stateName = "FinalKillReplay"; break;
            case DuelEndState::ReplayMenu:    stateName = "ReplayMenu"; break;
            }
            char buf[256];
            float timeRemaining = 0.0f;
            if (gDuelManager.endState() == DuelEndState::VictoryScreen)
                timeRemaining = gDuelManager.victoryTimeLeft();
            else if (gDuelManager.endState() == DuelEndState::Countdown)
                timeRemaining = gDuelManager.countdownTimeLeft();
            snprintf(buf, sizeof(buf),
                "Current State: %s\nTime Remaining: %.2f\nReplay Loaded: %s\nReplay Playing: %s\nFinal Kill Marker: %s",
                stateName,
                timeRemaining,
                REPLAY_PLAYER.totalTicks() > 0 ? "YES" : "NO",
                REPLAY_PLAYER.isPlaying() ? "YES" : "NO",
                gDuelManager.isReplayReady() ? "YES" : "NO");
            Terminal::instance().addLog(buf);
        }
    });
}
