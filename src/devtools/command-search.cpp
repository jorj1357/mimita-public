#include "command-search.h"

int fuzzyMatch(const std::string& pattern, const std::string& str, MatchResult& result) {
    result.score = 0;
    result.positions.clear();

    size_t pLen = pattern.size();
    size_t sLen = str.size();

    if (pLen == 0 || sLen == 0)
        return 0;

    if (pLen > sLen * 2)
        return 0;

    // 1. Exact match
    if (str == pattern) {
        result.positions.resize(pLen);
        for (size_t i = 0; i < pLen; i++)
            result.positions[i] = (int)i;
        result.score = 100000;
        return result.score;
    }

    // 2. Starts with
    if (sLen >= pLen) {
        bool match = true;
        for (size_t i = 0; i < pLen; i++) {
            if (pattern[i] != str[i]) { match = false; break; }
        }
        if (match) {
            result.positions.resize(pLen);
            for (size_t i = 0; i < pLen; i++)
                result.positions[i] = (int)i;
            result.score = 50000 + (int)((sLen - pLen) * 100);
            return result.score;
        }
    }

    // 3. Contains as substring (first occurrence)
    if (sLen >= pLen) {
        for (size_t start = 0; start <= sLen - pLen; start++) {
            bool match = true;
            for (size_t i = 0; i < pLen; i++) {
                if (pattern[i] != str[start + i]) { match = false; break; }
            }
            if (match) {
                result.positions.resize(pLen);
                for (size_t i = 0; i < pLen; i++)
                    result.positions[i] = (int)(start + i);
                result.score = 10000 + (int)((sLen - start) * 10 + pLen * 5);
                return result.score;
            }
        }
    }

    // 4. Fuzzy (character skip)
    int pIdx = 0;
    int totalScore = 0;
    int prevMatch = -2;
    result.positions.clear();

    for (size_t sIdx = 0; sIdx < sLen && pIdx < (int)pLen; sIdx++) {
        if (pattern[pIdx] == str[sIdx]) {
            result.positions.push_back((int)sIdx);

            if ((int)sIdx == prevMatch + 1)
                totalScore += 15;
            else
                totalScore += 5;

            if (sIdx > 0 && (str[sIdx - 1] == '_' || str[sIdx - 1] == '.'))
                totalScore += 10;

            prevMatch = (int)sIdx;
            pIdx++;
        }
    }

    if (pIdx < (int)pLen) {
        result.positions.clear();
        return 0;
    }

    totalScore += (int)((sLen - prevMatch) * 2);
    totalScore += (int)pLen * 3;
    result.score = totalScore;
    return result.score;
}
