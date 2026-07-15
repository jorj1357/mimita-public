#pragma once

#include <string>

namespace MimitaNet {

std::string testEventJsonEscape(const std::string& value);
void emitTestEvent(const char* type, const std::string& fieldsJson = "");

} // namespace MimitaNet
