#include "avatar-ui-action.h"
#include "debug/debug-log.h"
#include "devtools/terminal.h"
#include "entities/player.h"

#include <cstdio>

using json = nlohmann::json;

extern Player* gpPlayer;

AvatarUiActionRegistry& AvatarUiActionRegistry::instance() {
    static AvatarUiActionRegistry reg;
    return reg;
}

void AvatarUiActionRegistry::registerAction(const std::string& name, ActionFn fn) {
    mActions[name] = std::move(fn);
}

bool AvatarUiActionRegistry::execute(const std::string& name, const json& params) {
    auto it = mActions.find(name);
    if (it == mActions.end()) {
        Debug::log(Debug::Category::Avatar, "[AvatarUI] unknown action: %s\n", name.c_str());
        return false;
    }
    Debug::log(Debug::Category::Avatar, "[AvatarUI] action: %s\n", name.c_str());
    it->second(params);
    return true;
}

bool AvatarUiActionRegistry::executeJson(const std::string& name, const std::string& paramsJson) {
    json params;
    if (!paramsJson.empty()) {
        try { params = json::parse(paramsJson); }
        catch (...) { params = json::object(); }
    }
    return execute(name, params);
}

bool AvatarUiActionRegistry::hasAction(const std::string& name) const {
    return mActions.find(name) != mActions.end();
}
