// 07 21 2026, 17 20
/* purpose
* Owns process-test event JSON line emission helpers.
* Escapes event field strings and writes complete event lines to stdout.
* Keeps harness-visible lifecycle evidence machine parseable under threaded logs.
* Does NOT own gameplay state, network packet schemas, or test pass criteria.
* Does NOT suppress normal server/client diagnostics.
* Does NOT replace centralized runtime debug logging.
*/

#include "network/test-events.h"

#include <cstdio>

namespace MimitaNet {

std::string testEventJsonEscape(const std::string& value)
{
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value)
    {
        switch (c)
        {
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

void emitTestEvent(const char* type, const std::string& fieldsJson)
{
    if (!type || !*type)
        return;
    std::string line = "[MIMITA_TEST_EVENT] {\"type\":\"";
    line += type;
    line += "\"";
    if (!fieldsJson.empty())
    {
        line += ",";
        line += fieldsJson;
    }
    line += "}";
    std::printf("%s\n", line.c_str());
    std::fflush(stdout);
}

} // namespace MimitaNet
