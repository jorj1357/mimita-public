#include "character-manifest.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

static bool readFloat(const json& j, const char* key, float& out)
{
    if (!j.contains(key) || !j[key].is_number())
        return false;
    out = j[key].get<float>();
    return true;
}

CharacterManifest CharacterManifest::defaults()
{
    CharacterManifest m;
    m.name = "DefaultGuy";
    m.description = "Default playable character";
    m.author = "MimitaTeam";
    return m;
}

bool CharacterManifest::load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        printf("[CHARACTER] manifest not found: %s\n", path.c_str());
        return false;
    }

    try
    {
        json root;
        file >> root;

        if (root.contains("name") && root["name"].is_string())
            name = root["name"].get<std::string>();
        if (root.contains("description") && root["description"].is_string())
            description = root["description"].get<std::string>();
        if (root.contains("author") && root["author"].is_string())
            author = root["author"].get<std::string>();
        if (root.contains("version") && root["version"].is_string())
            version = root["version"].get<std::string>();
        if (root.contains("model") && root["model"].is_string())
            model = root["model"].get<std::string>();
        if (root.contains("preview") && root["preview"].is_string())
            preview = root["preview"].get<std::string>();

        if (root.contains("capsule") && root["capsule"].is_object())
        {
            const auto& c = root["capsule"];
            readFloat(c, "radius", capsule.radius);
            readFloat(c, "height", capsule.height);
        }

        if (root.contains("camera") && root["camera"].is_object())
        {
            const auto& c = root["camera"];
            readFloat(c, "distance", camera.distance);
            readFloat(c, "height", camera.height);
            readFloat(c, "shoulderOffset", camera.shoulderOffset);
        }

        if (root.contains("bodyPartRadius"))
            readFloat(root, "bodyPartRadius", bodyPartRadius);
        if (root.contains("scale"))
            readFloat(root, "scale", scale);

        if (root.contains("hidden") && root["hidden"].is_boolean())
            hidden = root["hidden"].get<bool>();

        if (root.contains("tags") && root["tags"].is_array())
            tags = root["tags"].get<std::vector<std::string>>();
        if (root.contains("cosmetics") && root["cosmetics"].is_array())
            cosmetics = root["cosmetics"].get<std::vector<std::string>>();
        if (root.contains("skins") && root["skins"].is_array())
            skins = root["skins"].get<std::vector<std::string>>();

        return true;
    }
    catch (const std::exception& e)
    {
        printf("[CHARACTER] manifest parse error: %s\n", e.what());
        return false;
    }
}

bool CharacterManifest::isValid() const
{
    return !name.empty() && capsule.radius > 0.0f && capsule.height > 0.0f;
}

std::string CharacterManifest::validationError() const
{
    if (name.empty())
        return "Character name is empty";
    if (capsule.radius <= 0.0f)
        return "Capsule radius must be > 0";
    if (capsule.height <= 0.0f)
        return "Capsule height must be > 0";
    return {};
}
