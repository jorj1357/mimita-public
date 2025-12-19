// C:\important\quiet\n\mimita-public\mimita-public\src\world\world-mesh.h
// dec 18 2025
/**
 * purpose 
 * do Geometry generation (CPU)
 * actual cpp file for HARD CODING LOGIC RENDERING the objects we have
 * NOT an obj not a mesh just render it
 * 1:1 i see it and its real and collidable
 */

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
// this comes after those so we have push_back working 
#include "world-mesh.h"
#include "coord.h"

static void addQuad(
    std::vector<WorldVertex>& v,
    glm::vec3 a, glm::vec3 b,
    glm::vec3 c, glm::vec3 d,
    float tex
) {
    v.push_back({a, {0,0}, tex, glm::vec3(0,1,0)});
    v.push_back({b, {1,0}, tex, glm::vec3(0,1,0)});
    v.push_back({c, {1,1}, tex, glm::vec3(0,1,0)});

    v.push_back({a, {0,0}, tex, glm::vec3(0,1,0)});
    v.push_back({c, {1,1}, tex, glm::vec3(0,1,0)});
    v.push_back({d, {0,1}, tex, glm::vec3(0,1,0)});
}

static void addSphere(
    std::vector<WorldVertex>& out,
    glm::vec3 center,
    float r,
    float tex
) {
    const int lat = 8;
    const int lon = 12;

    for (int y = 0; y < lat; y++) {
        float v0 = y / (float)lat;
        float v1 = (y+1) / (float)lat;

        float t0 = v0 * glm::pi<float>();
        float t1 = v1 * glm::pi<float>();

        for (int x = 0; x < lon; x++) {
            float u0 = x / (float)lon;
            float u1 = (x+1) / (float)lon;

            float p0 = u0 * glm::two_pi<float>();
            float p1 = u1 * glm::two_pi<float>();

            glm::vec3 a = center + r * glm::vec3(sin(t0)*cos(p0), cos(t0), sin(t0)*sin(p0));
            glm::vec3 b = center + r * glm::vec3(sin(t0)*cos(p1), cos(t0), sin(t0)*sin(p1));
            glm::vec3 c = center + r * glm::vec3(sin(t1)*cos(p1), cos(t1), sin(t1)*sin(p1));
            glm::vec3 d = center + r * glm::vec3(sin(t1)*cos(p0), cos(t1), sin(t1)*sin(p0));

            addQuad(out, a,b,c,d, tex);
        }
    }
}

void buildWorldMesh(
    const World& world,
    std::vector<WorldVertex>& out
) {
    out.clear();

    for (const Block& b : world.blocks) {
        glm::vec3 h = b.size * BLOCK_PHYS_MULT;
        glm::vec3 c = b.pos;

        glm::vec3 r = glm::radians(b.rot);

        // Blender rotation
        glm::mat3 Rb(1.0f);
        Rb = glm::mat3(glm::rotate(glm::mat4(1.0f), r.z, {0,0,1})) * Rb;
        Rb = glm::mat3(glm::rotate(glm::mat4(1.0f), r.y, {0,1,0})) * Rb;
        Rb = glm::mat3(glm::rotate(glm::mat4(1.0f), r.x, {1,0,0})) * Rb;

        // convert to engine space
        glm::mat3 C  = basisToYUp();
        glm::mat3 rot = C * Rb * glm::transpose(C);

        auto V = [&](float x, float y, float z) {
            // local in Blender
            glm::vec3 local(x, y, z);

            // convert local vector to engine basis
            glm::vec3 localEngine = basisToYUp() * local;

            // rotate in engine space
            glm::vec3 rotated = rot * localEngine;

            // translate in engine space
            glm::vec3 centerEngine = toYUp(c);

            return centerEngine + rotated;
        };

        glm::vec3 p000 = V(-h.x,-h.y,-h.z);
        glm::vec3 p001 = V(-h.x,-h.y, h.z);
        glm::vec3 p010 = V(-h.x, h.y,-h.z);
        glm::vec3 p011 = V(-h.x, h.y, h.z);
        glm::vec3 p100 = V( h.x,-h.y,-h.z);
        glm::vec3 p101 = V( h.x,-h.y, h.z);
        glm::vec3 p110 = V( h.x, h.y,-h.z);
        glm::vec3 p111 = V( h.x, h.y, h.z);

        // +X
        addQuad(out, p100,p101,p111,p110, b.tex[0]);
        // -X
        addQuad(out, p000,p010,p011,p001, b.tex[1]);
        // +Y
        addQuad(out, p010,p110,p111,p011, b.tex[2]);
        // -Y
        addQuad(out, p000,p001,p101,p100, b.tex[3]);
        // +Z
        addQuad(out, p001,p011,p111,p101, b.tex[4]);
        // -Z
        addQuad(out, p000,p100,p110,p010, b.tex[5]);
    }
    for (const Sphere& s : world.spheres) {
        addSphere(out, toYUp(s.pos), s.radius, s.tex);
    }
}
