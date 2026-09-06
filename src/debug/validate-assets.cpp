#include "debug/validate-assets.h"

#include "debug/debug-log.h"
#include "utils/path_utils.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>

namespace {

struct AssetCheck {
    const char* category;
    const char* path;
    const char* expectedMagic;
    int magicLen;
    bool critical;
};

bool readMagic(const std::string& path, unsigned char* magic, int len)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.read((char*)magic, len);
    return f.gcount() == len;
}

bool matchMagic(unsigned char* actual, const char* expected, int len)
{
    for (int i = 0; i < len; ++i)
        if (actual[i] != (unsigned char)expected[i])
            return false;
    return true;
}

const char* magicStr(unsigned char* magic, int len)
{
    static char buf[64];
    int pos = 0;
    for (int i = 0; i < len && pos < 60; ++i)
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%02x ", magic[i]);
    buf[pos] = '\0';
    return buf;
}

}

int validateAllAssets()
{
    int passed = 0, failed = 0;

    // 9 5 2026 todo why we doing this this is like super duper hardcoded nto good
    // eventually replace, unless we dont even use this code 
    AssetCheck checks[] = {
        // Fonts
        {"font", "assets/font/noto-serif-cjk-tc-mimita-v1.fnt", "", 0, true},
        {"font", "assets/font/noto-serif-cjk-tc-mimita-v1_0.png", "\x89PNG", 4, true},
        {"font", "assets/font/noto-serif-cjk-tc-mimita-v1_1.png", "\x89PNG", 4, true},

        // Textures
        {"texture", "assets/textures/default.png", "\x89PNG", 4, true},
        {"texture", "assets/textures/cursor.png", "\x89PNG", 4, true},
        {"texture", "assets/outfits/suitv4debug.png", "\x89PNG", 4, true},
        {"texture", "assets/textures/player.png", "\x89PNG", 4, false},

        // Skyboxes
        {"skybox", "assets/skybox/blue/front.png", "\x89PNG", 4, false},
        {"skybox", "assets/skybox/space/front.png", "\x89PNG", 4, false},

        // Maps (GLB)
        {"map", "assets/maps/coolplace.glb", "glTF", 4, true},
        {"map", "assets/maps/funworld.glb", "glTF", 4, true},
        {"map", "assets/maps/mimita-duels-map-v3.glb", "glTF", 4, true},
        {"map", "assets/maps/mimita-aabb-only-interior-small-v4.glb", "glTF", 4, true},

        // Player
        {"player", "assets/entity/player/default/mimita-char-no-animations-v4.glb", "glTF", 4, true},

        // Weapons
        {"weapon", "assets/objects/weapons/mimita-revolver-v1.glb", "glTF", 4, false},
        {"weapon", "assets/objects/weapons/mimita-shotgun-v1.glb", "glTF", 4, false},

        // Shaders (exist as text, just check presence)
        {"shader", "shaders/basic.vert", "", 0, true},
        {"shader", "shaders/basic.frag", "", 0, true},
        {"shader", "shaders/post.vert", "", 0, true},

        // Configs
        {"config", "config/debug/debug-settings.json", "{", 1, false},
        {"config", "config/accounts/default.json", "{", 1, false},
        {"config", "config/postfx.json", "{", 1, false},
    };

    int numChecks = sizeof(checks) / sizeof(checks[0]);

    printf("========================================\n");
    printf(" ASSET VALIDATION REPORT\n");
    printf("========================================\n\n");

    for (int i = 0; i < numChecks; ++i) {
        const auto& c = checks[i];
        std::string resolved = resolveAssetPath(c.path);

        if (c.magicLen == 0) {
            // Just check file exists (text-based assets)
            std::ifstream f(resolved);
            bool ok = f.good();
            printf("  %s %s (%s)\n", ok ? "[OK]" : "[MISSING]", c.path, c.category);
            if (ok) ++passed; else ++failed;
            continue;
        }

        unsigned char magic[16];
        bool read = readMagic(resolved, magic, c.magicLen);
        bool match = read && matchMagic(magic, c.expectedMagic, c.magicLen);

        if (match) {
            printf("  [OK] %s (%s)\n", c.path, c.category);
            ++passed;
        } else {
            printf("  [FAIL] %s (%s): expected=", c.path, c.category);
            for (int j = 0; j < c.magicLen; ++j)
                printf("%02x ", (unsigned char)c.expectedMagic[j]);
            printf("got=%s\n", magicStr(magic, c.magicLen));
            ++failed;
        }
    }

    printf("\n----------------------------------------\n");
    printf("  %d passed, %d failed", passed, failed);
    if (failed > 0) printf(" *** ISSUES FOUND ***");
    printf("\n========================================\n");

    return failed;
}
