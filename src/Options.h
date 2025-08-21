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

constexpr int defaultWidth = 1200;
constexpr int defaultHeight = 800;
constexpr int defaultEyeSep = 100;
constexpr float defaultFov = 45.0f;
constexpr float tolerance = 1e-6f;
constexpr float defaultOrthTuneLow = 0.6f;
constexpr float defaultOrthTuneHi = 1.2f;
// TODO: have these as options
constexpr float foreground_threshold = 0.90f;
constexpr float smoothThreshold = 0.75;

#include "Camera.h"

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
    float texture_brightness = 1.0f;
    float texture_contrast = 1.0f;
    float bg_separation = 0.4f;
    float orthTuneLow = defaultOrthTuneLow;
    float orthTuneHi = defaultOrthTuneHi;
};
