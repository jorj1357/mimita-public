#pragma once

#include <cstdint>
#include <cstdio>
#include <chrono>
#include <cstring>

// Forward declarations
struct PerfBudgetConfig;
extern PerfBudgetConfig gPerfBudget;
struct PerfFrame;

struct PerfTimes {
    double input = 0.0;
    double setup = 0.0;
    double audio = 0.0;
    double state = 0.0;
    double camera = 0.0;
    double combat = 0.0;
    double ui = 0.0;

    double physics = 0.0;
    double collision = 0.0;
    double movement = 0.0;
    double sweepSlide = 0.0;
    double depenetration = 0.0;
    double groundDetection = 0.0;
    double chunkQuery = 0.0;
    double broadphase = 0.0;
    double narrowphase = 0.0;
    double triangleTests = 0.0;
    double charVsWorld = 0.0;
    double charVsChar = 0.0;
    double weaponCollisions = 0.0;

    double npcUpdate = 0.0;
    double npcThink = 0.0;
    double npcPathfinding = 0.0;
    double npcTargetAcq = 0.0;
    double npcVision = 0.0;
    double npcMovement = 0.0;
    double npcWeapon = 0.0;
    double npcCombat = 0.0;
    double npcCollision = 0.0;
    double npcAnimation = 0.0;
    double npcRender = 0.0;
    double npcSpawn = 0.0;
    double npcAi = 0.0;

    double weapons = 0.0;
    double projectiles = 0.0;
    double projectileCollision = 0.0;
    double projectileExplosion = 0.0;
    double shotgun = 0.0;
    double hitfxTotal = 0.0;
    double hitfxSpawn = 0.0;
    double hitfxUpdate = 0.0;
    double particleSpawn = 0.0;
    double respawn = 0.0;
    double npcSpawnTime = 0.0;
    double npcDestroyTime = 0.0;
    double audioTime = 0.0;

    double particles = 0.0;
    double damageNumbers = 0.0;
    double blood = 0.0;

    double replay = 0.0;
    double replayTick = 0.0;
    double replaySeek = 0.0;
    double replayInterp = 0.0;
    double replayAudio = 0.0;
    double replayCamera = 0.0;
    double replayRender = 0.0;
    double replayGui = 0.0;
    double replayCollision = 0.0;
    double replayEffects = 0.0;
    double replayExport = 0.0;

    double networking = 0.0;

    double rendering = 0.0;
    double shadowRender = 0.0;
    double postFX = 0.0;
    double gpuSubmit = 0.0;
    double bufferUploads = 0.0;
    double textureUploads = 0.0;
    double vboUpdates = 0.0;
    double drawCalls = 0.0;
    double worldRender = 0.0;
    double playerRender = 0.0;
    double weaponRender = 0.0;
    double effectRender = 0.0;
    double healthbars = 0.0;
    double crosshair = 0.0;
    double killfeed = 0.0;

    double mapTraversal = 0.0;
    double worldCulling = 0.0;

    double replayHud = 0.0;
    double menus = 0.0;
    double debugOverlay = 0.0;
    double consoleTime = 0.0;
    double fontRender = 0.0;
    double textLayout = 0.0;
    double jsonTime = 0.0;

    double memoryAlloc = 0.0;
    double stringFormat = 0.0;
    double fileIO = 0.0;
    double debugLogging = 0.0;

    double frameOverhead = 0.0;
    double sleepTime = 0.0;
    double swapTime = 0.0;
    double terminalTime = 0.0;
    double menusTime = 0.0;
    double devOverlay = 0.0;
    double diagTime = 0.0;

    double total = 0.0;

    int chunkCellsVisited = 0;
    int uniqueTriangles = 0;
    int broadphaseQueries = 0;
    int repeatedQueries = 0;
    int triangleTestsCount = 0;
    int collisionPairs = 0;

    int npcCount = 0;
    int npcThinkCount = 0;

    int effectsAlive = 0;
    int damageNumbersAlive = 0;
    int particleCount = 0;

