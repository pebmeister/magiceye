// written by Paul Baxter
#pragma once
#include <array>
#include <vector>
#include <limits>
#include <algorithm>
#include <cmath>

#include "vec3.h"

#include "stl.h"
#include "Camera.h"

// Compile-time toggle for backface culling (off by default).
// Define MAGIC_EYE_ENABLE_CULLING=1 to enable in your build settings.
#ifndef MAGIC_EYE_ENABLE_CULLING
#define MAGIC_EYE_ENABLE_CULLING 0
#endif

class DepthMapGenerator {
private:
    static constexpr float tolerance = 1e-8f;
    static constexpr float INF = std::numeric_limits<float>::infinity();

    struct CamVert {
        glm::vec3 cam; // camera-space xyz
    };

    // Clip polygon (triangle) against plane z = near using Sutherland�Hodgman in camera space
    static inline std::vector<CamVert> clipAgainstNearPlane(const std::vector<CamVert>& in, float znear)
    {
        auto inside = [&](const CamVert& v) { return v.cam.z >= znear; };
        auto intersect = [&](const CamVert& A, const CamVert& B) -> CamVert
            {
                float t = (znear - A.cam.z) / (B.cam.z - A.cam.z);
                CamVert out;
                out.cam = A.cam + t * (B.cam - A.cam);
                out.cam.z = znear;
                return out;
            };

        std::vector<CamVert> out;
        if (in.empty()) return out;

        for (size_t i = 0; i < in.size(); ++i) {
            const CamVert& curr = in[i];
            const CamVert& prev = in[(i + in.size() - 1) % in.size()];
            bool Ic = inside(curr);
            bool Ip = inside(prev);

            if (Ic && Ip) {
                out.push_back(curr);
            }
            else if (!Ic && Ip) {
                out.push_back(intersect(prev, curr));
            }
            else if (Ic && !Ip) {
                out.push_back(intersect(prev, curr));
                out.push_back(curr);
            }
            else {
                // both outside: emit nothing
            }
        }
        return out;
    }

