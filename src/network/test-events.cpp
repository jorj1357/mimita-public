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
    std::printf("[MIMITA_TEST_EVENT] {\"type\":\"%s\"", type);
    if (!fieldsJson.empty())
        std::printf(",%s", fieldsJson.c_str());
    std::printf("}\n");
    std::fflush(stdout);
}

} // namespace MimitaNet
