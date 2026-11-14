// 2026-11-13
// Resident single owner for server session identity/policy.
//
// The EXE server and the game generation must read the same map id, coordinator
// code/token, and private-server password state. This module owns the only copy.

#pragma once

#include <string>

namespace mimita::host {

const std::string& sessionMapId();
void setSessionMapId(const std::string& mapId);

const std::string& sessionCoordinatorCode();
void setSessionCoordinatorCode(const std::string& code);

const std::string& sessionCoordinatorJoinToken();
void setSessionCoordinatorJoinToken(const std::string& joinToken);

const std::string& sessionMatchMode();
void setSessionMatchMode(const std::string& mode);

const std::string& sessionCommunityMode();
void setSessionCommunityMode(const std::string& mode);

bool sessionPasswordProtected();
void setSessionPasswordState(bool protectedFlag, const std::string& password);
bool sessionPasswordMatches(const char* supplied);

}  // namespace mimita::host
