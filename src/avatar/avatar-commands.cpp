#include "avatar-commands.h"
#include "avatar.h"

#include "devtools/terminal.h"
#include "entities/player.h"
#include "config/player-settings.h"

void registerAvatarCommands(Player& player) {
    Terminal& t = Terminal::instance();

    t.registerCommand({
        "avatar.create",
        "Create a new avatar folder",
        "avatar.create <name>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: avatar.create <name>");
                return;
            }
            std::string path = AvatarSystem::avatarPath(args[0]);
            std::filesystem::create_directories(path);
            AvatarSystem::instance().saveSimple(args[0], SimpleAvatar{});
            Terminal::instance().addLog("[AVATAR] Created: " + path);
        },
        std::string(),
        CommandCategory::Player
    });

    t.registerCommand({
        "avatar.list",
        "List available avatars",
        "avatar.list",
        [](const std::vector<std::string>&) {
            auto list = AvatarSystem::instance().listAvatars();
            if (list.empty()) {
                Terminal::instance().addLog("[AVATAR] No avatars found in assets/avatars/");
                return;
            }
            Terminal::instance().addLog("[AVATAR] Available avatars:");
            for (const auto& name : list)
                Terminal::instance().addLog("  " + name);
        },
        std::string(),
        CommandCategory::Player
    });

    t.registerCommand({
        "avatar.load",
        "Load and apply an avatar",
        "avatar.load <name>",
        [&player](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: avatar.load <name>");
                return;
            }
            if (AvatarSystem::instance().loadAvatar(args[0])) {
                AvatarSystem::instance().applyToPlayer(player, true);
                GetPlayerSettings().avatarName = args[0];
                SavePlayerSettings();
                Terminal::instance().addLog("[AVATAR] Loaded and applied: " + args[0]);
            }
        },
        std::string(),
        CommandCategory::Player
    });

    t.registerCommand({
        "avatar.apply",
        "Re-apply current avatar to player",
        "avatar.apply",
        [&player](const std::vector<std::string>&) {
            if (AvatarSystem::instance().hasAvatar())
                AvatarSystem::instance().applyToPlayer(player, true);
            else
                Terminal::instance().addLog("[AVATAR] No avatar loaded. Use avatar.load <name>");
        },
        std::string(),
        CommandCategory::Player
    });
}
