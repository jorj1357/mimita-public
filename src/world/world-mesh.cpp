// C:\important\quiet\n\mimita-public\mimita-public\src\world\world-mesh.h
// dec 18 2025
/**
 * purpose 
 * actual cpp file for HARD CODING LOGIC RENDERING the objects we have
 * NOT an obj not a mesh just render it
 * 1:1 i see it and its real and collidable
 */

#include "world-mesh.h"

static void addQuad(
    std::vector<WorldVertex>& v,
    glm::vec3 a, glm::vec3 b,
    glm::vec3 c, glm::vec3 d
) {
    v.push_back({a, {0,0}});
    v.push_back({b, {1,0}});
    v.push_back({c, {1,1}});

    v.push_back({a, {0,0}});
    v.push_back({c, {1,1}});
    v.push_back({d, {0,1}});
}

void buildWorldMesh(
    const World& world,
    std::vector<WorldVertex>& out
) {
    out.clear();

    for (const Block& b : world.blocks) {
        glm::vec3 h = b.size * 0.5f;
        glm::vec3 c = b.pos;

        glm::vec3 p000 = c + glm::vec3(-h.x,-h.y,-h.z);
        glm::vec3 p001 = c + glm::vec3(-h.x,-h.y, h.z);
        glm::vec3 p010 = c + glm::vec3(-h.x, h.y,-h.z);
        glm::vec3 p011 = c + glm::vec3(-h.x, h.y, h.z);
        glm::vec3 p100 = c + glm::vec3( h.x,-h.y,-h.z);
        glm::vec3 p101 = c + glm::vec3( h.x,-h.y, h.z);
        glm::vec3 p110 = c + glm::vec3( h.x, h.y,-h.z);
        glm::vec3 p111 = c + glm::vec3( h.x, h.y, h.z);

        // +X
        addQuad(out, p100,p101,p111,p110);
        // -X
        addQuad(out, p000,p010,p011,p001);
        // +Y
        addQuad(out, p010,p110,p111,p011);
        // -Y
        addQuad(out, p000,p001,p101,p100);
        // +Z
        addQuad(out, p001,p011,p111,p101);
        // -Z
        addQuad(out, p000,p100,p110,p010);
    }
}
