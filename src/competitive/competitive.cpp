#include "competitive.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "../website/api-client.h"
#include "../auth/auth-system.h"
#include "../devtools/terminal.h"

using json = nlohmann::json;

namespace {
CompetitiveProfile gProfile;
std::string gProfileAccount = "default";
}

CompetitiveTier tierForMmr(int mmr)
{
    if (mmr < 1) return CompetitiveTier::None;
    for (int i = kRankTierCount - 1; i > 0; --i) {
        if (mmr >= kRankTiers[i].minMmr && mmr <= kRankTiers[i].maxMmr)
            return kRankTiers[i].tier;
    }
    return CompetitiveTier::None;
}

const RankTierInfo& tierInfo(CompetitiveTier t)
{
    for (int i = 0; i < kRankTierCount; ++i) {
        if (kRankTiers[i].tier == t)
            return kRankTiers[i];
    }
    return kRankTiers[0];
}

const RankTierInfo& tierInfoForMmr(int mmr)
{
    return tierInfo(tierForMmr(mmr));
}

int calculateMmrChange(int playerMmr, int opponentMmr, bool won)
{
    double expected = 1.0 / (1.0 + std::pow(10.0, (double)(opponentMmr - playerMmr) / 1000.0));
    double actual = won ? 1.0 : 0.0;
    int change = (int)std::round((double)kMmrK * (actual - expected));
    return change;
}

CompetitiveMatchResult calculateMatchResult(int playerMmr, int opponentMmr, bool won,
                                              int playerKills, int opponentKills,
                                              const std::vector<std::string>& existingAchievements)
{
    CompetitiveMatchResult r;
    r.won = won;
    r.playerKills = playerKills;
    r.opponentKills = opponentKills;
    r.mmrBefore = playerMmr;
    r.opponentMmr = opponentMmr;
    r.tierBefore = tierInfoForMmr(playerMmr);

    int change = calculateMmrChange(playerMmr, opponentMmr, won);
    r.mmrChange = change;
    r.mmrAfter = std::max(kMmrMin, std::min(kMmrMax, playerMmr + change));
    r.tierAfter = tierInfoForMmr(r.mmrAfter);

    // Check for new rank achievements
    CompetitiveTier achievedTier = tierForMmr(r.mmrAfter);
    for (int i = 0; i < kRankAchievementCount; ++i) {
        if (kRankAchievements[i].tier == achievedTier || kRankAchievements[i].tier < achievedTier) {
            bool alreadyHas = false;
            for (const auto& a : existingAchievements) {
                if (a == kRankAchievements[i].achievementId) {
                    alreadyHas = true;
                    break;
                }
            }
            if (!alreadyHas && achievedTier >= kRankAchievements[i].tier) {
                // Only unlock when this specific tier was just reached
                CompetitiveTier prevTier = tierForMmr(r.mmrBefore);
                if (kRankAchievements[i].tier > prevTier && kRankAchievements[i].tier <= achievedTier) {
                    r.newAchievements.push_back(kRankAchievements[i].achievementId);
                }
            }
        }
    }

    return r;
}

CompetitiveProfile& GetCompetitiveProfile()
{
    return gProfile;
}

void LoadCompetitiveProfile(const std::string& account)
{
    gProfileAccount = account;
    gProfile = CompetitiveProfile{};

    std::string path = "config/accounts/" + account + ".json";
    std::ifstream file(path);
    if (!file.is_open()) return;

    try {
        json root;
        file >> root;
        if (root.contains("competitive")) {
            auto& c = root["competitive"];
            gProfile.mmr = c.value("mmr", kMmrDefault);
            gProfile.highestMmr = c.value("highest_mmr", kMmrDefault);
            gProfile.wins = c.value("wins", 0);
            gProfile.losses = c.value("losses", 0);
            gProfile.gamesPlayed = c.value("games_played", 0);
            gProfile.currentStreak = c.value("current_streak", 0);
            gProfile.bestStreak = c.value("best_streak", 0);
            gProfile.globalRank = c.value("global_rank", 0);
            if (c.contains("achievements") && c["achievements"].is_array()) {
                for (auto& a : c["achievements"])
                    gProfile.achievements.push_back(a.get<std::string>());
            }
        }
    } catch (const std::exception& e) {
        printf("[COMPETITIVE] Failed to load profile: %s\n", e.what());
    }
}

