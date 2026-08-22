#include "avatar.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "../devtools/terminal.h"

using json = nlohmann::json;

namespace {

bool isValidFilename(const std::string& name)
{
    if (name.empty()) return false;
    const std::string forbidden = "<>:\"/\\|?*;";
    for (char c : name) {
        if (forbidden.find(c) != std::string::npos)
            return false;
        if ((unsigned char)c < 32) return false;
    }
    return true;
}

std::string sanitizeName(const std::string& name)
{
    std::string s = name;
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);
    const std::string forbidden = "<>:\"/\\|?*;";
    std::string result;
    for (char c : s) {
        if (forbidden.find(c) == std::string::npos && (unsigned char)c >= 32)
            result += c;
    }
    return result;
}

std::string uniquePath(const std::string& basePath, const std::string& filename)
{
    std::string stem = filename;
    std::string ext;
    size_t dot = filename.rfind('.');
    if (dot != std::string::npos) {
        stem = filename.substr(0, dot);
        ext = filename.substr(dot);
    }
    std::string outPath = basePath + "/" + filename;
    if (!std::filesystem::exists(outPath))
        return outPath;
    for (int counter = 1; ; ++counter) {
        outPath = basePath + "/" + stem + " (" + std::to_string(counter) + ")" + ext;
        if (!std::filesystem::exists(outPath))
            return outPath;
    }
}

} // anonymous namespace

bool AvatarSystem::importPng(const std::string& sourcePath)
{
    // TODO: rename this legacy function to importImage over time; it now accepts PNG and JPEG files.
    if (mAvatarName.empty() || !mHasAvatar) return false;
    if (!std::filesystem::exists(sourcePath)) return false;
    std::string ext = std::filesystem::path(sourcePath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".png" && ext != ".jpg" && ext != ".jpeg") return false;

    std::string destDir = avatarPath(mAvatarName);
    std::filesystem::create_directories(destDir);
    std::string destPath = uniquePath(destDir, std::filesystem::path(sourcePath).filename().string());
    std::error_code ec;
    std::filesystem::copy(sourcePath, destPath, ec);
    if (ec) {
        printf("[AVATAR] Failed to import PNG: %s\n", ec.message().c_str());
        return false;
    }
    printf("[AVATAR] Imported PNG: %s\n", destPath.c_str());
    Terminal::instance().addLog("[AVATAR] Imported: " + std::filesystem::path(destPath).filename().string());
    return true;
}

bool AvatarSystem::createOutfit(const std::string& name)
{
    std::string clean = sanitizeName(name);
    if (clean.empty() || !isValidFilename(clean)) return false;
    std::string path = avatarPath(clean);
    if (std::filesystem::exists(path)) return false;

    std::filesystem::create_directories(path);
    SimpleAvatar empty;
    bool ok = saveSimple(clean, empty);
    if (ok) {
        loadAvatar(clean);
        Terminal::instance().addLog("[AVATAR] Created outfit: " + clean);
    }
    return ok;
}

bool AvatarSystem::renameOutfit(const std::string& oldName, const std::string& newName)
{
    std::string cleanNew = sanitizeName(newName);
    if (cleanNew.empty() || !isValidFilename(cleanNew)) return false;
    if (oldName == cleanNew) return true;

    std::string oldPath = avatarPath(oldName);
    std::string newPath = avatarPath(cleanNew);
    if (!std::filesystem::exists(oldPath) || std::filesystem::exists(newPath))
        return false;

    std::error_code ec;
    std::filesystem::rename(oldPath, newPath, ec);
    if (ec) return false;

    // Update avatar.json name field
    std::string jsonPath = newPath + "/avatar.json";
    if (std::filesystem::exists(jsonPath)) {
        std::ifstream f(jsonPath);
        if (f.is_open()) {
            try {
                json root;
                f >> root;
                root["name"] = cleanNew;
                std::ofstream out(jsonPath);
                if (out.is_open()) out << root.dump(2);
            } catch (...) {}
        }
    }

    Terminal::instance().addLog("[AVATAR] Renamed outfit: " + oldName + " -> " + cleanNew);
    return true;
}

