// 2026-10-23
/* purpose
* apply authored map blocks from config/maps/<mapId>.json to runtime geometry
* keep the GLB base mesh and the authored-block tail separable for editor rebuilds
* share the same collision rebuild path for client World and HeadlessWorld
* this file DOES NOT own the map JSON data or the editor interaction state
*/
#pragma once

#include <string>
#include <vector>

#include "gamemode/map-config.h"

struct World;
namespace MimitaNet { struct HeadlessWorld; }

struct AuthoredGeometryOptions
{
    bool rebuildCollision = true;
    bool staticFaceRemoval = true;
};

// Applies GLB node translation overrides authored by the map editor.
void applyGLBObjectOverrides(World& world,
                             const std::vector<MapGLBObjectOverride>& overrides);
void applyGLBObjectOverrides(MimitaNet::HeadlessWorld& world,
                             const std::vector<MapGLBObjectOverride>& overrides);

// Applies the authored block list for `mapId` on top of an already loaded GLB
// world. Repeated calls rebuild only the authored tail.
bool applyAuthoredMapGeometry(World& world, const std::string& mapId,
                              const AuthoredGeometryOptions& options = {});

// Applies authored blocks to a headless server world (initial load only).
bool applyAuthoredMapGeometry(MimitaNet::HeadlessWorld& world, const std::string& mapId);

// Removes authored geometry from a World and restores the GLB base counts.
void clearAuthoredMapGeometry(World& world);

// Derives the map id from an asset path (for example assets/maps/funworld3.glb).
std::string mapIdFromAssetPath(const std::string& path);

// Deterministic self-test for authored block mesh/collision application.
bool authoredMapGeometrySelfTest(std::string& report);
