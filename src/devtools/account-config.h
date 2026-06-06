#pragma once

#include <string>

bool LoadAccountConfig(const std::string& account);
bool SaveAccountConfig(const std::string& account);
void CreateDefaultAccountConfig();