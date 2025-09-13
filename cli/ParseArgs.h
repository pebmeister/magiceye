#pragma once
#include <exception>
#include <memory>
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

    static bool parseBool(const std::string& str)
    {
        bool out;
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
            throw std::invalid_argument("Warning: Invalid vector format '" + str + "'.");
        }
        return v;
    }

    static void printhelp()
    {
        Options options;

        std::cerr << "Usage: magic_eye input.stl texture.png/null outprefix [options]\n";
        std::cerr << "Options:\n";
        std::cerr << "  -w width             : Output width (default: " << options.width << ")\n";
        std::cerr << "  -h height            : Output height (default: " << options.height << ")\n";
        std::cerr << "  -sep eye_sep         : Eye separation in pixels (default: " << options.eye_sep << ")\n";
        std::cerr << "  -fov fov_deg         : Field of view in degrees (default: " << options.fov << ")\n";
        std::cerr << "  -persp true|false    : true for perspective, false for orthographic (default: " << options.perspective << ")\n";
        std::cerr << "  -cam x,y,z           : Camera position (default: auto)\n";
        std::cerr << "  -look x,y,z          : Look-at point (default: auto)\n";
        std::cerr << "  -rot x,y,z           : Rotate model (degrees, XYZ order default: " 
            << options.rot_deg[0] << "," << options.rot_deg[1] << "," << options.rot_deg[2] << ")\n";
        std::cerr << "  -trans x,y,z         : Translate model (default: "
            << options.trans[0] << "," << options.trans[1] << "," << options.trans[2] << ")\n";
        std::cerr << "  -sc x,y,z            : Scale model ( default :"
            << options.sc[0] << "," << options.sc[1] << "," << options.sc[2] << ")\n";
        std::cerr << "  -orthsc              : Orthographic scale (default : " << options.custom_orth_scale << ")\n";
        std::cerr << "  -sepbg               : background seperation scale (default :" << options.bg_separation <<")\n";
        std::cerr << "  -depthgama depth     : depth gama adjust (default: " << options.depth_gamma << ")\n";
        std::cerr << "  -orthtune lo hi      : Orthographic scale tuning lo hi (default: " <<
            options.orthTuneLow << " " << options.orthTuneHi << ")\n";
        std::cerr << "  -shear x,y,z         : Shear model (XY,XZ,YZ  default: "
            << options.shear[0] << "," << options.shear[1] << "," << options.shear[2] << ")\n";
        std::cerr << "  -depthrange near far : Set normalized depth range (default: "
            << options.depth_near << " " << options.depth_near << "\n";
        std::cerr << "  -brightness val      : Texture brightness (0.5-2.0, default: " << options.texture_brightness << ")\n";
        std::cerr << "  -contrast val        : Texture contrast (0.5-2.0, default: " << options.texture_contrast << ")\n";
        std::cerr << "  -fthresh thresh      : Forground threshhold (0-1 default: " << options.foreground_threshold << ")\n";
        std::cerr << "  -sthresh thresh      : Smooth threshhold (0-1 default  " << options.smoothThreshold << ")\n";
        std::cerr << "  -sweight weight      : Smooth weigth (default: "  << options.smoothWeight << ")\n";
        std::cerr << "  -laplace             : Enable Laplace mesh smoothing (default: " << options.laplace_smoothing << ")\n";
        std::cerr << "  -laplacelayers       : Laplace smooth layers (if laplace enabled default: " << options.laplace_smooth_layers << ")\n";
        std::cerr << "  -rwidth              : Ramp width (default: " << options.rampWidth << ")\n";
        std::cerr << "  -rheight             : Ramp height (default: " << options.rampHeight << ")\n";
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
                options->perspective = parseBool(argv[++i]);
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
            else if (arg == "-rwidth" && i + 1 < argc) {
                options->rampWidth = parseFloat(argv[++i]);
            }
            else if (arg == "-rheight" && i + 1 < argc) {
                options->rampHeight = parseFloat(argv[++i]);
            }
            else {
                printhelp();
                throw std::invalid_argument("Unknown or incomplete option: " + arg);
            }
        }

        return true;
    }

};