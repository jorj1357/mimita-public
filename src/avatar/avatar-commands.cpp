#include "avatar-commands.h"
#include "avatar.h"
#include "character-registry.h"

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
        "character.list",
        "List available characters",
        "character.list",
        [](const std::vector<std::string>&) {
            auto names = CharacterRegistry::instance().names();
            if (names.empty()) {
                Terminal::instance().addLog("[CHARACTER] No characters found in Characters/");
                return;
            }
            Terminal::instance().addLog("[CHARACTER] Available characters:");
            for (const auto& name : names)
                Terminal::instance().addLog("  " + name);
        },
        std::string(),
        CommandCategory::Player
    });

    t.registerCommand({
        "character.load",
        "Load a character by name",
        "character.load <name>",
        [&player](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: character.load <name>");
                return;
            }
            if (player.loadCharacter(args[0])) {
                GetPlayerSettings().characterName = args[0];
                SavePlayerSettings();
                Terminal::instance().addLog("[CHARACTER] Loaded: " + args[0]);
            } else {
                Terminal::instance().addLog("[ERROR] Failed to load character: " + args[0]);
            }
        },
        std::string(),
        CommandCategory::Player
    });

    t.registerCommand({
        "character.current",
        "Show the current character name",
        "character.current",
        [&player](const std::vector<std::string>&) {
            Terminal::instance().addLog("[CHARACTER] Current: " + player.characterName());
        },
        std::string(),
        CommandCategory::Player
    });

    t.registerCommand({
        "character.validate",
        "Validate all character manifests and GLB files",
        "character.validate",
        [](const std::vector<std::string>&) {
            CharacterRegistry::instance().scanAll();
            auto all = CharacterRegistry::instance().all();
            int ok = 0, fail = 0;
            for (const auto& m : all) {
                if (m.isValid()) {
                    Terminal::instance().addLog("[VALID] " + m.name + " OK");
                    ok++;
                } else {
                    Terminal::instance().addLog("[VALID] " + m.name + " FAIL: " + m.validationError());
                    fail++;
                }
            }
            Terminal::instance().addLog("[VALID] " + std::to_string(ok) + " valid, " +
                                        std::to_string(fail) + " failed");
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

    t.registerCommand({
        "avatar.reload",
        "Reload current avatar JSON and apply it",
        "avatar.reload",
        [&player](const std::vector<std::string>&) {
            auto& av = AvatarSystem::instance();
            if (!av.hasAvatar()) {
                Terminal::instance().addLog("[AVATAR] No avatar loaded. Use avatar.load <name>");
                return;
            }
            const std::string name = av.currentName();
            if (av.loadAvatar(name)) {
                av.applyToPlayer(player, true);
                Terminal::instance().addLog("[AVATAR] Reloaded: " + name);
            }
        },
        std::string(),
        CommandCategory::Player
    });

    t.registerCommand({
        "avatar.debugModel",
        "Print loaded GLB node names and avatar player_model path",
        "avatar.debugModel",
        [&player](const std::vector<std::string>&) {
            auto& av = AvatarSystem::instance();
            if (!av.hasAvatar()) {
                Terminal::instance().addLog("[AVATAR] No avatar loaded.");
                return;
            }
            Terminal::instance().addLog("[AVATAR] Avatar: " + av.currentName());
            const auto& pm = av.current().getPlayerModel();
            if (pm.empty())
                Terminal::instance().addLog("[AVATAR] player_model: (none, using default)");
            else
                Terminal::instance().addLog("[AVATAR] player_model: " + pm);

            Terminal::instance().addLog("[AVATAR] Loaded GLB body parts:");
            for (const auto& part : player.physicalBody.parts)
                Terminal::instance().addLog("  " + part.name);

            Terminal::instance().addLog("[AVATAR] All skeleton nodes:");
            for (const auto& node : player.nodes)
                Terminal::instance().addLog("  " + node.name);
        },
        std::string(),
        CommandCategory::Player
    });
}
