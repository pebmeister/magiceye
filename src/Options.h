#pragma once

#include <string>
#include <sstream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "vectorutils.h"

using v = vectorutils;

constexpr int defaultWidth = 1200;
constexpr int defaultHeight = 800;
constexpr int defaultEyeSep = 100;

#include "Camera.h"
#include <iostream>

class Options {
public:
    std::string stlpath = "";
    std::string texpath = "";
    std::string outprefix = "out";
    int width = defaultWidth;
    int height = defaultHeight;
    int eye_sep = defaultEyeSep;
    float fov = defaultFov;
    int perspective_flag = 1;
    glm::vec3 custom_cam_pos = { 0, 0, 0 };
    glm::vec3 custom_look_at = { 0, 0, 0 };
    glm::vec3 rot_deg = { 0, 0, 0 };
    glm::vec3 trans = { 0, 0, 0 };
    glm::vec3 sc = { 1, 1, 1 };
    glm::vec3 shear = { 0, 0, 0 };
    float custom_orth_scale = 1;
    bool custom_cam_provided = false;
    bool custom_lookat_provided = false;
    bool custom_orth_scale_provided = false;
    float depth_near = 0.75f;
    float depth_far = 0.1f;
    float custom_depth_near = 0.75f;
    float custom_depth_far = 0.1f;
    float texture_brightness = 1.0f;
    float texture_contrast = 1.0f;
    float bg_separation = 0.0f;
    bool custom_depth_range = false;

    static float parseFloat(const std::string& str)
    {
        float out;
        std::stringstream ss(str);
        ss >> out;
        return out;
    }

    static glm::vec3 parseVec3(const std::string& str)
    {
        std::stringstream ss(str);
        glm::vec3 v{ 0, 0, 0 };
        char comma;
        ss >> v.x >> comma >> v.y >> comma >> v.z;
        if (ss.fail()) {
            std::cerr << "Warning: Invalid vector format '" << str << "'. Using (0,0,0)\n";
        }
        return v;
    }
};
