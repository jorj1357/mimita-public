#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct GameUserInfo
{
    bool valid = false;
    int id = 0;
    std::string username;
    std::string displayName;
    std::string email;
    std::string bio;
    std::string avatarUrl;
    json avatarData;
    std::string supporterTier;
    std::string role;
    std::vector<std::string> achievements;
    bool emailVerified = false;
    std::string createdAt;
};

struct LinkCodeResult
{
    bool ok = false;
    std::string code;
    std::string grantToken;
};

struct GameStats
{
    int wins = 0;
    int losses = 0;
    int kills = 0;
    int deaths = 0;
    int gamesPlayed = 0;
    long long playtimeSeconds = 0;
    int highestMmr = 1000;
    int currentMmr = 1000;
    float accuracy = 0.0f;
    int headshots = 0;
    int bestKillStreak = 0;
};

struct MatchEntry
{
    std::string matchId;
    std::string mapName;
    std::string gameMode;
    int durationSeconds = 0;
    std::string createdAt;
    int kills = 0;
    int deaths = 0;
    float accuracy = 0.0f;
    int headshots = 0;
    int damageDealt = 0;
    bool won = false;
    int mmrBefore = 0;
    int mmrAfter = 0;
};

struct LeaderboardEntry
{
    int rank = 0;
    int id = 0;
    std::string username;
    std::string avatarUrl;
    std::string supporterTier;
    GameStats stats;
};

// Core auth
bool websiteReachable();
LinkCodeResult requestAuthLink();
bool pollLinkStatus(const std::string& code);
std::string finalizeLink(const std::string& code, const std::string& grantToken);
GameUserInfo validateSession(const std::string& sessionToken);

// Token exchange (mimita:// flow)
std::string exchangeTempToken(const std::string& exchangeToken);

// Profile
GameUserInfo getProfile(const std::string& sessionToken);
bool updateProfile(const std::string& sessionToken, const json& updates);

// Avatar data (JSON)
json getAvatarData(const std::string& sessionToken);
bool uploadAvatarData(const std::string& sessionToken, const json& avatarData);

// Stats
GameStats getStats(const std::string& sessionToken);
bool submitMatchResult(const std::string& sessionToken, const json& matchData);

// Leaderboard
std::vector<LeaderboardEntry> getLeaderboard(const std::string& type, int limit = 50);

// Match history
std::vector<MatchEntry> getMatchHistory(const std::string& sessionToken, int page = 1, int limit = 20);

// Settings
json getSettings(const std::string& sessionToken);
bool updateSettings(const std::string& sessionToken, const json& settings);
