// JSON config parsing helper.
//
// Runtime-authored config files may contain // and /* ... */ comments. Keep
// this behavior at one parser boundary instead of making each config owner
// invent its own comment stripping rules.
#pragma once

#include <istream>

#include <nlohmann/json.hpp>

inline nlohmann::json parseJsonConfig(std::istream& input)
{
    // nlohmann/json's final argument enables comment skipping while keeping
    // strings such as "https://..." and quoted // text intact.
    return nlohmann::json::parse(input, nullptr, true, true);
}
