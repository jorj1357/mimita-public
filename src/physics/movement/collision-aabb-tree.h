// 09 29 2026
/* purpose
* Own one small flat binary AABB tree used to accelerate collision pair
* discovery. The tree is built over a primitive list plus their AABBs and a
* query returns only the primitives whose own bounds overlap the query box.
* Header-only and dependency-free beyond glm/STL so it can be unit tested and
* reused by the actor narrowphase, entities, and future shape queries.
* Does NOT own collision response, contact solving, or the world mesh.
* Does NOT change contact semantics; it only prunes which pairs are tested.
*/
#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

#include "physics/physics-types.h"

struct AabbTree
{
    struct Node
    {
        AABB bounds;
        int left = -1;   // internal node child, or -1 for a leaf
        int right = -1;
        int start = 0;   // leaf: first position in `order`
        int count = 0;   // leaf: primitive count
    };

    std::vector<Node> nodes;
    std::vector<int> order;      // positions into ids/primAABBs, permuted by build
    std::vector<int> ids;        // original primitive ids (what query returns)
    std::vector<AABB> primAABBs; // parallel to ids

    bool empty() const { return nodes.empty(); }

    void clear()
    {
        nodes.clear();
        order.clear();
        ids.clear();
        primAABBs.clear();
    }

    // Build over `primitives` (ids), using `aabbs[primitive]` for bounds.
    // leafSize is the maximum primitives in a leaf.
    void build(const std::vector<int>& primitives,
               const std::vector<AABB>& aabbs,
               int leafSize = 4)
    {
        clear();
        if (primitives.empty())
            return;
        ids = primitives;
        primAABBs.resize(primitives.size());
        for (size_t i = 0; i < primitives.size(); ++i)
            primAABBs[i] = aabbs[primitives[i]];
        order.resize(primitives.size());
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = (int)i;
        nodes.reserve(order.size() * 2);
        buildRecursive(0, (int)order.size(), std::max(1, leafSize));
    }

    // Append every primitive whose own AABB overlaps `q` to `out`.
    void query(const AABB& q, std::vector<int>& out) const
    {
        if (nodes.empty())
            return;
        // Balanced median split: depth is ~log2(n/leafSize). 128 is far above
        // any real world depth and avoids a heap stack in the hot path.
        int stack[128];
        int sp = 0;
        stack[sp++] = 0;
        while (sp > 0)
        {
            const Node& n = nodes[stack[--sp]];
            if (!overlaps(q, n.bounds))
                continue;
            if (n.left < 0)
            {
                for (int i = 0; i < n.count; ++i)
                {
                    const int pos = order[n.start + i];
                    if (overlaps(q, primAABBs[pos]))
                        out.push_back(ids[pos]);
                }
            }
            else if (sp + 2 <= 128)
            {
                stack[sp++] = n.left;
                stack[sp++] = n.right;
            }
        }
    }

private:
    static bool overlaps(const AABB& a, const AABB& b)
    {
        return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
               (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
               (a.min.z <= b.max.z && a.max.z >= b.min.z);
    }

    static AABB merge(const AABB& a, const AABB& b)
    {
        return {glm::min(a.min, b.min), glm::max(a.max, b.max)};
    }

    static glm::vec3 centroid(const AABB& a) { return (a.min + a.max) * 0.5f; }

    int buildRecursive(int start, int count, int leafSize)
    {
        const int nodeIndex = (int)nodes.size();
        nodes.push_back(Node{});

        AABB bounds = primAABBs[order[start]];
        for (int i = 1; i < count; ++i)
            bounds = merge(bounds, primAABBs[order[start + i]]);
        nodes[nodeIndex].bounds = bounds;

        if (count <= leafSize)
        {
            nodes[nodeIndex].start = start;
            nodes[nodeIndex].count = count;
            return nodeIndex;
        }

        // Split on the axis with the largest centroid spread.
        AABB centroidBounds = {centroid(primAABBs[order[start]]),
                               centroid(primAABBs[order[start]])};
        for (int i = 1; i < count; ++i)
        {
            const glm::vec3 c = centroid(primAABBs[order[start + i]]);
            centroidBounds.min = glm::min(centroidBounds.min, c);
            centroidBounds.max = glm::max(centroidBounds.max, c);
        }
        const glm::vec3 extent = centroidBounds.max - centroidBounds.min;
        int axis = 0;
        if (extent.y > extent[axis]) axis = 1;
        if (extent.z > extent[axis]) axis = 2;

        const int mid = start + count / 2;
        std::nth_element(
            order.begin() + start, order.begin() + mid,
            order.begin() + start + count,
            [&](int a, int b) {
                return centroid(primAABBs[a])[axis] < centroid(primAABBs[b])[axis];
            });

        const int left = buildRecursive(start, mid - start, leafSize);
        const int right = buildRecursive(mid, start + count - mid, leafSize);
        nodes[nodeIndex].left = left;
        nodes[nodeIndex].right = right;
        return nodeIndex;
    }
};
