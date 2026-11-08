// 2026-10-23
/* purpose
* apply authored map blocks from config/maps/<mapId>.json to runtime geometry
* keep the GLB base mesh and the authored-block tail separable for editor rebuilds
* share the same collision rebuild path for client World and HeadlessWorld
* this file DOES NOT own the map JSON data or the editor interaction state
*/
#pragma once

#include <cstdint>
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

// Content hash of every authored field that changes collision, spawn, GLB
// placement, or generated chunk geometry. Presentation-only and entity-only
// changes are deliberately excluded so a live reload does not rebuild them.
uint64_t authoredGeometrySignature(const MapObjectiveConfig& config);

// Applies the authored block list for `mapId` on top of an already loaded GLB
// world. Repeated calls rebuild only the authored tail.
bool applyAuthoredMapGeometry(World& world, const std::string& mapId,
                              const AuthoredGeometryOptions& options = {});

// Applies authored blocks to a headless server world. The server passes a
// pristine GLB-only base each time so live rebuilds never duplicate the tail.
bool applyAuthoredMapGeometry(MimitaNet::HeadlessWorld& world, const std::string& mapId);

// A replicated map solid supplies its own rendered/collidable surface after
// activation. Save and hide only that GLB node's source triangle range.
bool hideDestructibleMapNode(World& world, int nodeIndex,
                            unsigned int* textureHandle = nullptr);
bool restoreDestructibleMapNode(World& world, int nodeIndex);

// Removes authored geometry from a World and restores the GLB base counts.
void clearAuthoredMapGeometry(World& world);

// Derives the map id from an asset path (for example assets/maps/funworld3.glb).
std::string mapIdFromAssetPath(const std::string& path);

// Deterministic self-test for authored block mesh/collision application.
bool authoredMapGeometrySelfTest(std::string& report);
