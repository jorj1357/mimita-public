#pragma once

#include <string>
#include <vector>

struct MatchResult {
    int score = 0;
    std::vector<int> positions;
};

int fuzzyMatch(const std::string& pattern, const std::string& str, MatchResult& result);
