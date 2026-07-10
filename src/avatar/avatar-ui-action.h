#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>

// Singleton action registry for UI-driven avatar editor actions.
// Actions are registered once and executed by ID from JSON layout elements.
class AvatarUiActionRegistry {
public:
    static AvatarUiActionRegistry& instance();

    using ActionFn = std::function<void(const nlohmann::json& params)>;

    void registerAction(const std::string& name, ActionFn fn);
    bool execute(const std::string& name, const nlohmann::json& params);

    // Convenience: parse params from a JSON string
    bool executeJson(const std::string& name, const std::string& paramsJson);

    bool hasAction(const std::string& name) const;

private:
    AvatarUiActionRegistry() = default;
    std::unordered_map<std::string, ActionFn> mActions;
};
