// 2026-11-13
// Game-side CAP_SESSION_IDENTITY helpers.

#pragma once

#include <string>

#include "host/abi/game-host-abi.h"
#include "host/abi/session-identity-capability.h"

namespace mimita::game {

void initializeHostSessionIdentity(const mimita::host::HostServicesV1* services);
std::string getSessionText(mimita::host::SessionTextField field);
void setSessionText(mimita::host::SessionTextField field,
                    const std::string& value);
std::uint32_t getSessionFlag(mimita::host::SessionFlagField field);
void setSessionFlag(mimita::host::SessionFlagField field, bool value);
void setSessionPassword(bool protectedFlag, const std::string& password);
bool sessionPasswordMatches(const char* supplied);

}  // namespace mimita::game
