#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct GLFWwindow;
class Player;
class NpcSystem;
struct World;
class Camera;

void setArmToWeaponPose(Player& p, bool hasBomb);

enum class BombTagPhase {
    Off,
    Countdown,
    Active,
    MatchEnd
};

enum class BombTagEndState {
    None,
    VictoryScreen,
    Countdown,
    FinalKillReplay,
    ReplayMenu
};

enum class BombTagMenuAction {
    None,
    PlayAgain,
    ExitToMenu
};

struct BombTagConfig {
    int numNpcs = 3;
    float npcDifficulty = 5.0f;
    int lives = 0; // 0 = infinite
    int timeLimitSeconds = 180;
    std::string mapPath = "assets/maps/mimita-aabb-only-interior-small-v4.glb";
    bool enabled = false;
};

struct BombTagStats {
    int kills = 0;
    int deaths = 0;
    int bombPasses = 0;
    int explosions = 0;
};

class BombTagManager {
public:
    void start(const BombTagConfig& cfg, Player& player, NpcSystem& npcs, World& world);
    void update(float dt, Player& player, NpcSystem& npcs, World& world);
    void renderHud();
    BombTagMenuAction renderMatchOverScreen(GLFWwindow* win);
    void setCamera(class Camera& cam) { mCamera = &cam; }

    bool enabled() const { return mConfig.enabled; }
    BombTagPhase phase() const { return mPhase; }
    bool isCountdownActive() const { return mPhase == BombTagPhase::Countdown; }
    BombTagEndState endState() const { return mEndState; }
    void setEndState(BombTagEndState s) { mEndState = s; }

    int bombHolderIndex() const { return mBombHolderIndex; }
    bool playerIsBombHolder() const { return mBombHolderIsPlayer; }

    void stop();

    uint32_t matchEndTick = 0;
    bool finalKillSavedOnce = false;

private:
    BombTagConfig mConfig;
    BombTagPhase mPhase = BombTagPhase::Off;

    float mCountdown = 3.0f;
    float mTimer = 0.0f;
    int mCurrentBombHolderNpc = -1;
    bool mBombHolderIsPlayer = false;
    int mBombHolderIndex = -1;
    float mBombTimer = 15.0f;
    float mPassCooldown = 0.0f;
    float mTransferCooldown = 0.0f;
    int mLastTickSecond = -1;
    float mTickPitch = 1.0f;
    bool mCooldownSoundPlayed = false;

    int mPlayerKills = 0;
    std::vector<int> mNpcKills;
    std::vector<int> mNpcPasses;
    int mPlayerDeaths = 0;
    int mPlayerBombPasses = 0;

    int mPlayerLivesRemaining = 0;

    glm::vec3 mSpawnPos{0.0f};
    glm::vec3 mBombWorldPos{0.0f};
    Camera* mCamera = nullptr;

    BombTagEndState mEndState = BombTagEndState::None;
    float mVictoryTimer = 0.0f;
    float mCountdownTimer = 0.0f;
    int mCurrentCountdownNumber = 0;

    int mWinnerIndex = -1;
    int mWinnerKills = 0;
    bool mWinnerIsTie = false;

    void assignInitialBomb(Player& player, NpcSystem& npcs);
    void passBomb(Player& player, NpcSystem& npcs);
    void explodeBomb(Player& player, NpcSystem& npcs);
    int findNearestLivingNpc(const glm::vec3& pos, NpcSystem& npcs, int excludeIndex);
    int countLivingNpcs(NpcSystem& npcs);
    void checkMatchEnd(Player& player, NpcSystem& npcs);
    void freezeAll(Player& player, NpcSystem& npcs);
};
