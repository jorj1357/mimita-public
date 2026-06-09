#include "map-catalog.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <unordered_map>

#include "utils/path_utils.h"

namespace {

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return value;
}

std::string cleanDisplayName(std::string stem)
{
    bool capitalize = true;
    for (char& c : stem)
    {
        if (c == '-' || c == '_')
        {
            c = ' ';
            capitalize = true;
        }
        else if (capitalize)
        {
            c = (char)std::toupper((unsigned char)c);
            capitalize = false;
        }
    }
    return stem.empty() ? "Unnamed Map" : stem;
}

std::filesystem::path resolveDirectory(const std::string& assetDirectory)
{
    const std::filesystem::path relative(assetDirectory);
    const std::filesystem::path besideExecutable =
        std::filesystem::path(getExecutableDirectory()) / relative;

    std::error_code error;
    if (std::filesystem::is_directory(besideExecutable, error) && !error)
        return besideExecutable;
    return relative;
}

} // namespace

MapCatalogResult scanMapCatalog(const std::string& assetDirectory)
{
    MapCatalogResult result;
    const std::filesystem::path root = resolveDirectory(assetDirectory);
    result.scanRoot = root.lexically_normal().string();

    printf("[MAP SCAN] root=%s\n", result.scanRoot.c_str());

    std::error_code error;
    if (!std::filesystem::exists(root, error) || error)
    {
        result.status = "Maps folder is missing: " + result.scanRoot;
        printf("[MAP SCAN ERROR] %s\n", result.status.c_str());
        return result;
    }
    if (!std::filesystem::is_directory(root, error) || error)
    {
        result.status = "Maps path is not a directory: " + result.scanRoot;
        printf("[MAP SCAN ERROR] %s\n", result.status.c_str());
        return result;
    }

    std::filesystem::directory_iterator iterator(root, error);
    const std::filesystem::directory_iterator end;
    while (!error && iterator != end)
    {
        const std::filesystem::directory_entry& file = *iterator;
        std::error_code fileError;
        if (file.is_regular_file(fileError) && !fileError)
        {
            std::string ext = lowerCopy(file.path().extension().string());
            if (ext == ".glb" || ext == ".gltf")
            {
                MapCatalogEntry entry;
                entry.assetPath =
                    (std::filesystem::path(assetDirectory) / file.path().filename())
                        .lexically_normal().generic_string();
                entry.displayName = cleanDisplayName(file.path().stem().string());
                result.maps.push_back(std::move(entry));
            }
        }
        iterator.increment(error);
    }

    if (error)
    {
        result.status = "Map scan failed: " + error.message();
        printf("[MAP SCAN ERROR] %s\n", result.status.c_str());
        result.maps.clear();
        return result;
    }

    std::sort(result.maps.begin(), result.maps.end(),
        [](const MapCatalogEntry& a, const MapCatalogEntry& b) {
            return lowerCopy(a.assetPath) < lowerCopy(b.assetPath);
        });

    std::unordered_map<std::string, int> displayCounts;
    for (MapCatalogEntry& entry : result.maps)
    {
        const std::string key = lowerCopy(entry.displayName);
        const int duplicateIndex = ++displayCounts[key];
        if (duplicateIndex > 1)
            entry.displayName += " (" + std::to_string(duplicateIndex) + ")";
        printf("[MAP SCAN] discovered name=\"%s\" path=%s\n",
               entry.displayName.c_str(), entry.assetPath.c_str());
    }

    if (result.maps.empty())
        result.status = "No .glb maps found in " + result.scanRoot;
    else
        result.status = std::to_string(result.maps.size()) + " map(s) found";

    printf("[MAP SCAN] count=%zu status=\"%s\"\n",
           result.maps.size(), result.status.c_str());
    return result;
}
