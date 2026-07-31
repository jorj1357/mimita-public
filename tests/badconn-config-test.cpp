// 07 31 2026, 15 30
/* purpose
* Tests badconn config loading, validation, unknown-field warnings, and isolation.
* Exercises missing files, malformed JSON, invalid ranges, and per-preset skipping.
* Compiles the badconn config sources directly and drives loadConfig.
* Does NOT open sockets, contact the network, or load the game engine.
* Does NOT modify game source; only writes temp JSON into build/.
*/

#include "network/badconn/badconn.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

int gPassed = 0;
int gFailed = 0;

void check(bool condition, const char* message)
{
    if (condition)
    {
        ++gPassed;
    }
    else
    {
        ++gFailed;
        std::printf("[FAIL] %s\n", message);
    }
}

std::string tempPath(const char* name)
{
    return std::string("build/badconn-config-") + name;
}

void writeTempConfig(const char* name, const std::string& json)
{
    std::ofstream out(tempPath(name), std::ios::trunc);
    out << json;
}

void testRealConfigLoads()
{
    badconn::disable();
    const bool ok = badconn::loadConfig("config/badconnconfig.json");
    check(ok, "real config loads");
    check(badconn::presets().size() >= 5, "real config has at least 5 presets");

    bool foundHighLatency = false;
    for (const badconn::BadConnPreset& preset : badconn::presets())
    {
        if (preset.name == "High latency" && preset.latency.enabled &&
            preset.latency.minMs == 250 && preset.latency.maxMs == 999)
            foundHighLatency = true;
    }
    check(foundHighLatency, "real config contains the high-latency preset");

    bool foundBlackout = false;
    for (const badconn::BadConnPreset& preset : badconn::presets())
    {
        if (preset.name == "15 second blackout" && preset.blackout.enabled &&
            preset.blackout.minMs == 15000 && preset.blackout.maxMs == 15000)
            foundBlackout = true;
    }
    check(foundBlackout, "real config contains the 15s blackout preset");
}

void testMissingFileFailsSafe()
{
    badconn::disable();
    const bool ok = badconn::loadConfig("build/does-not-exist-badconn.json");
    check(!ok, "missing file returns false");
    check(badconn::presets().empty(), "missing file leaves no presets");
    check(!badconn::active(), "missing file leaves nothing active");
}

void testMalformedJsonFailsSafe()
{
    writeTempConfig("malformed.json", "{ this is not json !!!");
    badconn::disable();
    const bool ok = badconn::loadConfig(tempPath("malformed.json"));
    check(!ok, "malformed json returns false");
    check(badconn::presets().empty(), "malformed json leaves no presets");
}

void testUnknownFieldsIgnored()
{
    writeTempConfig("unknown.json",
        "{\"version\":1,\"presets\":{"
        "\"1\":{\"name\":\"Unknown\",\"jitter\":{\"min\":1,\"max\":2},"
        "\"latency\":{\"min_ms\":10,\"max_ms\":20}}}}");
    badconn::disable();
    const bool ok = badconn::loadConfig(tempPath("unknown.json"));
    check(ok, "unknown field does not block loading");
    check(badconn::presets().size() == 1, "preset with unknown field still loads");
    check(badconn::presets().front().latency.enabled &&
          badconn::presets().front().latency.minMs == 10,
          "known fields in same preset still load");
}

void testInvalidRangeSkipsOnlyThatPreset()
{
    writeTempConfig("invalid.json",
        "{\"version\":1,\"presets\":{"
        "\"1\":{\"name\":\"Bad\",\"latency\":{\"min_ms\":1000,\"max_ms\":100}},"
        "\"2\":{\"name\":\"Good\",\"latency\":{\"min_ms\":10,\"max_ms\":20}}}}");
    badconn::disable();
    const bool ok = badconn::loadConfig(tempPath("invalid.json"));
    check(ok, "invalid preset does not fail the whole file");
    check(badconn::presets().size() == 1, "only the invalid preset is skipped");
    check(badconn::presets().front().name == "Good", "valid preset survives");
}

void testPercentOutOfRangeSkipped()
{
    writeTempConfig("percent.json",
        "{\"version\":1,\"presets\":{"
        "\"1\":{\"name\":\"Bad\",\"loss\":{\"min_percent\":0,\"max_percent\":150}}}}");
    badconn::disable();
    const bool ok = badconn::loadConfig(tempPath("percent.json"));
    check(ok, "percent error does not fail the whole file");
    check(badconn::presets().empty(), "bad percent preset is skipped");
}

void testMinGreaterThanMaxSkipped()
{
    writeTempConfig("reverse.json",
        "{\"version\":1,\"presets\":{"
        "\"1\":{\"name\":\"Bad\",\"loss\":{\"min_percent\":80,\"max_percent\":20}}}}");
    badconn::disable();
    badconn::loadConfig(tempPath("reverse.json"));
    check(badconn::presets().empty(), "min>max preset is skipped");
}

void testUnsupportedVersionFailsSafe()
{
    writeTempConfig("version.json", "{\"version\":99,\"presets\":{}}");
    badconn::disable();
    const bool ok = badconn::loadConfig(tempPath("version.json"));
    check(!ok, "unsupported version returns false");
    check(badconn::presets().empty(), "unsupported version leaves no presets");
}

void testPresetNotActivatedOnLoad()
{
    writeTempConfig("default.json",
        "{\"version\":1,\"presets\":{"
        "\"1\":{\"name\":\"A\",\"latency\":{\"min_ms\":10,\"max_ms\":20}}}}");
    badconn::disable();
    badconn::loadConfig(tempPath("default.json"));
    check(!badconn::active(), "loading config never activates a preset");
}

} // namespace

int main()
{
    testRealConfigLoads();
    testMissingFileFailsSafe();
    testMalformedJsonFailsSafe();
    testUnknownFieldsIgnored();
    testInvalidRangeSkipsOnlyThatPreset();
    testPercentOutOfRangeSkipped();
    testMinGreaterThanMaxSkipped();
    testUnsupportedVersionFailsSafe();
    testPresetNotActivatedOnLoad();

    std::printf("[badconn-config-test] passed=%d failed=%d\n", gPassed, gFailed);
    return gFailed > 0 ? 1 : 0;
}
