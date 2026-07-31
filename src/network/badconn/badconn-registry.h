// 07 31 2026, 15 30
/* purpose
* Declares the impairment registry used to load and validate preset blocks.
* Maps a JSON field name to a typed handler that validates and fills a preset.
* Keeps adding a new impairment to one handler + one registration line.
* Does NOT own the preset list, packet processing, or the simulator state.
*/

#pragma once

#include "network/badconn/badconn.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace badconn {

struct BadConnImpairmentHandler
{
    const char* fieldName;
    bool (*load)(const nlohmann::json& block, const char* file,
                 const char* presetId, BadConnPreset& preset);
};

const std::vector<BadConnImpairmentHandler>& badConnImpairmentHandlers();
void badConnWarnUnknownKey(const char* file, const char* presetId,
                           const char* fieldName);

} // namespace badconn