    int visibleMeshes = 0;
    int visibleTriangles = 0;
    int hiddenMeshes = 0;
    int totalDrawCalls = 0;
    int frustumCulled = 0;
    int occlusionCulled = 0;
    int shaderSwitches = 0;
    int textureBinds = 0;
};

struct SpikeInfo {
    double frameTimeMs = 0.0;
    double worstSubsystemMs = 0.0;
    char subsystemName[32] = {};
    int npcCount = 0;
    int frameNumber = 0;
    double replayMemoryMb = 0.0;
};

struct CollisionQueryRecord {
    int queryId = 0;
    int frame = 0;
    int tick = 0;
    char caller[64];
    char reason[64];
    char entity[64];
    char object[64];
    double aabbMin[3] = {};
    double aabbMax[3] = {};
    int chunkCells = 0;
    int uniqueTriangles = 0;
    double elapsedMs = 0.0;
};

struct FrameSpikeReport {
    int frameNumber = 0;
    double totalFrameMs = 0.0;
    double avgFrameMs = 0.0;
    int npcCount = 0;
    int playerCount = 0;
    int effectsAlive = 0;
    int damageNumbersAlive = 0;
    int particleCount = 0;
    int drawCalls = 0;
    int visibleMeshes = 0;
    int visibleTriangles = 0;
    int chunkCellsVisited = 0;
    int uniqueTriangles = 0;
    int repeatedQueryCount = 0;
    double estimatedCacheSavingsMs = 0.0;

    static constexpr int MAX_CONTRIBUTORS = 24;
    int contributorCount = 0;
    struct { const char* name; double exclMs; double inclMs; } contributors[MAX_CONTRIBUTORS];

    CollisionQueryRecord largestQuery;

    struct DuplicateQueryInfo {
        const char* caller = "?";
        int count = 0;
        double wastedMs = 0.0;
        bool couldCache = false;
        const char* reason = "?";
    };
    static constexpr int MAX_DUPS = 16;
    int dupCount = 0;
    DuplicateQueryInfo dups[MAX_DUPS];
};

struct SubsystemTimer {
    const char* name;
    uint64_t startUs;
};

struct LargeAABBAlert {
    int frame = 0;
    const char* caller = "?";
    const char* entity = "?";
    const char* object = "?";
    const char* reason = "?";
    int chunkCells = 0;
    int uniqueTriangles = 0;
    double aabbSize[3] = {};
    const char* suspectedCause = "?";
};

struct PerfState {
    bool showPerfReport = false;
    bool showGraph = false;
    bool showNpcPerf = false;
    bool showMemory = false;
    bool showSpikes = false;
    bool renderStats = false;
    bool allocAudit = false;
    bool audioAudit = false;
    bool perfFileLogging = false;
    bool showLargeAabb = false;
    bool showCollQueries = false;
    bool showEntityCounts = false;

    int preset = 0;

    PerfTimes current;
    PerfTimes children;

    double avgFrameTimeMs = 0.0;
    double avgFrameCount = 0.0;

    SpikeInfo lastSpike;
    FrameSpikeReport lastSpikeReport;

    float frameHistory[300] = {};
    int frameHistoryCount = 0;

    bool benchmarkRunning = false;
    double benchmarkStartWall = 0.0;
    double benchmarkDuration = 60.0;
    double benchmarkFrameTimes[3600] = {};
    int benchmarkFrameCount = 0;

    bool stressRunning = false;
    bool combatTestRunning = false;
    int stressTotalNpcs = 0;
    double stressTimer = 0.0;

    int drawCalls = 0;
    int triangles = 0;
    int playerCount = 0;
    int npcCount = 0;
    int bloodCount = 0;
    int particleCount = 0;
    int projectileCount = 0;

    double netBytesIn = 0.0;
    double netBytesOut = 0.0;
    double snapshotBuildMs = 0.0;
    double serializeMs = 0.0;
    double receiveMs = 0.0;

    double replayMemoryMb = 0.0;

    int allocationsThisFrame = 0;
    uint64_t totalAllocations = 0;
    int assetLoadsThisFrame = 0;