    // Triangulate a convex polygon fan (v0, v1, ..., vn) into triangles (v0, vi, i+1)
    static inline void triangulateConvexFan(const std::vector<CamVert>& poly, std::vector<std::array<CamVert, 3>>& outTris)
    {
        if (poly.size() < 3) return;
        for (size_t i = 1; i + 1 < poly.size(); ++i) {
            outTris.push_back({ poly[0], poly[i], poly[i + 1] });
        }
    }

public:
    // Public API kept identical
    static inline std::vector<float> generate(const stl& mesh, int width, int height,
        const Camera& cam, float ortho_scale,
        float& out_zmin, float& out_zmax,
        float depth_near, float depth_far,
        float bg_separation)
    {
        std::vector<float> zbuffer(static_cast<size_t>(width) * height, INF);

        glm::vec3 right, up_cam, forward;
        cam.computeBasis(right, up_cam, forward);
        float aspect = static_cast<float>(width) / std::max(1, height);

        const size_t triCount = mesh.m_num_triangles;
        const float* vdata = mesh.m_vectors.data();

        const float znear = std::max(cam.near_plane, Camera::kEpsilon);

        for (size_t t = 0; t < triCount; ++t) {
            // Load world triangle
            glm::vec3 vworld[3] = {
                {vdata[t * 9 + 0], vdata[t * 9 + 1], vdata[t * 9 + 2]},
                {vdata[t * 9 + 3], vdata[t * 9 + 4], vdata[t * 9 + 5]},
                {vdata[t * 9 + 6], vdata[t * 9 + 7], vdata[t * 9 + 8]}
            };

            // To camera space
            CamVert vcam[3];
            for (int i = 0; i < 3; ++i) {
                glm::vec3 rel = vworld[i] - cam.position;
                vcam[i].cam.x = glm::dot(rel, right);
                vcam[i].cam.y = glm::dot(rel, up_cam);
                vcam[i].cam.z = glm::dot(rel, forward);
            }

            // Clip against near plane in camera space
            std::vector<CamVert> triPoly = { vcam[0], vcam[1], vcam[2] };
            triPoly = clipAgainstNearPlane(triPoly, znear);
            if (triPoly.size() < 3) continue;

            // Triangulate
            std::vector<std::array<CamVert, 3>> clippedTris;
            clippedTris.reserve(triPoly.size() - 2);
            triangulateConvexFan(triPoly, clippedTris);

            // Rasterize each produced triangle
            for (const auto& ctri : clippedTris) {
                processTriangle(ctri.data(), cam, aspect, ortho_scale, width, height, zbuffer);
            }
        }

        return finalizeDepthMap(zbuffer, width, height, out_zmin, out_zmax,
            depth_near, depth_far, bg_separation);
    }

private:
    // Triangle processing with perspective-correct depth interpolation
    static inline void processTriangle(const CamVert* tri_cam,
        const Camera& cam, float aspect, float ortho_scale,
        int width, int height, std::vector<float>& zbuffer)
    {
        // Project to NDC
        float ndc_x[3]{}, ndc_y[3]{};
        float zcam[3]{};
        float invz[3]{};
        bool ok[3]{};

        for (int i = 0; i < 3; ++i) {
            glm::vec3 p_for_ndc = tri_cam[i].cam;
            if (!cam.perspective) {
                // Pre-scale for orthographic to reuse projectToNDC semantics
                p_for_ndc.x /= (ortho_scale * aspect);
                p_for_ndc.y /= ortho_scale;
            }
            ok[i] = cam.projectToNDC(p_for_ndc, aspect, ndc_x[i], ndc_y[i], zcam[i]);
            if (!ok[i]) return; // clipped already by near; safety
            invz[i] = 1.0f / std::max(zcam[i], Camera::kEpsilon);
        }

#if MAGIC_EYE_ENABLE_CULLING
        // Backface culling in screen space (keep original winding visible)
        float area2 = (ndc_x[1] - ndc_x[0]) * (ndc_y[2] - ndc_y[0]) - (ndc_x[2] - ndc_x[0]) * (ndc_y[1] - ndc_y[0]);
        if (area2 > 0.0f) {
            // Cull clockwise (assuming standard convention). Remove if two-sided desired.
            return;
        }
#endif

        // NDC -> pixel
        float px[3]{}, py[3]{};
        for (int i = 0; i < 3; ++i) {
            float clx = std::max(-1.0f, std::min(1.0f, ndc_x[i]));
            float cly = std::max(-1.0f, std::min(1.0f, ndc_y[i]));
            px[i] = (clx * 0.5f + 0.5f) * (width - 1);
            py[i] = (-cly * 0.5f + 0.5f) * (height - 1);
        }

        int minx = std::max(0, static_cast<int>(std::floor(std::min({ px[0], px[1], px[2] }))));
        int maxx = std::min(width - 1, static_cast<int>(std::ceil(std::max({ px[0], px[1], px[2] }))));
        int miny = std::max(0, static_cast<int>(std::floor(std::min({ py[0], py[1], py[2] }))));
        int maxy = std::min(height - 1, static_cast<int>(std::ceil(std::max({ py[0], py[1], py[2] }))));

        float denom = (py[1] - py[2]) * (px[0] - px[2]) + (px[2] - px[1]) * (py[0] - py[2]);
        if (std::fabs(denom) < tolerance) return;
        float invDen = 1.0f / denom;

        for (int y = miny; y <= maxy; ++y) {
            for (int x = minx; x <= maxx; ++x) {
                float cx = x + 0.5f;
                float cy = y + 0.5f;

                float u = ((py[1] - py[2]) * (cx - px[2]) + (px[2] - px[1]) * (cy - py[2])) * invDen;
                float v = ((py[2] - py[0]) * (cx - px[2]) + (px[0] - px[2]) * (cy - py[2])) * invDen;
                float w = 1.0f - u - v;

                // Inside test (including edges)
                if (u < 0.0f || v < 0.0f || w < 0.0f) continue;

                // Perspective-correct depth (interpolate 1/z and invert)
                float invz_interp = u * invz[0] + v * invz[1] + w * invz[2];
                float z_interp = 1.0f / std::max(invz_interp, Camera::kEpsilon);
                if (z_interp <= cam.near_plane) continue;

                int idx = y * width + x;
                if (z_interp < zbuffer[idx]) {
                    zbuffer[idx] = z_interp;
                }
            }
        }
    }

    static inline std::vector<float> finalizeDepthMap(const std::vector<float>& zbuffer, int width, int height,
        float& out_zmin, float& out_zmax,
        float depth_near, float depth_far,
        float bg_separation)
    {
        out_zmin = INF;
        out_zmax = -INF;

        for (float z : zbuffer) {
            if (std::isfinite(z)) {
                out_zmin = std::min(out_zmin, z);
                out_zmax = std::max(out_zmax, z);
            }
        }

        // Extend background
        float extended_zmax = out_zmax + (out_zmax - out_zmin) * bg_separation;

        std::vector<float> depth(static_cast<size_t>(width) * height, 0.0f);
        if (!std::isfinite(out_zmin) || !std::isfinite(extended_zmax)) {
            return depth;
        }

        float range = extended_zmax - out_zmin;
        if (range < tolerance) range = 1.0f;

        for (int i = 0; i < width * height; ++i) {
            float z = zbuffer[i];
            if (!std::isfinite(z)) {
                depth[i] = depth_far;
            }
            else {
                float t = (z - out_zmin) / range;
                depth[i] = depth_near + (depth_far - depth_near) * t;
            }
        }

        out_zmax = extended_zmax;
        return depth;
    }
};
