#pragma once

#include <string>
#include <vector>

struct MapCatalogEntry
{
    std::string displayName;
    std::string assetPath;
};

struct MapCatalogResult
{
    std::vector<MapCatalogEntry> maps;
    std::string scanRoot;
    std::string status;
};

MapCatalogResult scanMapCatalog(const std::string& assetDirectory = "assets/maps");
