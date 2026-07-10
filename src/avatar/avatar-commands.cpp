#include "avatar-commands.h"
#include "avatar.h"
#include "character-registry.h"

#include "devtools/terminal.h"
#include "entities/player.h"
#include "config/player-settings.h"
#include "tinygltf/tiny_gltf.h"

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

    // ── glbuvinfo: report UV data from a GLB model ────────────────────
    t.registerCommand({
        "glbuvinfo",
        "Report UV layout from a GLB model",
        "glbuvinfo [path]",
        [&player](const std::vector<std::string>& args) {
            std::string path;
            if (!args.empty()) {
                path = args[0];
            } else if (AvatarSystem::instance().hasAvatar()) {
                path = AvatarSystem::instance().current().getPlayerModel();
                if (path.empty()) {
                    // Check if a character model was loaded via CharacterRegistry
                    path = GetPlayerSettings().characterName.empty()
                        ? "assets/entity/player/default/mimita-char-no-animations-v4.glb"
                        : ("Characters/" + GetPlayerSettings().characterName + "/character.glb");
                }
            } else {
                path = "assets/entity/player/default/mimita-char-no-animations-v4.glb";
            }

            if (!std::filesystem::exists(path)) {
                Terminal::instance().addLog("[GLBUV] File not found: " + path);
                return;
            }

            tinygltf::TinyGLTF loader;
            tinygltf::Model model;
            std::string err, warn;
            bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, path);
            if (!ok) {
                Terminal::instance().addLog("[GLBUV] Failed to load: " + path);
                if (!err.empty()) Terminal::instance().addLog("  " + err);
                return;
            }
            if (!warn.empty()) Terminal::instance().addLog("[GLBUV] Warning: " + warn);

            printf("\n[GLBUV] path=%s\n", path.c_str());
            printf("[GLBUV] nodes=%zu meshes=%zu materials=%zu\n",
                   model.nodes.size(), model.meshes.size(), model.materials.size());

            int totalPrimitives = 0;
            int totalPosVerts = 0;
            int totalUvVerts = 0;
            int missingUvCount = 0;

            for (size_t mi = 0; mi < model.meshes.size(); ++mi) {
                const auto& mesh = model.meshes[mi];
                for (size_t pi = 0; pi < mesh.primitives.size(); ++pi) {
                    const auto& prim = mesh.primitives[pi];
                    totalPrimitives++;

                    auto posIt = prim.attributes.find("POSITION");
                    auto uvIt  = prim.attributes.find("TEXCOORD_0");

                    int posCount = 0, uvCount = 0;
                    if (posIt != prim.attributes.end()) {
                        const auto& acc = model.accessors[posIt->second];
                        posCount = (int)acc.count;
                        totalPosVerts += posCount;
                    }
                    if (uvIt != prim.attributes.end()) {
                        const auto& acc = model.accessors[uvIt->second];
                        uvCount = (int)acc.count;
                        totalUvVerts += uvCount;
                    } else {
                        missingUvCount++;
                    }

                    printf("[GLBUV]   mesh=%zu prim=%zu POS=%d TEXCOORD_0=%d%s\n",
                           mi, pi, posCount, uvCount,
                           uvIt == prim.attributes.end() ? " MISSING_UV" : "");

                    // Report first few UV values
                    if (uvIt != prim.attributes.end()) {
                        const auto& acc = model.accessors[uvIt->second];
                        const auto& bv = model.bufferViews[acc.bufferView];
                        const auto& buf = model.buffers[bv.buffer];
                        const unsigned char* base = buf.data.data() + bv.byteOffset + acc.byteOffset;
                        int stride = acc.ByteStride(bv);
                        float minU = 1e10f, maxU = -1e10f, minV = 1e10f, maxV = -1e10f;
                        int outsideCount = 0;
                        int printCount = std::min(10, (int)acc.count);
                        printf("[GLBUV]     first %d UVs:\n", printCount);
                        for (int vi = 0; vi < (int)acc.count; ++vi) {
                            const float* f = (const float*)(base + vi * stride);
                            float u = f[0], v = f[1];
                            if (vi < printCount)
                                printf("[GLBUV]       [%d] u=%.4f v=%.4f\n", vi, u, v);
                            minU = std::min(minU, u); maxU = std::max(maxU, u);
                            minV = std::min(minV, v); maxV = std::max(maxV, v);
                            if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
                                outsideCount++;
                        }
                        printf("[GLBUV]     uv_range=(%.4f,%.4f)..(%.4f,%.4f)\n",
                               minU, minV, maxU, maxV);
                        printf("[GLBUV]     outside_0..1=%d\n", outsideCount);
                    }
                }
            }

            printf("[GLBUV] total primitives=%d pos_vertices=%d uv_vertices=%d missing_uv=%d\n",
                   totalPrimitives, totalPosVerts, totalUvVerts, missingUvCount);

            // Current avatar info
            if (AvatarSystem::instance().hasAvatar()) {
                const auto& av = AvatarSystem::instance().current();
                printf("[GLBUV] current_avatar=%s texture_mode=%s\n",
                       av.name.c_str(), av.textureMode.c_str());
                if (av.textureMode == "uv_atlas") {
                    printf("[GLBUV]   atlas=%s alpha=%s cutoff=%.2f unlit=%d\n",
                           av.atlasPath.c_str(), av.alphaMode.c_str(),
                           av.alphaCutoff, (int)av.unlit);
                    int texW = 0, texH = 0;
                    glBindTexture(GL_TEXTURE_2D, AvatarSystem::instance().uvAtlasTexture());
                    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texW);
                    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texH);
                    printf("[GLBUV]   atlas_gl_size=%dx%d\n", texW, texH);
                }
            }

            Terminal::instance().addLog("[GLBUV] Report printed to console");
        },
        std::string(),
        CommandCategory::Debug
    });

    // ── avatar_render_info: report player render batch details ──────
    t.registerCommand({
        "avatar_render_info",
        "Report player render batch details",
        "avatar_render_info",
        [&player](const std::vector<std::string>&) {
            printf("\n[AVATAR RENDER INFO]\n");
            printf("  modelLoaded=%d\n", (int)player.modelLoaded);
            printf("  bodyParts=%zu\n", player.physicalBody.parts.size());
            printf("  partMeshes=%zu\n", player.physicalBody.partMeshes.size());

            for (size_t pi = 0; pi < player.physicalBody.parts.size() && pi < player.physicalBody.partMeshes.size(); ++pi) {
                const auto& part = player.physicalBody.parts[pi];
                const auto& mesh = player.physicalBody.partMeshes[pi];
                printf("  part[%zu]: name=%s nodeIndex=%d verts=%zu batches=%zu\n",
                       pi, part.name.c_str(), part.nodeIndex,
                       mesh.verts.size(), mesh.batches.size());
                for (size_t bi = 0; bi < mesh.batches.size(); ++bi) {
                    const auto& batch = mesh.batches[bi];
                    printf("    batch[%zu]: first=%zu count=%zu matIdx=%d doubleSided=%d texture=%u\n",
                           bi, batch.first, batch.count,
                           batch.materialIndex, (int)batch.doubleSided, batch.texture);

                    // Compute UV range for this batch
                    if (!mesh.verts.empty()) {
                        float minU = 1e10f, maxU = -1e10f, minV = 1e10f, maxV = -1e10f;
                        size_t end = std::min(batch.first + batch.count, mesh.verts.size());
                        for (size_t vi = batch.first; vi < end; ++vi) {
                            minU = std::min(minU, mesh.verts[vi].uv.x);
                            maxU = std::max(maxU, mesh.verts[vi].uv.x);
                            minV = std::min(minV, mesh.verts[vi].uv.y);
                            maxV = std::max(maxV, mesh.verts[vi].uv.y);
                        }
                        printf("    uv_range=(%.4f,%.4f)..(%.4f,%.4f)\n",
                               minU, minV, maxU, maxV);
                    }
                }
            }

            // Current OpenGL state snapshot
            printf("  GL state:\n");
            GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
            GLint cullMode = GL_BACK, frontFace = GL_CCW;
            glGetIntegerv(GL_CULL_FACE_MODE, &cullMode);
            glGetIntegerv(GL_FRONT_FACE, &frontFace);
            printf("    cull_face=%s mode=%s front_face=%s\n",
                   cullEnabled ? "enabled" : "disabled",
                   cullMode == GL_BACK ? "back" : "front",
                   frontFace == GL_CCW ? "CCW" : "CW");
            GLboolean blendEnabled = glIsEnabled(GL_BLEND);
            GLint blendSrc = 0, blendDst = 0;
            glGetIntegerv(GL_BLEND_SRC, &blendSrc);
            glGetIntegerv(GL_BLEND_DST, &blendDst);
            printf("    blend=%s src=0x%x dst=0x%x\n",
                   blendEnabled ? "enabled" : "disabled", blendSrc, blendDst);

            // Current avatar info
            if (AvatarSystem::instance().hasAvatar()) {
                const auto& av = AvatarSystem::instance().current();
                printf("  avatar=%s texture_mode=%s\n", av.name.c_str(), av.textureMode.c_str());
                if (av.textureMode == "uv_atlas") {
                    printf("    atlas=%s alpha=%s cutoff=%.2f unlit=%d doubleSided=%d\n",
                           av.atlasPath.c_str(), av.alphaMode.c_str(),
                           av.alphaCutoff, (int)av.unlit, 0);
                }
            }

            Terminal::instance().addLog("[AVATAR RENDER INFO] Report printed to console");
        },
        std::string(),
        CommandCategory::Debug
    });
}
