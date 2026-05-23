// C:\important\mimita-priv-v8\src\world\world-gltf-loader.cpp
// 5 23 2026
/** purpose
 * do gltf loading for the world instead of the old json loader
 * so we cna have cones and sphres triangles etc
 * just all objects are point + line between points + faces between lines 3 dimesnions
 */

#include "world-gltf-loader.h"
#include "map/map_loader.h"

bool loadWorldFromGLB(
    World& world,
    const char* path
)
{
    printf("[WORLD GLB] loading %s\n", path);

    world.clear();

    world.mesh = loadGLB(path);

    printf(
        "[WORLD GLB] verts=%zu triangles=%zu batches=%zu\n",
        world.mesh.verts.size(),
        world.mesh.verts.size() / 3,
        world.mesh.batches.size()
    );

    if (world.mesh.verts.empty())
    {
        printf("[WORLD GLB ERROR] GLB produced no renderable vertices\n");
        return false;
    }

    return true;
}
