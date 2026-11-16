#include "network/ice/ice-events.h"

#include <nlohmann/json.hpp>

#include "debug/structured-log.h"

namespace MimitaNet {

std::string iceEventJsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

void emitIceEvent(const char* type, const std::string& fieldsJson) {
    if (!type || !*type) {
        return;
    }
    nlohmann::json fields = fieldsJson.empty()
        ? nlohmann::json::object()
        : nlohmann::json::parse(fieldsJson, nullptr, false);
    if (fields.is_discarded() || !fields.is_object()) {
        fields = nlohmann::json::object();
    }
    fields["ice_event"] = type;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        (std::string("network.ice.") + type).c_str(), "ice",
        "ICE lifecycle event", 0, fields, __FILE__, __LINE__, __FUNCTION__);
}

}  // namespace MimitaNet
