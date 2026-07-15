#include "utils/tips.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Tips {

namespace {
std::vector<std::string> gTips;
int gLastIndex = -1;
std::mt19937 gRng((unsigned int)std::chrono::steady_clock::now().time_since_epoch().count());
bool gLoaded = false;
} // anonymous namespace

void load()
{
    if (gLoaded) return;
    gLoaded = true;

    std::ifstream file("config/tips.json");
    if (!file.is_open())
    {
        printf("[TIPS] No tips file found at config/tips.json\n");
        return;
    }

    try {
        json j;
        file >> j;
        if (!j.is_array())
        {
            printf("[TIPS] tips.json is not an array\n");
            return;
        }
        gTips.clear();
        for (const auto& item : j)
        {
            if (item.is_string())
                gTips.push_back(item.get<std::string>());
        }
        printf("[TIPS] loaded %zu tips from config/tips.json\n", gTips.size());
    }
    catch (const std::exception& e)
    {
        printf("[TIPS] failed to parse tips.json: %s\n", e.what());
    }
}

std::string getRandomTip()
{
    if (gTips.empty()) return "";

    int idx;
    if (gTips.size() == 1)
    {
        idx = 0;
    }
    else
    {
        do {
            std::uniform_int_distribution<int> dist(0, (int)gTips.size() - 1);
            idx = dist(gRng);
        } while (idx == gLastIndex);
    }
    gLastIndex = idx;
    return gTips[idx];
}

int count()
{
    return (int)gTips.size();
}

std::string getTip(int index)
{
    if (index < 0 || index >= (int)gTips.size())
        return "";
    return gTips[index];
}

} // namespace Tips
