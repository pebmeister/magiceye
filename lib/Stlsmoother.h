#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <cmath>
#include "Laplace.h"   // your Laplace smoothing header
#include "stl.h"       // your STL class

using glm::vec3;
using Tri = glm::ivec3;

// --- Deduplication helpers ---
struct Vec3Hash {
    size_t operator()(const vec3& v) const noexcept
    {
        auto h1 = std::hash<float>()(v.x);
        auto h2 = std::hash<float>()(v.y);
        auto h3 = std::hash<float>()(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

struct Vec3Equal {
    bool operator()(const vec3& a, const vec3& b) const noexcept
    {
        return glm::all(glm::epsilonEqual(a, b, 1e-6f));
    }
};

// --- Convert STL -> Mesh ---
static void buildMeshFromSTL(const stl& model,
    std::vector<vec3>& V,
    std::vector<Tri>& F)
{
    std::unordered_map<vec3, int, Vec3Hash, Vec3Equal> uniqueVerts;
    V.clear();
    F.clear();

    for (uint32_t t = 0; t < model.m_num_triangles; ++t) {
        int idx[3];
        for (int k = 0; k < 3; ++k) {
            int base = (t * 9) + (k * 3);
            vec3 v(model.m_vectors[base + 0],
                model.m_vectors[base + 1],
                model.m_vectors[base + 2]);

            auto it = uniqueVerts.find(v);
            if (it == uniqueVerts.end()) {
                int newIdx = (int)V.size();
                V.push_back(v);
                uniqueVerts[v] = newIdx;
                idx[k] = newIdx;
            }
            else {
                idx[k] = it->second;
            }
        }
        F.emplace_back(idx[0], idx[1], idx[2]);
    }
}

// --- Put smoothed mesh back into STL ---
static void updateSTLFromMesh(stl& model,
    const std::vector<vec3>& V,
    const std::vector<Tri>& F)
{
    for (size_t t = 0; t < F.size(); ++t) {
        const Tri& tri = F[t];
        for (int k = 0; k < 3; ++k) {
            vec3 v = V[tri[k]];
            model.m_vectors[t * 9 + k * 3 + 0] = v.x;
            model.m_vectors[t * 9 + k * 3 + 1] = v.y;
            model.m_vectors[t * 9 + k * 3 + 2] = v.z;
        }
    }
}

// --- Main smoothing wrapper ---
void smoothSTL(stl& model,
    int iterations = 20,
    bool useTaubin = true)
{
    std::vector<vec3> V;
    std::vector<Tri> F;
    buildMeshFromSTL(model, V, F);

    if (useTaubin) {
        // λ/μ smoothing (less shrinkage)
        taubinCotanSmooth(V, F, iterations, 0.5f, -0.53f, true);
    }
    else {
        // Simple umbrella smoother
        uniformSmooth(V, F, iterations, 0.4f, true);
    }

    updateSTLFromMesh(model, V, F);
}
