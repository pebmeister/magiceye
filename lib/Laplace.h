#pragma once

#include <glm/glm.hpp>  
#include <vector>  
#include <unordered_map>  
#include <unordered_set>  
#include <algorithm>  
#include <cstdint>  
#include <cmath>  

using glm::vec3;

// Triangles as indices into V  
using Tri = glm::ivec3;

// -------- Utilities --------  

static inline uint64_t edgeKey(uint32_t a, uint32_t b)
{
    if (a > b) std::swap(a, b);
    return (uint64_t(a) << 32) | uint64_t(b);
}

static inline float cotangent(const vec3& u, const vec3& v)
{
    vec3 c = glm::cross(u, v);
    float denom = glm::length(c);
    if (denom <= 1e-12f) return 0.0f;
    return glm::dot(u, v) / denom;
}

// Compute boundary vertices (edges incident to only one face)  
static void computeBoundaryVertices(
    const std::vector<Tri>& F, int nVerts, std::vector<char>& isBoundary)
{
    std::unordered_map<uint64_t, int> edgeCount;
    edgeCount.reserve(F.size() * 3);

    for (const auto& t : F) {
        int i0 = t.x, i1 = t.y, i2 = t.z;
        edgeCount[edgeKey(i0, i1)]++;
        edgeCount[edgeKey(i1, i2)]++;
        edgeCount[edgeKey(i2, i0)]++;
    }
    isBoundary.assign(nVerts, 0);
    for (const auto& kv : edgeCount) {
        if (kv.second == 1) {
            uint64_t k = kv.first;
            int a = int(k >> 32);
            int b = int(k & 0xffffffffu);
            isBoundary[a] = 1;
            isBoundary[b] = 1;
        }
    }
}

// Build uniform neighbor lists (each neighbor weight = 1)  
static void buildUniformNeighbors(
    const std::vector<Tri>& F, int nVerts,
    std::vector<std::vector<int>>& nbrs)
{
    nbrs.assign(nVerts, {});
    auto addPair = [&](int a, int b)
        {
            auto& v = nbrs[a];
            if (std::find(v.begin(), v.end(), b) == v.end()) v.push_back(b);
        };

    for (const auto& t : F) {
        int i0 = t.x, i1 = t.y, i2 = t.z;
        addPair(i0, i1); addPair(i1, i0);
        addPair(i1, i2); addPair(i2, i1);
        addPair(i2, i0); addPair(i0, i2);
    }
}

// Build cotangent weights per edge and neighbor lists  
// We clamp negative weights to zero by default (robustness).  
static void buildCotanWeights(
    const std::vector<vec3>& V,
    const std::vector<Tri>& F,
    std::vector<std::vector<std::pair<int, float>>>& nbrsW,  // (neighbor, weight)  
    std::vector<std::vector<int>>& nbrsIdx,                  // neighbor indices (for fallback)  
    bool clampNegative = true)
{
    const int n = (int)V.size();
    std::vector<std::unordered_map<int, float>> W(n);
    W.shrink_to_fit();

    // Accumulate per-triangle contributions  
    for (const auto& t : F) {
        int i0 = t.x, i1 = t.y, i2 = t.z;
        const vec3& v0 = V[i0];
        const vec3& v1 = V[i1];
        const vec3& v2 = V[i2];

        float c0 = 0.5f * cotangent(v1 - v0, v2 - v0); // opposite edge (i1,i2)  
        float c1 = 0.5f * cotangent(v2 - v1, v0 - v1); // opposite edge (i2,i0)  
        float c2 = 0.5f * cotangent(v0 - v2, v1 - v2); // opposite edge (i0,i1)  

        if (!std::isfinite(c0)) c0 = 0.0f;
        if (!std::isfinite(c1)) c1 = 0.0f;
        if (!std::isfinite(c2)) c2 = 0.0f;

        W[i1][i2] += c0; W[i2][i1] += c0;
        W[i2][i0] += c1; W[i0][i2] += c1;
        W[i0][i1] += c2; W[i1][i0] += c2;
    }

    nbrsW.assign(n, {});
    nbrsIdx.assign(n, {});
    for (int i = 0; i < n; ++i) {
        float sum = 0.0f;
        nbrsW[i].reserve(W[i].size());
        nbrsIdx[i].reserve(W[i].size());
        for (const auto& kv : W[i]) {
            int j = kv.first;
            float w = kv.second;
            if (clampNegative && w < 0.0f) w = 0.0f;
            nbrsW[i].emplace_back(j, w);
            nbrsIdx[i].push_back(j);
            sum += w;
        }
        // If a vertex has no weighted neighbors (e.g., all clamped), it's fine;  
        // we'll fall back to uniform averaging using nbrsIdx.  
    }
}

