// Written by Paul Baxter (revised)
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

class Options {
public:
    std::string stlpath = "";
    std::string texpath = "";
    std::string outprefix = "out";
    int width = 1280;
    int height = 800;
    int eye_sep = 160;
    int laplace_smooth_layers = 15;
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
    bool perspective = true;
    bool smoothEdges = true;
    float custom_orth_scale = 50;
    float fov = 45;
    float depth_near = 0.75f;
    float depth_far = 0.1f;
    float texture_brightness = 1.0f;
    float texture_contrast = 1.0f;
    float bg_separation = 0.3f;
    float depth_gamma = 1.0f;
    float orthTuneLow = 0.6f;
    float orthTuneHi = 1.2f;
    float foreground_threshold = 0.90f;
    float smoothThreshold = 0.75f;
    float smoothWeight = 6.0f; // larger -> milder smoothing
    float rampWidth = 2.5f;
    float rampHeight = 100.0f;

    // New options (non-breaking defaults)
    bool add_floor = true;          // enable/disable ramp "floor" geometry
    int  rng_seed = -1;             // <0 -> random_device, otherwise fixed seed for reproducibility
    //bool occlusion = true;          // enable simple occlusion gate in SIRDS linking
    //float occlusion_epsilon = 0.02f;// depth tolerance for occlusion gate
    bool tile_texture = true;       // true: repeat texture, false: clamp at edges
};

