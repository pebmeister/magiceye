// written by Paul Baxter
#pragma once

#include <string>
#include <sstream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include "vectorutils.h"

using v = vectorutils;

constexpr float tolerance = 1e-6f;

#include "Camera.h"

class Options {
public:
    std::string stlpath = "";
    std::string texpath = "";
    std::string outprefix = "out";
    int width = 1200;
    int height = 800;
    int eye_sep = 160;
    int perspective_flag = 1;
    glm::vec3 custom_cam_pos = { 0, 0, 0 };
    glm::vec3 custom_look_at = { 0, 0, 0 };
    glm::vec3 rot_deg = { 0, 0, 0 };
    glm::vec3 trans = { 0, 0, 0 };
    glm::vec3 sc = { 1, 1, 1 };
    glm::vec3 shear = { 0, 0, 0 };
    bool custom_cam_provided = false;
    bool custom_lookat_provided = false;
    bool custom_orth_scale_provided = false;
    bool laplace_smoothing = false;
    float custom_orth_scale = 1;
    float fov = 45;
    float depth_near = 0.75f;
    float depth_far = 0.1f;
    float texture_brightness = 1.0f;
    float texture_contrast = 1.0f;
    float bg_separation = 0.6f;
    float depth_gamma = 1.0f;
    float orthTuneLow = 0.6f;
    float orthTuneHi = 1.2f;
    float foreground_threshold = 0.90f;
    float smoothThreshold = 0.75;
    float smoothWeight = 10;
    float rampWidth = 2.5;
    float rampHeight = 100.0;
    int laplace_smooth_layers = 15;
};
