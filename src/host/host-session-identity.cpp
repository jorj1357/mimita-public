// 2026-11-13
// Resident session identity storage.

#include "host/host-session-identity.h"

namespace mimita::host {
namespace {

struct SessionIdentityState {
    std::string mapId = "funworldv3";
    std::string coordinatorCode;
    std::string coordinatorJoinToken;
    std::string matchMode = "sandbox";
    std::string communityMode = "sandbox";
    bool passwordProtected = false;
    std::string password;
};

SessionIdentityState& state() {
    static SessionIdentityState value;
    return value;
}

}  // namespace

const std::string& sessionMapId() { return state().mapId; }

void setSessionMapId(const std::string& mapId) { state().mapId = mapId; }

const std::string& sessionCoordinatorCode() { return state().coordinatorCode; }

void setSessionCoordinatorCode(const std::string& code) {
    state().coordinatorCode = code;
}

const std::string& sessionCoordinatorJoinToken() {
    return state().coordinatorJoinToken;
}

void setSessionCoordinatorJoinToken(const std::string& joinToken) {
    state().coordinatorJoinToken = joinToken;
}

const std::string& sessionMatchMode() { return state().matchMode; }

void setSessionMatchMode(const std::string& mode) {
    if (state().matchMode != mode) {
        state().matchMode = mode;
    }
}

const std::string& sessionCommunityMode() { return state().communityMode; }

void setSessionCommunityMode(const std::string& mode) {
    if (state().communityMode != mode) {
        state().communityMode = mode;
    }
}

bool sessionPasswordProtected() { return state().passwordProtected; }

void setSessionPasswordState(bool protectedFlag, const std::string& password) {
    state().passwordProtected = protectedFlag;
    state().password = password;
}

bool sessionPasswordMatches(const char* supplied) {
    const SessionIdentityState& current = state();
    if (!current.passwordProtected) {
        return true;
    }
    if (!supplied) {
        return false;
    }
    return current.password == supplied && !current.password.empty();
}

}  // namespace mimita::host