    bool soundLoadsDetected = false;
    int soundLoadCount = 0;

    int frameNumber = 0;
    double gameTime = 0.0;

    int totalNpcsSpawned = 0;
    int totalNpcsDestroyed = 0;
    float peakNpcCount = 0.0f;
    int effectCount = 0;
    int audioSourceCount = 0;
    int corpseCount = 0;

    static constexpr int MAX_NPC_PROFILE = 64;
    int npcProfileCount = 0;
    struct NpcProfile {
        unsigned int id = 0;
        double thinkMs = 0.0;
        double pathfindingMs = 0.0;
        double targetAcqMs = 0.0;
        double visionMs = 0.0;
        double movementMs = 0.0;
        double weaponMs = 0.0;
        double combatMs = 0.0;
        double collisionMs = 0.0;
        double animationMs = 0.0;
        double renderMs = 0.0;
        double totalMs = 0.0;
    };
    NpcProfile npcProfiles[MAX_NPC_PROFILE];

    // Timer stack for exclusive/inclusive tracking
    static constexpr int MAX_TIMER_STACK = 64;
    SubsystemTimer timerStack[MAX_TIMER_STACK];
    int timerStackDepth = 0;

    // Collision query tracking
    int queryIdCounter = 0;
    static constexpr int MAX_QUERY_RECORDS = 256;
    CollisionQueryRecord queryRecords[MAX_QUERY_RECORDS];
    int queryRecordCount = 0;

    // Large AABB alerts
    LargeAABBAlert largeAabbAlert;

    // Duplicate query tracking
    struct DupTracker {
        char caller[64];
        int count;
        double totalMs;
    };
    static constexpr int MAX_DUP_TRACKERS = 32;
    DupTracker dupTrackers[MAX_DUP_TRACKERS];
    int dupTrackerCount = 0;
};

namespace Perf {

PerfState& state();

void beginFrame();
void endFrame(float currentFrameMs = 0.0f);

void renderOverlay();

struct ScopedTimer {
    const char* name;
    uint64_t startUs;
    ScopedTimer(const char* n);
    ~ScopedTimer();
};

void addTime(const char* name, double ms);
void addChildTime(const char* name, double ms);

void detectSpike(double currentFrameMs);
void writeProfileToFile();
void writeSpikeReport(const FrameSpikeReport& report);
void recordCollisionQuery(const char* caller, const char* reason,
    const char* entity, const char* object,
    const float* aabbMin, const float* aabbMax,
    int chunkCells, int triangles, double ms);
void checkLargeAABB(const char* caller, const char* entity, const char* object,
    const char* reason, int chunkCells, int triangles,
    const float* aabbMin, const float* aabbMax);
void trackDuplicateQuery(const char* caller, double ms);

void togglePerfReport();
void toggleGraph();
void toggleNpcPerf();
void toggleMemory();
void toggleSpikes();
void toggleRenderStats();
void toggleAllocAudit();
void toggleAudioAudit();
void togglePerfFileLogging();
void toggleLargeAabb();
void toggleCollQueries();
void toggleEntityCounts();
void printTopExclusive(int n);
void printCollQuerySummary();
void printEntityCounts();
void setPreset(int p);
void startBenchmark(double seconds);
void startStress(int npcTarget);
void startCombatTest();
void exportReport(const char* path);
void printSuggestions();

void applyPreset(int p);

void generateSuggestions(char* buf, int bufSize);

void collectNpcProfile(unsigned int npcId, const char* category, double ms);
void flushNpcProfiles();

double exclusive(const PerfTimes& t, const PerfTimes& children, const char* name);

} // namespace Perf

void registerPerfCommands();
void perfLoadBudgetConfig();
void perfResetScopes();
void perfWriteFrameBreakdown(FILE* f, const PerfFrame& frame, bool showChildren);
void perfCaptureFrame(double totalMs, double budgetMs, int frameNumber);
void perfSetCorrelation(const char* id);
void perfClearCorrelation();
