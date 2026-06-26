#pragma once

#include <string>

bool storeSessionToken(const std::string& token);
std::string loadSessionToken();
void clearSessionToken();