// -------- Smoothers --------  

// 1) Simple uniform Laplacian ("umbrella") smoothing.  
// Xi' = (1 - alpha) Xi + alpha * average(neighbors)  
// iterations: 5-30; alpha: 0.1 - 0.6  
void uniformSmooth(
    std::vector<vec3>& V,
    const std::vector<Tri>& F,
    int iterations,
    float alpha,
    bool fixBoundary = true)
{
    const int n = (int)V.size();
    if (n == 0 || F.empty() || iterations <= 0 || alpha <= 0.0f) return;

    std::vector<std::vector<int>> nbrs;
    buildUniformNeighbors(F, n, nbrs);

    std::vector<char> isBoundary;
    computeBoundaryVertices(F, n, isBoundary);

    const std::vector<vec3> Vfixed = V; // original positions for boundary pin  

    std::vector<vec3> Vnew(n);
    for (int it = 0; it < iterations; ++it) {
        for (int i = 0; i < n; ++i) {
            if (fixBoundary && isBoundary[i]) {
                Vnew[i] = Vfixed[i];
                continue;
            }
            const auto& N = nbrs[i];
            if (N.empty()) { Vnew[i] = V[i]; continue; }

            vec3 avg(0.0f);
            for (int j : N) avg += V[j];
            avg /= float(N.size());
            Vnew[i] = (1.0f - alpha) * V[i] + alpha * avg;
        }
        V.swap(Vnew);
    }
}

// 2) Taubin λ/μ smoothing with cotangent weights (explicit, non-shrinking).  
// Use small positive lambda (e.g., 0.5) and negative mu (e.g., -0.53).  
// iterations: ~10–50. Keeps boundaries pinned if desired.  
void taubinCotanSmooth(
    std::vector<vec3>& V,
    const std::vector<Tri>& F,
    int iterations,
    float lambda = 0.5f,
    float mu = -0.53f,
    bool fixBoundary = true,
    bool clampNegativeWeights = true)
{
    const int n = (int)V.size();
    if (n == 0 || F.empty() || iterations <= 0) return;

    // Build cotan weights once (frozen) + neighbor indices for fallback  
    std::vector<std::vector<std::pair<int, float>>> nbrsW;
    std::vector<std::vector<int>> nbrsIdx;
    buildCotanWeights(V, F, nbrsW, nbrsIdx, clampNegativeWeights);

    std::vector<char> isBoundary;
    computeBoundaryVertices(F, n, isBoundary);

    const std::vector<vec3> Vfixed = V; // keep exact boundary pin  

    std::vector<vec3> Vtmp(n), Vnew(n);

    auto smoothPass = [&](const std::vector<vec3>& Xin, std::vector<vec3>& Xout, float step)
        {
            for (int i = 0; i < n; ++i) {
                if (fixBoundary && isBoundary[i]) {
                    Xout[i] = Vfixed[i];
                    continue;
                }
                const auto& Wlist = nbrsW[i];
                const auto& Nidx = nbrsIdx[i];

                // Weighted mean by cotan weights  
                vec3 weightedSum(0.0f);
                float sumW = 0.0f;
                for (auto [j, w] : Wlist) {
                    if (w <= 0.0f) continue;
                    weightedSum += w * Xin[j];
                    sumW += w;
                }

                vec3 mean;
                if (sumW > 1e-12f) {
                    mean = weightedSum / sumW;
                }
                else {
                    // Fallback: uniform mean  
                    if (Nidx.empty()) { Xout[i] = Xin[i]; continue; }
                    vec3 avg(0.0f);
                    for (int j : Nidx) avg += Xin[j];
                    mean = avg / float(Nidx.size());
                }

                Xout[i] = Xin[i] + step * (mean - Xin[i]);
            }
        };

    for (int it = 0; it < iterations; ++it) {
        // First pass (smoothing)  
        smoothPass(V, Vtmp, lambda);
        // Second pass (inflation to counter shrinkage)  
        smoothPass(Vtmp, Vnew, mu);
        V.swap(Vnew);
    }
}
