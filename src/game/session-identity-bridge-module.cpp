// 2026-11-13
// Game-DLL session identity bridge.

#include "game/session-identity-bridge-module.h"

namespace mimita::game {
namespace {

const mimita::host::SessionIdentityCapabilityV1* gSessionIdentity = nullptr;
void* gHostData = nullptr;

}  // namespace

void initializeHostSessionIdentity(
    const mimita::host::HostServicesV1* services) {
    gSessionIdentity = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) {
        return;
    }
    auto* capability =
        static_cast<const mimita::host::SessionIdentityCapabilityV1*>(
            services->get_capability(
                services->hostUserData,
                static_cast<std::uint32_t>(
                    mimita::host::HostCapabilityId::SessionIdentity),
                mimita::host::kSessionIdentityCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kSessionIdentityCapabilityVersion &&
        capability->structSize >=
            sizeof(mimita::host::SessionIdentityCapabilityV1)) {
        gSessionIdentity = capability;
    }
}

std::string getSessionText(mimita::host::SessionTextField field) {
    if (!gSessionIdentity || !gSessionIdentity->get_text) {
        return {};
    }
    char buffer[512] = {};
    const std::uint32_t length = gSessionIdentity->get_text(
        gHostData, static_cast<std::uint32_t>(field), buffer,
        static_cast<std::uint32_t>(sizeof(buffer)));
    if (length >= sizeof(buffer)) {
        return std::string(buffer, sizeof(buffer) - 1);
    }
    return std::string(buffer, length);
}

void setSessionText(mimita::host::SessionTextField field,
                    const std::string& value) {
    if (gSessionIdentity && gSessionIdentity->set_text) {
        gSessionIdentity->set_text(gHostData, static_cast<std::uint32_t>(field),
                                   value.c_str());
    }
}

std::uint32_t getSessionFlag(mimita::host::SessionFlagField field) {
    if (!gSessionIdentity || !gSessionIdentity->get_flag) {
        return 0;
    }
    return gSessionIdentity->get_flag(gHostData,
                                      static_cast<std::uint32_t>(field));
}

void setSessionFlag(mimita::host::SessionFlagField field, bool value) {
    if (gSessionIdentity && gSessionIdentity->set_flag) {
        gSessionIdentity->set_flag(gHostData, static_cast<std::uint32_t>(field),
                                   value ? 1u : 0u);
    }
}

void setSessionPassword(bool protectedFlag, const std::string& password) {
    if (gSessionIdentity && gSessionIdentity->set_password) {
        gSessionIdentity->set_password(gHostData, protectedFlag ? 1u : 0u,
                                       password.c_str());
    }
}

bool sessionPasswordMatches(const char* supplied) {
    if (!gSessionIdentity || !gSessionIdentity->password_matches) {
        return false;
    }
    return gSessionIdentity->password_matches(gHostData, supplied) != 0;
}

}  // namespace mimita::game
