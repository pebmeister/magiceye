// written by Paul Baxter

#pragma once
#include <vector>
#include <limits>
#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "stl.h"
#include "Camera.h"

/**
 * @class DepthMapGenerator
 * @brief A utility class for generating depth maps from 3D meshes.
 */
class DepthMapGenerator {
private:
    static constexpr float tolerance = 1e-8f;

    static inline bool barycentric2D(float px, float py, float ax, float ay, float bx, float by,
        float cx, float cy, float& u, float& v, float& w)
    {
        float denom = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
        if (std::fabs(denom) < tolerance) return false;
        u = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / denom;
        v = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / denom;
        w = 1.0f - u - v;
        return true;
    }

public:
    static inline std::vector<float> generate(const stl& mesh, int width, int height,
        const Camera& cam, float ortho_scale,
        float& out_zmin, float& out_zmax,
        float depth_near, float depth_far,
        float bg_separation)
    {
        constexpr float INF = std::numeric_limits<float>::infinity();
        std::vector<float> zbuffer(width * height, INF);

        glm::vec3 right, up_cam, forward;
        cam.computeBasis(right, up_cam, forward);
        float aspect = static_cast<float>(width) / height;

        size_t triCount = mesh.m_num_triangles;
        const float* vdata = mesh.m_vectors.data();

        for (size_t t = 0; t < triCount; ++t) {
            processTriangle(vdata + t * 9, cam, right, up_cam, forward, aspect,
                ortho_scale, width, height, zbuffer);
        }

        return finalizeDepthMap(zbuffer, width, height, out_zmin, out_zmax,
            depth_near, depth_far, bg_separation);
    }

private:
    static inline void processTriangle(const float* tri, const Camera& cam,
        const glm::vec3& right, const glm::vec3& up_cam, const glm::vec3& forward,
        float aspect, float ortho_scale, int width, int height,
        std::vector<float>& zbuffer)
    {
        glm::vec3 vworld[3] = { {tri[0], tri[1], tri[2]}, {tri[3], tri[4], tri[5]}, {tri[6], tri[7], tri[8]} };
        glm::vec3 vcam[3]{};
        float ndc_x[3]{}, ndc_y[3]{}, zcam[3]{};
        bool validProj[3]{};

        for (int i = 0; i < 3; i++) {
            glm::vec3 rel = vworld[i] - cam.position;
            vcam[i].x = glm::dot(rel, right);
            vcam[i].y = glm::dot(rel, up_cam);
            vcam[i].z = glm::dot(rel, forward);

            glm::vec3 p_for_ndc = vcam[i];
            if (!cam.perspective) {
                p_for_ndc.x /= (ortho_scale * aspect);
                p_for_ndc.y /= ortho_scale;
            }
            validProj[i] = cam.projectToNDC(p_for_ndc, aspect, ndc_x[i], ndc_y[i], zcam[i]);
        }

        if (!validProj[0] && !validProj[1] && !validProj[2]) return;

        rasterizeTriangle(ndc_x, ndc_y, zcam, width, height, zbuffer);
    }

    static inline void rasterizeTriangle(const float ndc_x[3], const float ndc_y[3], const float zcam[3],
        int width, int height, std::vector<float>& zbuffer)
    {
        float px[3]{}, py[3]{};
        for (int i = 0; i < 3; i++) {
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
        float invDenom = 1.0f / denom;

        for (int y = miny; y <= maxy; ++y) {
            for (int x = minx; x <= maxx; ++x) {
                float px_center = x + 0.5f;
                float py_center = y + 0.5f;

                float u = ((py[1] - py[2]) * (px_center - px[2]) +
                    (px[2] - px[1]) * (py_center - py[2])) * invDenom;
                float v = ((py[2] - py[0]) * (px_center - px[2]) +
                    (px[0] - px[2]) * (py_center - py[2])) * invDenom;
                float w = 1.0f - u - v;

                if (u < 0 || v < 0 || w < 0) continue;

                float z_interp = u * zcam[0] + v * zcam[1] + w * zcam[2];
                if (z_interp <= 0.0f) continue;

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
        out_zmin = std::numeric_limits<float>::infinity();
        out_zmax = -std::numeric_limits<float>::infinity();

        for (float z : zbuffer) {
            if (std::isfinite(z)) {
                out_zmin = std::min(out_zmin, z);
                out_zmax = std::max(out_zmax, z);
            }
        }

        // Extend background
        float extended_zmax = out_zmax + (out_zmax - out_zmin) * bg_separation;

        std::vector<float> depth(width * height, 0.0f);
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