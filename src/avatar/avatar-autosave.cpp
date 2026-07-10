#include "avatar-autosave.h"
#include "avatar.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>
#include <regex>

AvatarAutosave::AvatarAutosave() {}

void AvatarAutosave::setBasePath(const std::string& path) {
    mBasePath = path;
    mAvatarJsonPath = path + "/avatar.json";
    mBackupJsonPath = path + "/avatar.backup.json";
    mAutosavesDir = path + "/autosaves";
    mTimer = 0.0;
    mStatus = Status::Idle;
    mStatusMsg.clear();
}

void AvatarAutosave::update(float dt, std::function<bool()> saveFn) {
    if (mBasePath.empty()) return;
    mTimer += dt;
    if (mTimer >= AUTOSAVE_INTERVAL) {
        mTimer = 0.0;
        if (saveFn) {
            mStatus = Status::Saving;
            mStatusMsg = "Saving...";
            bool ok = saveFn();
            if (ok) {
                mStatus = Status::Ok;
                mLastSaveTime = std::chrono::duration<double>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                mStatusMsg = "Autosaved";
            } else {
                mStatus = Status::Failed;
                mStatusMsg = "Could not save - retrying";
            }
        }
    }
}

bool AvatarAutosave::saveNow(const AvatarDefinition& avatar) {
    if (mBasePath.empty()) return false;

    nlohmann::json j;
    avatarToJson(avatar, j);  // uses existing serialization

    std::string content = j.dump(2);

    // Atomic write: temp file → rename over primary
    std::string tmpPath = mAvatarJsonPath + ".tmp";
    if (!writeFileAtomic(tmpPath, content))
        return false;

    // Verify the temp file can be read
    {
        std::ifstream verify(tmpPath);
        if (!verify.is_open()) {
            std::error_code ec;
            std::filesystem::remove(tmpPath, ec);
            return false;
        }
        nlohmann::json test;
        try { verify >> test; }
        catch (...) {
            verify.close();
            std::error_code ec;
            std::filesystem::remove(tmpPath, ec);
            return false;
        }
        verify.close();
    }

    // Backup current file if it exists
    std::error_code ec;
    if (std::filesystem::exists(mAvatarJsonPath, ec))
        std::filesystem::rename(mAvatarJsonPath, mBackupJsonPath, ec);

    // Rename temp to primary
    std::filesystem::rename(tmpPath, mAvatarJsonPath, ec);
    if (ec) return false;

    // Ensure autosaves directory exists
    std::filesystem::create_directories(mAutosavesDir, ec);

    mLastSaveTime = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    mStatus = Status::Ok;
    mStatusMsg = "Saved locally";

    Debug::log(Debug::Category::Avatar, "[AvatarAutosave] saved: %s\n", mAvatarJsonPath.c_str());
    return true;
}

bool AvatarAutosave::recover(AvatarDefinition& out, std::string& outRecoveryMsg) {
    // Try primary file
    std::string content;
    if (readFile(mAvatarJsonPath, content)) {
        int version = getFormatVersion(content);
        if (version > 0) {
            outRecoveryMsg = "Loaded primary save";
            return true;  // parsing happens in AvatarSystem::loadAvatar
        }
    }

    // Try backup
    if (readFile(mBackupJsonPath, content)) {
        int version = getFormatVersion(content);
        if (version > 0) {
            outRecoveryMsg = "Recovered from backup";
            Debug::log(Debug::Category::Avatar, "[AvatarAutosave] recovered from backup: %s\n", mBackupJsonPath.c_str());
            return true;
        }
    }

    // Try newest autosave
    std::error_code ec;
    if (std::filesystem::exists(mAutosavesDir, ec)) {
        std::vector<std::string> saves;
        for (const auto& entry : std::filesystem::directory_iterator(mAutosavesDir, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
                saves.push_back(entry.path().string());
        }
        std::sort(saves.begin(), saves.end());
        std::reverse(saves.begin(), saves.end()); // newest first

        for (const auto& savePath : saves) {
            if (readFile(savePath, content)) {
                int version = getFormatVersion(content);
                if (version > 0) {
                    // Copy autosave to primary
                    std::filesystem::copy_file(savePath, mAvatarJsonPath, std::filesystem::copy_options::overwrite_existing, ec);
                    outRecoveryMsg = "Recovered from autosave";
                    Debug::log(Debug::Category::Avatar, "[AvatarAutosave] recovered from autosave: %s\n", savePath.c_str());
                    return true;
                }
            }
        }
    }

    outRecoveryMsg = "No recoverable save found";
    return false;
}

bool AvatarAutosave::snapshot() {
    if (mBasePath.empty() || !std::filesystem::exists(mAvatarJsonPath))
        return false;

    std::error_code ec;
    std::filesystem::create_directories(mAutosavesDir, ec);

    std::string ts = generateTimestamp();
    std::string snapPath = mAutosavesDir + "/autosave-" + ts + ".json";

    std::filesystem::copy_file(mAvatarJsonPath, snapPath,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) return false;

    mAutosaveFiles.push_back(snapPath);
    pruneAutosaves();

    Debug::log(Debug::Category::Avatar, "[AvatarAutosave] snapshot: %s\n", snapPath.c_str());
    return true;
}

double AvatarAutosave::secondsSinceLastSave() const {
    if (mLastSaveTime == 0.0) return -1.0;
    double now = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return now - mLastSaveTime;
}

std::string AvatarAutosave::projectFilePath() const {
    return mAvatarJsonPath;
}

void AvatarAutosave::pruneAutosaves() {
    while ((int)mAutosaveFiles.size() > MAX_AUTOSAVES) {
        std::error_code ec;
        std::filesystem::remove(mAutosaveFiles.front(), ec);
        mAutosaveFiles.pop_front();
    }
}

void AvatarAutosave::clearAutosaves() {
    std::error_code ec;
    for (const auto& path : mAutosaveFiles)
        std::filesystem::remove(path, ec);
    mAutosaveFiles.clear();
}

bool AvatarAutosave::writeFileAtomic(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    f.write(content.data(), content.size());
    f.flush();
    if (f.fail()) { f.close(); std::error_code ec; std::filesystem::remove(path, ec); return false; }
    f.close();
    return true;
}

bool AvatarAutosave::readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;
    auto size = f.tellg();
    if (size <= 0) return false;
    f.seekg(0);
    out.resize((size_t)size);
    f.read(&out[0], size);
    return !f.fail();
}

std::string AvatarAutosave::generateTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &local);
    return buf;
}

std::string AvatarAutosave::generateAvatarId() {
    std::random_device rd;
    std::mt19937_64 rng(rd());
    uint64_t id = rng();
    char buf[24];
    std::snprintf(buf, sizeof(buf), "local_%016llx", (unsigned long long)id);
    return buf;
}

int AvatarAutosave::getFormatVersion(const std::string& jsonContent) {
    try {
        auto j = nlohmann::json::parse(jsonContent);
        return j.value("format_version", 0);
    } catch (...) {
        return 0;
    }
}
