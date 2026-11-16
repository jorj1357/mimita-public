#pragma once

#include <string>

namespace MimitaNet {
std::string iceEventJsonEscape(const std::string& value);
void emitIceEvent(const char* type, const std::string& fieldsJson = "");
}
