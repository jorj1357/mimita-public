#include "profile/local-profile-system.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
using json = nlohmann::json;

namespace {

const char* PROFILES_PATH = "config/profiles.json";
const char* CURRENT_PROFILE_PATH = "config/current-profile.json";

json readJson(const char* path, const json& fallback)
{
    std::ifstream input(path);
    if (!input)
        return fallback;
    try {
        json value;
        input >> value;
        return value;
    } catch (...) {
        return fallback;
    }
}

bool writeJson(const char* path, const json& value)
{
    std::ofstream output(path, std::ios::trunc);
    if (!output)
        return false;
    output << value.dump(2) << '\n';
    return output.good();
}

}

LocalProfileSystem& LocalProfileSystem::instance()
{
    static LocalProfileSystem system;
    return system;
}

void LocalProfileSystem::init()
{
    ensureFiles();

    const json current = readJson(CURRENT_PROFILE_PATH, json::object());
    currentUsername_ = current.value("username", "");
    if (currentUsername_.empty())
        currentUsername_ = makeFallbackUsername();

    std::printf("[LOCAL PROFILE] active username=\"%s\" dev-auth-only=1\n",
                currentUsername_.c_str());
}

void LocalProfileSystem::ensureFiles()
{
    std::filesystem::create_directories("config");
    if (!std::filesystem::exists(PROFILES_PATH))
        writeJson(PROFILES_PATH, json{{"profiles", json::array()}});
    if (!std::filesystem::exists(CURRENT_PROFILE_PATH))
        writeJson(CURRENT_PROFILE_PATH, json{{"username", ""}});
}

std::string LocalProfileSystem::makeFallbackUsername() const
{
    return "player" + std::to_string((unsigned long)getpid());
}

const std::string& LocalProfileSystem::currentUsername() const
{
    return currentUsername_;
}

bool LocalProfileSystem::signIn(
    const std::string& username,
    const std::string& password)
{
    const json root = readJson(PROFILES_PATH, json{{"profiles", json::array()}});
    const json profiles = root.value("profiles", json::array());
    for (const json& profile : profiles)
    {
        if (profile.value("username", "") == username &&
            profile.value("password", "") == password)
        {
            currentUsername_ = username;
            lastError_.clear();
            writeJson(CURRENT_PROFILE_PATH, json{{"username", currentUsername_}});
            std::printf("[LOCAL PROFILE] sign in success username=\"%s\"\n",
                        currentUsername_.c_str());
            return true;
        }
    }

    lastError_ = "Invalid username or password";
    std::printf("[LOCAL PROFILE] sign in failed username=\"%s\"\n", username.c_str());
    return false;
}

const std::string& LocalProfileSystem::lastError() const
{
    return lastError_;
}
