#pragma once

#include <string>
#include <vector>
#include <cstdint>

// ── Rank Tiers ──────────────────────────────────────────────────────
enum class CompetitiveTier {
    None, DMinus, D, CMinus, C, BMinus, B, AMinus, A, SMinus, S
};

struct RankTierInfo {
    CompetitiveTier tier;
    const char* label;
    const char* displayName;
    int minMmr;
    int maxMmr;
};

constexpr RankTierInfo kRankTiers[] = {
    {CompetitiveTier::None,    "Unranked", "Unranked",     0,    0},
    {CompetitiveTier::DMinus,  "D-",       "D-",           1,    999},
    {CompetitiveTier::D,       "D",        "D",            1000, 1999},
    {CompetitiveTier::CMinus,  "C-",       "C-",           2000, 2999},
    {CompetitiveTier::C,       "C",        "C",            3000, 3999},
    {CompetitiveTier::BMinus,  "B-",       "B-",           4000, 4999},
    {CompetitiveTier::B,       "B",        "B",            5000, 5999},
    {CompetitiveTier::AMinus,  "A-",       "A-",           6000, 6999},
    {CompetitiveTier::A,       "A",        "A",            7000, 7999},
    {CompetitiveTier::SMinus,  "S-",       "S-",           8000, 8999},
    {CompetitiveTier::S,       "S",        "S",            9000, 9999},
};
constexpr int kRankTierCount = sizeof(kRankTiers) / sizeof(kRankTiers[0]);

// ── Competitive Profile ─────────────────────────────────────────────
struct CompetitiveProfile {
    int mmr = 5000;
    int highestMmr = 5000;
    int wins = 0;
    int losses = 0;
    int gamesPlayed = 0;
    int currentStreak = 0;
    int bestStreak = 0;
    int globalRank = 0;
    std::vector<std::string> achievements;
};

// ── MMR Constants ───────────────────────────────────────────────────
constexpr int kMmrMin = 1;
constexpr int kMmrMax = 9999;
constexpr int kMmrDefault = 5000;
constexpr int kMmrK = 100;

// ── Match Result ────────────────────────────────────────────────────
struct CompetitiveMatchResult {
    bool won = false;
    int playerKills = 0;
    int opponentKills = 0;
    int mmrBefore = kMmrDefault;
    int mmrAfter = kMmrDefault;
    int mmrChange = 0;
    int opponentMmr = kMmrDefault;
    RankTierInfo tierBefore;
    RankTierInfo tierAfter;
    std::vector<std::string> newAchievements;
};

// ── Rank Achievements ───────────────────────────────────────────────
struct RankAchievementDef {
    const char* achievementId;
    CompetitiveTier tier;
    const char* title;
    const char* description;
};

constexpr RankAchievementDef kRankAchievements[] = {
    {"rank_dminus", CompetitiveTier::DMinus, "Reached D-", "You reached Tier D- in Competitive Duels."},
    {"rank_d",      CompetitiveTier::D,      "Reached D",  "You reached Tier D in Competitive Duels."},
    {"rank_cminus", CompetitiveTier::CMinus, "Reached C-", "You reached Tier C- in Competitive Duels."},
    {"rank_c",      CompetitiveTier::C,      "Reached C",  "You reached Tier C in Competitive Duels."},
    {"rank_bminus", CompetitiveTier::BMinus, "Reached B-", "You reached Tier B- in Competitive Duels."},
    {"rank_b",      CompetitiveTier::B,      "Reached B",  "You reached Tier B in Competitive Duels."},
    {"rank_aminus", CompetitiveTier::AMinus, "Reached A-", "You reached Tier A- in Competitive Duels."},
    {"rank_a",      CompetitiveTier::A,      "Reached A",  "You reached Tier A in Competitive Duels."},
    {"rank_sminus", CompetitiveTier::SMinus, "Reached S-", "You reached Tier S- in Competitive Duels."},
    {"rank_s",      CompetitiveTier::S,      "Reached S",  "You reached Tier S in Competitive Duels."},
};
constexpr int kRankAchievementCount = sizeof(kRankAchievements) / sizeof(kRankAchievements[0]);

// ── API ──────────────────────────────────────────────────────────────
CompetitiveTier tierForMmr(int mmr);
const RankTierInfo& tierInfo(CompetitiveTier t);
const RankTierInfo& tierInfoForMmr(int mmr);

int calculateMmrChange(int playerMmr, int opponentMmr, bool won);
CompetitiveMatchResult calculateMatchResult(int playerMmr, int opponentMmr, bool won,
                                              int playerKills, int opponentKills,
                                              const std::vector<std::string>& existingAchievements);

CompetitiveProfile& GetCompetitiveProfile();
void LoadCompetitiveProfile(const std::string& account = "default");
void SaveCompetitiveProfile(const std::string& account = "default");
void RefreshCompetitiveProfileFromApi();

void SubmitCompetitiveMatch(const CompetitiveMatchResult& result);
void PrintCompetitiveProfile();