void SaveCompetitiveProfile(const std::string& account)
{
    std::string path = "config/accounts/" + account + ".json";
    std::filesystem::create_directories("config/accounts");

    json root;
    {
        std::ifstream input(path);
        if (input.is_open()) {
            try { input >> root; } catch (...) { root = json::object(); }
        }
    }

    json c;
    c["mmr"] = gProfile.mmr;
    c["highest_mmr"] = gProfile.highestMmr;
    c["wins"] = gProfile.wins;
    c["losses"] = gProfile.losses;
    c["games_played"] = gProfile.gamesPlayed;
    c["current_streak"] = gProfile.currentStreak;
    c["best_streak"] = gProfile.bestStreak;
    c["global_rank"] = gProfile.globalRank;
    c["achievements"] = json::array();
    for (const auto& a : gProfile.achievements)
        c["achievements"].push_back(a);

    root["competitive"] = c;

    const std::string tmp = path + ".tmp";
    std::ofstream out(tmp);
    if (!out.is_open()) return;
    out << root.dump(2);
    out.close();
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec)
        printf("[COMPETITIVE] Save failed: %s\n", ec.message().c_str());
}

void RefreshCompetitiveProfileFromApi()
{
    AuthSystem& auth = AuthSystem::instance();
    if (auth.state() != AuthState::Authenticated) return;

    std::string token = auth.user().sessionToken;
    if (token.empty()) return;

    GameStats stats = getStats(token);
    if (stats.currentMmr > 0) {
        gProfile.mmr = stats.currentMmr;
        gProfile.highestMmr = stats.highestMmr;
    }
    gProfile.wins = stats.wins;
    gProfile.losses = stats.losses;
    gProfile.gamesPlayed = stats.gamesPlayed;
    SaveCompetitiveProfile(gProfileAccount);
}

void SubmitCompetitiveMatch(const CompetitiveMatchResult& result)
{
    CompetitiveProfile& p = GetCompetitiveProfile();

    p.mmr = result.mmrAfter;
    if (result.mmrAfter > p.highestMmr)
        p.highestMmr = result.mmrAfter;

    if (result.won) {
        p.wins++;
        p.currentStreak = std::max(0, p.currentStreak + 1);
    } else {
        p.losses++;
        p.currentStreak = std::min(0, p.currentStreak - 1);
    }
    p.bestStreak = std::max(p.bestStreak, std::abs(p.currentStreak));
    p.gamesPlayed++;

    for (const auto& a : result.newAchievements) {
        bool found = false;
        for (const auto& e : p.achievements) {
            if (e == a) { found = true; break; }
        }
        if (!found) p.achievements.push_back(a);
    }

    SaveCompetitiveProfile(gProfileAccount);

    // Submit to API if authenticated
    AuthSystem& auth = AuthSystem::instance();
    if (auth.state() == AuthState::Authenticated && !auth.user().sessionToken.empty()) {
        json matchData;
        matchData["mode"] = "competitive_duels";
        matchData["won"] = result.won;
        matchData["player_kills"] = result.playerKills;
        matchData["opponent_kills"] = result.opponentKills;
        matchData["mmr_before"] = result.mmrBefore;
        matchData["mmr_after"] = result.mmrAfter;
        matchData["mmr_change"] = result.mmrChange;
        matchData["opponent_mmr"] = result.opponentMmr;
        submitMatchResult(auth.user().sessionToken, matchData);
        printf("[COMPETITIVE] Submitted match result to API\n");
    }
}

void PrintCompetitiveProfile()
{
    CompetitiveProfile& p = GetCompetitiveProfile();
    const auto& ti = tierInfoForMmr(p.mmr);
    printf("=== COMPETITIVE PROFILE ===\n");
    printf("MMR: %d\n", p.mmr);
    printf("Rank: %s\n", ti.displayName);
    printf("Highest MMR: %d\n", p.highestMmr);
    printf("Wins: %d  Losses: %d\n", p.wins, p.losses);
    printf("Games Played: %d\n", p.gamesPlayed);
    printf("Win Rate: %.1f%%\n", p.gamesPlayed > 0 ? (100.0f * p.wins / p.gamesPlayed) : 0.0f);
    printf("Streak: %d  Best: %d\n", p.currentStreak, p.bestStreak);
    printf("Global Rank: #%d\n", p.globalRank);
    printf("Achievements (%zu):\n", p.achievements.size());
    for (const auto& a : p.achievements)
        printf("  - %s\n", a.c_str());
}
