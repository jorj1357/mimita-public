#pragma once

#include "dev-types.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

class DevCommandRegistry {
public:
    static DevCommandRegistry& instance();
    
    void registerCommand(const DevCommand& cmd);
    bool execute(const std::string& name, const std::vector<std::string>& args);
    const DevCommand* find(const std::string& name) const;
    void listAll(std::vector<const DevCommand*>& out) const;
    const std::unordered_map<std::string, DevCommand>& allCommands() const { return mCommands; }
    
private:
    DevCommandRegistry() = default;
    std::unordered_map<std::string, DevCommand> mCommands;
};
