#pragma once
#include "Options.h"

class ParseArgs {
private:
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

    static void printhelp()
    {
        std::cerr << "Usage: magic_eye input.stl texture.png/null outprefix [options]\n";
        std::cerr << "Options:\n";
        std::cerr << "  -w width             : Output width (default: " << defaultWidth << ")\n";
        std::cerr << "  -h height            : Output height (default: " << defaultHeight << ")\n";
        std::cerr << "  -sep eye_sep         : Eye separation in pixels (default: " << defaultEyeSep << ")\n";
        std::cerr << "  -fov fov_deg         : Field of view in degrees (default: " << defaultFov << ")\n";
        std::cerr << "  -persp 0|1           : 1 for perspective, 0 for orthographic (default: 1)\n";
        std::cerr << "  -cam x,y,z           : Camera position (default: auto)\n";
        std::cerr << "  -look x,y,z          : Look-at point (default: auto)\n";
        std::cerr << "  -rot x,y,z           : Rotate model (degrees, XYZ order)\n";
        std::cerr << "  -trans x,y,z         : Translate model\n";
        std::cerr << "  -sc x,y,z            : Scale model\n";
        std::cerr << "  -orthsc              : Orthographic scale\n";
        std::cerr << "  -sepbg               : background seperation scale\n";
        std::cerr << "  -depthgama depth     : depth gama adjust (0-1) (default .9)\n";
        std::cerr << "  -orthtune lo hi      : Orthographic scale tuning lo hi (default 0.6 1.2)\n";
        std::cerr << "  -shear x,y,z         : Shear model (XY,XZ,YZ)\n";
        std::cerr << "  -depthrange near far : Set normalized depth range (default: 1.0 0.0)\n";
        std::cerr << "  -brightness val      : Texture brightness (0.5-2.0, default 1.0)\n";
        std::cerr << "  -contrast val        : Texture contrast (0.5-2.0, default 1.0)\n";
        std::cerr << "  -fthresh thresh      : Forground threshhold (0-1 default .90)\n";
        std::cerr << "  -sthresh thresh      : Smooth threshhold (0-1 default .75)\n";
        std::cerr << "  -sweight weight      : Smooth weigth (default 12.0)\n";
        std::cerr << "  -laplace             : Enable Laplace mesh smoothing\n";
        std::cerr << "  -laplacelayers       : Laplace smooth layers (if laplace enabled default 15)\n";
    }

public:
    static bool parseArgs(int argc, char** argv, std::shared_ptr<Options>& options)
    {
        if (argc < 4) {
            printhelp();
            return false;
        }
        options->stlpath = argv[1];
        options->texpath = argv[2];
        options->outprefix = argv[3];

        for (int i = 4; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-w" && i + 1 < argc) {
                options->width = std::atoi(argv[++i]);
            }
            else if (arg == "-h" && i + 1 < argc) {
                options->height = std::atoi(argv[++i]);
            }
            else if (arg == "-sep" && i + 1 < argc) {
                options->eye_sep = std::atoi(argv[++i]);
            }
            else if (arg == "-fov" && i + 1 < argc) {
                options->fov = parseFloat(argv[++i]);
            }
            else if (arg == "-persp" && i + 1 < argc) {
                options->perspective_flag = std::atoi(argv[++i]);
            }
            else if (arg == "-cam" && i + 1 < argc) {
                options->custom_cam_pos = parseVec3(argv[++i]);
                options->custom_cam_provided = true;
            }
            else if (arg == "-brightness" && i + 1 < argc) {
                options->texture_brightness = parseFloat(argv[++i]);
            }
            else if (arg == "-contrast" && i + 1 < argc) {
                options->texture_contrast = parseFloat(argv[++i]);
            }
            else if (arg == "-look" && i + 1 < argc) {
                options->custom_look_at = parseVec3(argv[++i]);
                options->custom_lookat_provided = true;
            }
            else if (arg == "-rot" && i + 1 < argc) {
                options->rot_deg = parseVec3(argv[++i]);
            }
            else if (arg == "-trans" && i + 1 < argc) {
                options->trans = parseVec3(argv[++i]);
            }
            else if (arg == "-sc" && i + 1 < argc) {
                options->sc = parseVec3(argv[++i]);
            }
            else if (arg == "-shear" && i + 1 < argc) {
                options->shear = parseVec3(argv[++i]);
            }
            else if (arg == "-orthsc" && i + 1 < argc) {
                options->custom_orth_scale = parseFloat(argv[++i]);
                options->custom_orth_scale_provided = true;
            }
            else if (arg == "-depthrange" && i + 2 < argc) {
                options->depth_near = parseFloat(argv[++i]);
                options->depth_far = parseFloat(argv[++i]);
            }
            else if (arg == "-orthtune" && i + 2 < argc) {
                options->orthTuneLow = parseFloat(argv[++i]);
                options->orthTuneHi = parseFloat(argv[++i]);
            }
            else if (arg == "-depthgama" && i + 1 < argc) {
                options->depth_gamma = parseFloat(argv[++i]);
            }
            else if (arg == "-sepbg" && i + 1 < argc) {
                options->bg_separation = parseFloat(argv[++i]);
            }
            else if (arg == "-fthresh" && i + 1 < argc) {
                options->foreground_threshold = parseFloat(argv[++i]);
            }
            else if (arg == "-sthresh" && i + 1 < argc) {
                options->smoothThreshold = parseFloat(argv[++i]);
            }
            else if (arg == "-sweight" && i + 1 < argc) {
                options->smoothWeight = parseFloat(argv[++i]);
            }
            else if (arg == "-laplace" && i + 0 < argc) {
                options->laplace_smoothing = true;
            }
            else if (arg == "-laplacelayers" && i + 1 < argc) {
                options->laplace_smooth_layers = std::atoi(argv[++i]);
            }
            else {
                std::cerr << "Unknown or incomplete option: " << arg << "\n";
                return false;
            }
        }

        return true;
    }

};