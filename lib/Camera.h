// written by Paul Baxter
#pragma once
#include "vec3.h"
// #include <glm/glm.hpp>
// #include <glm/gtc/matrix_transform.hpp>
#include <numbers>
#include <algorithm>
#include <cmath>

class Camera {
public:
    glm::vec3 position;
    glm::vec3 look_at;
    glm::vec3 up;
    float fov_deg;
    bool perspective;

    // New: near/far planes for clipping/normalization
    float near_plane;
    float far_plane;

    static constexpr float kEpsilon = 1e-6f;

    Camera()
        : position(0, 0, 0)
        , look_at(0, 0, 0)
        , up(0, 1, 0)
        , fov_deg(45)
        , perspective(true)
        , near_plane(1e-3f)   // safe tiny near
        , far_plane(1e6f)     // large far
    {
    }

    void computeBasis(glm::vec3& right, glm::vec3& up_cam, glm::vec3& forward) const
    {
        forward = glm::normalize(look_at - position);
        right = glm::normalize(glm::cross(forward, up));
        up_cam = glm::cross(right, forward);
    }

    // New: View matrix (right/up/forward basis)
    [[nodiscard]] glm::mat4 viewMatrix() const
    {
        glm::vec3 r, u, f;
        computeBasis(r, u, f);
        glm::mat4 V(1.0f);
        V[0] = glm::vec4(r, 0.0f);
        V[1] = glm::vec4(u, 0.0f);
        V[2] = glm::vec4(-f, 0.0f);
        V = glm::transpose(V);
        return V * glm::translate(glm::mat4(1.0f), -position);
    }

    // New: Projection matrix helpers (aspect required)
    [[nodiscard]] glm::mat4 projectionMatrix(float aspect, float ortho_scale = 1.0f) const
    {
        if (perspective) {
            float fov_rad = fov_deg * (std::numbers::pi_v<float> / 180.0f);
            return glm::perspective(fov_rad, aspect, std::max(near_plane, kEpsilon), std::max(far_plane, near_plane + kEpsilon));
        }
        else {
            float half_w = ortho_scale * aspect * 0.5f;
            float half_h = ortho_scale * 0.5f;
            return glm::ortho(-half_w, half_w, -half_h, half_h, std::max(near_plane, kEpsilon), std::max(far_plane, near_plane + kEpsilon));
        }
    }

    // Kept signature: project a camera-space point to NDC; returns false if behind near plane
    bool projectToNDC(const glm::vec3& p_cam, float aspect, float& ndc_x, float& ndc_y, float& zcam) const
    {
        zcam = p_cam.z;
        // Near plane check
        if (zcam <= std::max(near_plane, kEpsilon)) return false;

        if (perspective) {
            float fov_rad = fov_deg * (std::numbers::pi_v<float> / 180.0f);
            float scale = std::tan(fov_rad * 0.5f);
            ndc_x = (p_cam.x / (zcam * scale)) / aspect;
            ndc_y = (p_cam.y / (zcam * scale));
        }
        else {
            // For orthographic, caller usually pre-scales by ortho_scale/aspect; leave semantics intact
            ndc_x = p_cam.x;
            ndc_y = p_cam.y;
        }
        return true;
    }
};
