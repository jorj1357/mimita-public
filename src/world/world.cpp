// C:\important\quiet\n\mimita-public\mimita-public\src\map\world.cpp
// dec 16 2025
/**
 * purpose
 * get chunks for whole world 
 * so we stop doing triangle stuff forever 
 * this file runs 1 time no more just makes chunks at sstart then we use them 
 */

#include "world.h"
#include <cmath>

static glm::ivec3 chunkCoord(glm::vec3 p, float size)
{
    return glm::ivec3(
        floor(p.x / size),
        floor(p.y / size),
        floor(p.z / size)
    );
}

void World::buildFromMesh(const Mesh& mesh)
{
    for (size_t i = 0; i + 2 < mesh.verts.size(); i += 3)
    {
        Triangle t{
            mesh.verts[i+0].pos,
            mesh.verts[i+1].pos,
            mesh.verts[i+2].pos
        };

        glm::vec3 center = (t.a + t.b + t.c) / 3.0f;
        glm::ivec3 c = chunkCoord(center, chunkSize);
        chunks[c].tris.push_back(t);
    }
}

void World::getNearbyTriangles(glm::vec3 pos, std::vector<Triangle>& out) const
{
    glm::ivec3 base = chunkCoord(pos, chunkSize);

    for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
    for (int z = -1; z <= 1; z++)
    {
        glm::ivec3 c = base + glm::ivec3(x,y,z);
        auto it = chunks.find(c);
        if (it != chunks.end())
        {
            out.insert(out.end(),
                it->second.tris.begin(),
                it->second.tris.end());
        }
    }
}
