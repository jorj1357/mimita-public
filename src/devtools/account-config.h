#pragma once

#include <string>

struct DuelStats;

bool LoadAccountConfig(const std::string& account);
bool SaveAccountConfig(const std::string& account);
void CreateDefaultAccountConfig();

bool LoadDuelStats(const std::string& account);
bool SaveDuelStats(const std::string& account);
DuelStats& GetDuelStats();