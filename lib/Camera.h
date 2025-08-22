// written by Paul Baxter
#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Options.h"

class Camera {
public:
    glm::vec3 position;
    glm::vec3 look_at;
    glm::vec3 up;
    float fov_deg;
    bool perspective;

    Camera() : position(0, 0, 0), look_at(0, 0, 0), up(0, 1, 0),
        fov_deg(defaultFov), perspective(true)
    {
    }

    void computeBasis(glm::vec3& right, glm::vec3& up_cam, glm::vec3& forward) const
    {
        forward = glm::normalize(look_at - position);
        right = glm::normalize(glm::cross(forward, up));
        up_cam = glm::cross(right, forward);
    }

    bool projectToNDC(const glm::vec3& p_cam, float aspect, float& ndc_x, float& ndc_y, float& zcam) const
    {
        zcam = p_cam.z;
        if (zcam <= tolerance) return false;

        if (perspective) {
            float fov_rad = fov_deg * (3.14159265358979323846f / 180.0f);
            float scale = std::tan(fov_rad * 0.5f);
            ndc_x = (p_cam.x / (zcam * scale)) / aspect;
            ndc_y = (p_cam.y / (zcam * scale));
        }
        else {
            ndc_x = p_cam.x;
            ndc_y = p_cam.y;
        }
        return true;
    }
};
