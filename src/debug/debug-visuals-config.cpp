#include "debug/debug-visuals.h"

#include <cstdio>
#include <fstream>
#include <filesystem>

#include <nlohmann/json.hpp>

static const char* DBG_CONFIG_PATH = "config/debug/debug-settings.json";

void DebugVis::loadConfig()
{
    std::ifstream file(DBG_CONFIG_PATH);
    if (!file.is_open())
    {
        printf("[DBGVIS] config not found: %s\n", DBG_CONFIG_PATH);
        return;
    }
    try
    {
        nlohmann::json j;
        file >> j;
        if (j.contains("debugVisualizationEnabled"))
            setMasterEnabled(j["debugVisualizationEnabled"].get<bool>());
        printf("[DBGVIS] config loaded: %s\n", DBG_CONFIG_PATH);
    }
    catch (const std::exception& e)
    {
        printf("[DBGVIS] config parse failed: %s\n", e.what());
    }
}

void DebugVis::saveConfig()
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::path(DBG_CONFIG_PATH).parent_path();
    if (!dir.empty())
        std::filesystem::create_directories(dir, ec);
    nlohmann::json j;
    j["debugVisualizationEnabled"] = masterEnabled();
    std::ofstream file(DBG_CONFIG_PATH);
    if (file.is_open())
    {
        file << j.dump(4) << std::endl;
        printf("[DBGVIS] config saved: %s\n", DBG_CONFIG_PATH);
    }
}
