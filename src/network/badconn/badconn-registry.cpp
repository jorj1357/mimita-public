// 07 31 2026, 15 30
/* purpose
* Registers the known impairment handlers and the shared unknown-field warning.
* Wires latency, loss, reorder, and blackout blocks to their load functions.
* Keeps the registry the single place new impairment types get added.
* Does NOT parse the top-level config file or own the preset list.
*/

#include "network/badconn/badconn-registry.h"
#include "network/badconn/badconn-internal.h"
#include "debug/debug-log.h"

namespace badconn {

bool loadLatencyBlock(const nlohmann::json& block, const char* file,
                      const char* presetId, BadConnPreset& preset);
bool loadLossBlock(const nlohmann::json& block, const char* file,
                   const char* presetId, BadConnPreset& preset);
bool loadReorderBlock(const nlohmann::json& block, const char* file,
                      const char* presetId, BadConnPreset& preset);
bool loadBlackoutBlock(const nlohmann::json& block, const char* file,
                       const char* presetId, BadConnPreset& preset);

void badConnWarnUnknownKey(const char* file, const char* presetId,
                           const char* fieldName)
{
    Debug::warn(Debug::Category::Networking,
                "[BADCONN CFG] unknown field '%s' in %s preset '%s' (ignored)\n",
                fieldName, file, presetId);
}

const std::vector<BadConnImpairmentHandler>& badConnImpairmentHandlers()
{
    static const std::vector<BadConnImpairmentHandler> handlers = {
        {"latency", loadLatencyBlock},
        {"loss", loadLossBlock},
        {"reorder", loadReorderBlock},
        {"blackout", loadBlackoutBlock},
    };
    return handlers;
}

} // namespace badconn
