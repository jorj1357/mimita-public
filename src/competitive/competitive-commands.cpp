#include "competitive.h"
#include "../devtools/terminal.h"
#include "../auth/auth-system.h"

void registerCompetitiveCommands()
{
    Terminal& t = Terminal::instance();

    t.registerCommand({
        "comp.profile",
        "Show competitive profile",
        "comp.profile",
        [](const std::vector<std::string>&) {
            PrintCompetitiveProfile();
            Terminal::instance().addLog("[COMP] Profile printed to console");
        },
        std::string(),
        CommandCategory::Player
    });

    t.registerCommand({
        "comp.refresh",
        "Refresh competitive profile from API",
        "comp.refresh",
        [](const std::vector<std::string>&) {
            RefreshCompetitiveProfileFromApi();
            Terminal::instance().addLog("[COMP] Profile refreshed from API");
        },
        std::string(),
        CommandCategory::Player
    });

    t.registerCommand({
        "comp.mmr",
        "Set MMR for testing (admin only)",
        "comp.mmr <value>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: comp.mmr <value>");
                return;
            }
            int newMmr = std::stoi(args[0]);
            newMmr = std::max(kMmrMin, std::min(kMmrMax, newMmr));
            CompetitiveProfile& p = GetCompetitiveProfile();
            p.mmr = newMmr;
            if (newMmr > p.highestMmr)
                p.highestMmr = newMmr;
            SaveCompetitiveProfile();
            const auto& ti = tierInfoForMmr(newMmr);
            Terminal::instance().addLog("[COMP] MMR set to " + std::to_string(newMmr)
                + " (Rank: " + ti.displayName + ")");
        },
        std::string(),
        CommandCategory::Debug
    });

    t.registerCommand({
        "comp.calc",
        "Calculate MMR change for a match",
        "comp.calc <playerMMR> <opponentMMR> <1=win|0=loss>",
        [](const std::vector<std::string>& args) {
            if (args.size() < 3) {
                Terminal::instance().addLog("[ERROR] Usage: comp.calc <playerMMR> <opponentMMR> <1|0>");
                return;
            }
            int pMmr = std::stoi(args[0]);
            int oMmr = std::stoi(args[1]);
            bool won = args[2] == "1";
            int change = calculateMmrChange(pMmr, oMmr, won);
            int newMmr = std::max(kMmrMin, std::min(kMmrMax, pMmr + change));
            char buf[256];
            snprintf(buf, sizeof(buf), "[COMP] %d vs %d (%s): change=%d new=%d",
                     pMmr, oMmr, won ? "WIN" : "LOSS", change, newMmr);
            Terminal::instance().addLog(buf);
        },
        std::string(),
        CommandCategory::Debug
    });

    t.registerCommand({
        "comp.simulate",
        "Simulate a match result",
        "comp.simulate <won=1|0> <opponentMMR> [playerKills] [opponentKills]",
        [](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                Terminal::instance().addLog("[ERROR] Usage: comp.simulate <1|0> <opponentMMR> [kills] [opponentKills]");
                return;
            }
            bool won = args[0] == "1";
            int oppMmr = std::stoi(args[1]);
            int pKills = args.size() > 2 ? std::stoi(args[2]) : 5;
            int oKills = args.size() > 3 ? std::stoi(args[3]) : (won ? 2 : 5);

            CompetitiveProfile& p = GetCompetitiveProfile();
            auto result = calculateMatchResult(p.mmr, oppMmr, won, pKills, oKills, p.achievements);
            SubmitCompetitiveMatch(result);

            const auto& tiB = tierInfoForMmr(result.mmrBefore);
            const auto& tiA = tierInfoForMmr(result.mmrAfter);
            char buf[256];
            snprintf(buf, sizeof(buf), "[COMP] Match: %s | MMR: %d->%d (%+d) | Rank: %s -> %s",
                     won ? "VICTORY" : "DEFEAT",
                     result.mmrBefore, result.mmrAfter, result.mmrChange,
                     tiB.displayName, tiA.displayName);
            Terminal::instance().addLog(buf);

            for (const auto& a : result.newAchievements) {
                Terminal::instance().addLog("[COMP] Achievement unlocked: " + a);
            }
        },
        std::string(),
        CommandCategory::Debug
    });

    t.registerCommand({
        "comp.tier",
        "Look up rank tier for an MMR value",
        "comp.tier <mmr>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: comp.tier <mmr>");
                return;
            }
            int mmr = std::stoi(args[0]);
            const auto& ti = tierInfoForMmr(mmr);
            char buf[128];
            snprintf(buf, sizeof(buf), "[COMP] MMR %d = Rank %s (%d-%d)",
                     mmr, ti.displayName, ti.minMmr, ti.maxMmr);
            Terminal::instance().addLog(buf);
        },
        std::string(),
        CommandCategory::Player
    });
}