bool AvatarSystem::duplicateOutfit(const std::string& sourceName, const std::string& destName)
{
    std::string cleanDest = sanitizeName(destName);
    if (cleanDest.empty() || !isValidFilename(cleanDest)) return false;
    if (sourceName == cleanDest) return false;

    std::string srcPath = avatarPath(sourceName);
    std::string dstPath = avatarPath(cleanDest);
    if (!std::filesystem::exists(srcPath) || std::filesystem::exists(dstPath))
        return false;

    std::error_code ec;
    std::filesystem::copy(srcPath, dstPath,
        std::filesystem::copy_options::recursive |
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) return false;

    // Update name in copied avatar.json
    std::string jsonPath = dstPath + "/avatar.json";
    if (std::filesystem::exists(jsonPath)) {
        std::ifstream f(jsonPath);
        if (f.is_open()) {
            try {
                json root;
                f >> root;
                root["name"] = cleanDest;
                std::ofstream out(jsonPath);
                if (out.is_open()) out << root.dump(2);
            } catch (...) {}
        }
    }

    Terminal::instance().addLog("[AVATAR] Duplicated: " + sourceName + " -> " + cleanDest);
    return true;
}

bool AvatarSystem::deleteOutfit(const std::string& name)
{
    std::string path = avatarPath(name);
    if (!std::filesystem::exists(path)) return false;
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    if (ec) return false;

    Terminal::instance().addLog("[AVATAR] Deleted outfit: " + name);
    return true;
}

bool AvatarSystem::saveCurrentOutfit(const std::string& outfitName)
{
    std::string clean = sanitizeName(outfitName);
    if (clean.empty() || !isValidFilename(clean)) return false;

    std::string dstPath = avatarPath(clean);
    std::filesystem::create_directories(dstPath);

    // Collect referenced textures
    std::set<std::string> usedTextures;
    auto collect = [&](const FaceVector& part) {
        if (!part.front.texture.empty()) usedTextures.insert(part.front.texture);
        if (!part.back.texture.empty()) usedTextures.insert(part.back.texture);
        if (!part.left.texture.empty()) usedTextures.insert(part.left.texture);
        if (!part.right.texture.empty()) usedTextures.insert(part.right.texture);
        if (!part.top.texture.empty()) usedTextures.insert(part.top.texture);
        if (!part.bottom.texture.empty()) usedTextures.insert(part.bottom.texture);
    };
    collect(mAvatar.head);
    collect(mAvatar.torso);
    collect(mAvatar.leftArm);
    collect(mAvatar.rightArm);
    collect(mAvatar.leftLeg);
    collect(mAvatar.rightLeg);

    // Copy referenced textures
    std::string srcPath = mBasePath;
    for (const auto& tex : usedTextures) {
        std::string srcFile = srcPath + "/" + tex;
        if (std::filesystem::exists(srcFile)) {
            std::error_code ec;
            std::filesystem::copy(srcFile, dstPath + "/" + tex,
                std::filesystem::copy_options::overwrite_existing, ec);
        }
    }

    // Save the complete current definition, including cosmetics, body-part
    // overrides, and custom model settings, into the new avatar folder.
    AvatarDefinition copy = mAvatar;
    copy.name = clean;
    copy.basePath = dstPath;
    json root;
    avatarToJson(copy, root);
    const std::string tmpPath = dstPath + "/avatar.json.tmp";
    std::ofstream out(tmpPath);
    if (!out.is_open()) return false;
    out << root.dump(2);
    out.close();
    std::error_code saveEc;
    std::filesystem::rename(tmpPath, dstPath + "/avatar.json", saveEc);
    bool saved = !saveEc;
    if (saved) {
        // Copy presets
        std::string srcPresets = srcPath + "/presets";
        std::string dstPresets = dstPath + "/presets";
        if (std::filesystem::exists(srcPresets)) {
            std::error_code ec;
            std::filesystem::copy(srcPresets, dstPresets,
                std::filesystem::copy_options::recursive |
                std::filesystem::copy_options::overwrite_existing, ec);
        }
        Terminal::instance().addLog("[AVATAR] Saved outfit: " + clean);
    }
    return saved;
}
