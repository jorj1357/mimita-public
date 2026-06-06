#include "dev-commands.h"
#include <cstdio>

DevCommandRegistry& DevCommandRegistry::instance() {
    static DevCommandRegistry sInstance;
    return sInstance;
}

void DevCommandRegistry::registerCommand(const DevCommand& cmd) {
    if (mCommands.find(cmd.name) != mCommands.end()) {
        printf("[DEV COMMANDS] Warning: command '%s' already registered, overwriting\n", cmd.name.c_str());
    }
    mCommands[cmd.name] = cmd;
    printf("[DEV COMMANDS] Registered: %s - %s\n", cmd.name.c_str(), cmd.description.c_str());
}

bool DevCommandRegistry::execute(const std::string& name, const std::vector<std::string>& args) {
    auto it = mCommands.find(name);
    if (it == mCommands.end()) {
        printf("[DEV COMMANDS] Unknown command: %s\n", name.c_str());
        return false;
    }
    
    const DevCommand& cmd = it->second;
    printf("[DEV COMMANDS] Executing: %s\n", name.c_str());
    
    try {
        cmd.handler(args);
        return true;
    } catch (const std::exception& e) {
        printf("[DEV COMMANDS] Error executing %s: %s\n", name.c_str(), e.what());
        return false;
    }
}

const DevCommand* DevCommandRegistry::find(const std::string& name) const {
    auto it = mCommands.find(name);
    return it != mCommands.end() ? &it->second : nullptr;
}

void DevCommandRegistry::listAll(std::vector<const DevCommand*>& out) const {
    out.reserve(mCommands.size());
    for (const auto& pair : mCommands) {
        out.push_back(&pair.second);
    }
}
