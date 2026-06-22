#pragma once

#include <string>

namespace AnalyticsEvents
{
    const char* categoryFor(const std::string& eventName);
    bool isKnown(const std::string& eventName);
}
