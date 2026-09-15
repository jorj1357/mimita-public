// 09 15 2026
/* purpose
* Implements the transitional menu-shell compatibility bridge. Projects typed
* cold profile/version/connection data into generic MenuShellState. Does NOT own
* auth/account state.
*/
#include "gui/hud/menu-shell-bridge.h"

#include <cstdint>
#include <cstdio>

#include "auth/auth-system.h"
#include "avatar/avatar.h"
#include "ecs/actor-entities.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-types.h"
#include "game/version.h"
#include "hot-reload/hot-ui.h"

namespace MenuShell {

void project()
{
    const EntityId entity = Ecs::ensureLocalPlayerEntity();
    if (entity == kInvalidEntityId)
        return;

    HotMenuShellStateV1 shell{};
    AuthSystem& auth = AuthSystem::instance();
    const bool signedIn = auth.state() == AuthState::Authenticated;
    std::snprintf(shell.username, sizeof(shell.username), "%s",
                  auth.displayName().c_str());
    std::snprintf(shell.version, sizeof(shell.version), "%s",
                  MIMITA_VERSION_STRING);
    std::snprintf(shell.avatar, sizeof(shell.avatar), "%s",
                  AvatarSystem::instance().currentName().c_str());
    shell.flags = signedIn ? 1u : 0u;
    shell.connection = 0;
    const auto& user = auth.user();
    shell.mmr = user.stats.currentMmr;
    shell.wins = user.stats.wins;
    shell.losses = user.stats.losses;
    shell.kills = user.stats.kills;
    shell.deaths = user.stats.deaths;
    shell.tier = user.supporterTier.empty() ? 0u : 1u;  // presentation fact
    MimitaRuntime::DynamicComponentStore::instance().write(
        entity, HOT_MENU_SHELL_COMPONENT, &shell, sizeof(shell));
}

} // namespace MenuShell
