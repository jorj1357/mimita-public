// 07 19 2026, 12 00
/* purpose
* Declare HTTP helpers used by the game client to talk to mimita.fun APIs.
* Model account, auth, profile, stats, settings, and game bootstrap responses.
* Keep website API shapes explicit for exe-side account loading.
* DOES NOT store local tokens or passwords.
* DOES NOT render UI or own gameplay state.
* DOES NOT implement backend database logic.
*/

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
    int highestMmr = 5000;
    int currentMmr = 5000;
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

struct GameBootstrap
{
    bool valid = false;
    GameUserInfo user;
    GameStats stats;
    json settings;
    json inventory;
    json titles;
    json loadout;
};

// Core auth
bool websiteReachable();
LinkCodeResult requestAuthLink();
bool pollLinkStatus(const std::string& code);
std::string finalizeLink(const std::string& code, const std::string& grantToken);
GameUserInfo validateSession(const std::string& sessionToken);
GameBootstrap getGameBootstrap(const std::string& sessionToken);

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

// Account-owned game data
json getInventory(const std::string& sessionToken);
bool updateInventory(const std::string& sessionToken, const json& inventory);
json getLoadout(const std::string& sessionToken);
bool updateLoadout(const std::string& sessionToken, const json& loadout);
json getTitles(const std::string& sessionToken);
bool updateTitles(const std::string& sessionToken, const json& titles);

// Game Auth (direct username/email + password login)
struct GameLoginResult
{
    bool ok = false;
    int accountId = 0;
    std::string username;
    std::vector<std::string> permissions;
    std::string supporterTier;
    std::string accessToken;
    std::string accessExpiresAt;
    std::string refreshToken;
    std::string refreshExpiresAt;
    std::string errorCode;
    std::string errorMessage;
};

struct GameRefreshResult
{
    bool ok = false;
    std::string accessToken;
    std::string accessExpiresAt;
    std::string refreshToken;
    std::string refreshExpiresAt;
    std::string errorCode;
    std::string errorMessage;
};

struct GameLogoutResult
{
    bool ok = false;
};

struct GameAccountLookupResult
{
    bool ok = false;
    bool exists = false;
    int accountId = 0;
    std::string username;
    std::string errorMessage;
};

GameAccountLookupResult gameLookupAccount(const std::string& identifier);
GameLoginResult gameLogin(const std::string& identifier, const std::string& password,
                          bool rememberMe, const std::string& deviceId,
                          const std::string& deviceName, const std::string& platform,
                          const std::string& clientBuild);
GameRefreshResult gameRefresh(const std::string& refreshToken, const std::string& deviceId);
GameLogoutResult gameLogout(const std::string& refreshToken, const std::string& deviceId);

// Client Login (4-letter code flow)
struct ClientCodePreview
{
    bool valid = false;
    std::string username;
    std::string displayName;
    std::string avatarUrl;
    json avatarData;
    std::string supporterTier;
};

struct ClientCodeConfirm
{
    bool success = false;
    std::string sessionToken;
    int accountId = 0;
    std::string username;
};

ClientCodePreview previewClientCode(const std::string& code);
ClientCodeConfirm confirmClientCode(const std::string& code);
std::string createClientCode(); // website-side only, returns raw code
