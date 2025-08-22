// written by Paul Baxter
#pragma once
#include <stl.h>
#include <iostream>

#include "Camera.h"
#include "DepthMapGenerator.h"
#include "SIRDSGenerator.h"
#include "Options.h"

// stb_image libraries for loading/writing textures/images
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

class StereogramGenerator {
public:
    // Main entry point for creating a stereogram
    static int create(const std::shared_ptr<Options>& options)
    {
        // Load STL mesh from file
        stl mesh;
        if (mesh.read_stl(options->stlpath.c_str()) != 0) {
            std::cerr << "Failed to read STL: " << options->stlpath << "\n";
            return 1;
        }
        std::cout << "Loaded triangles: " << mesh.m_num_triangles << "\n";

        // Apply transformations (scale, shear, rotate, translate) to the mesh
        transformMesh(mesh, options);

        // Compute the bounding box center and span (size of object in space)
        auto [center, span] = calculateMeshBounds(mesh.m_vectors.data(), mesh.m_num_triangles * 3);

        // Setup camera based on options and bounding box
        Camera cam = setupCamera(options, center, span);

        // Compute orthographic projection scale (if used)
        float ortho_scale = calculateOrthoScale(options, span);

        // Generate depth map from STL mesh
        float zmin, zmax;
        auto depth = DepthMapGenerator::generate(mesh, options->width, options->height,
            cam, ortho_scale, zmin, zmax,
            options->depth_near, options->depth_far,
            options->bg_separation);

        std::cout << "Depth zmin=" << zmin << " zmax=" << zmax << "\n";

        // Save grayscale visualization of depth map for debugging
        saveDepthVisualization(depth, options);

        // Load texture (if provided) or fall back to random-dot pattern
        auto textureData = loadTexture(options);

        // Generate stereogram image (Single Image Random Dot Stereogram)
        std::vector<uint8_t> sirds_rgb;
        SIRDSGenerator::generate(depth, options->width, options->height, options->eye_sep,
            textureData.texture, textureData.tw, textureData.th, textureData.tchan,
            sirds_rgb, options->texture_brightness, options->texture_contrast,
            options->bg_separation, options);

        // Save stereogram image to disk
        saveStereogram(sirds_rgb, options);

        return 0;
    }

private:
    // Structure for holding texture data
    struct TextureData {
        std::vector<uint8_t> texture;  // Raw pixel data
        int tw = 0;                    // Width
        int th = 0;                    // Height
        int tchan = 0;                 // Number of channels (RGB=3, RGBA=4, etc.)
        bool hasTexture = false;       // True if valid texture loaded
    };

    // Apply transformations from options to mesh vertices
    static void transformMesh(stl& mesh, const std::shared_ptr<Options>& options)
    {
        float* vdata = mesh.m_vectors.data();
        size_t vcount = static_cast<size_t>(mesh.m_num_triangles) * 3;

        v::scale(vdata, vcount, options->sc.x, options->sc.y, options->sc.z);
        v::shear_mesh(vdata, vcount, options->shear.x, options->shear.y, options->shear.z);
        v::rotate(vdata, vcount, options->rot_deg.x, options->rot_deg.y, options->rot_deg.z);
        v::translate(vdata, vcount, options->trans.x, options->trans.y, options->trans.z);
    }

    // Compute mesh bounds → returns center and largest span (size in 3D space)
    static std::pair<glm::vec3, float> calculateMeshBounds(const float* vdata, size_t vcount)
    {
        float minx = 1e9f, miny = 1e9f, minz = 1e9f;
        float maxx = -1e9f, maxy = -1e9f, maxz = -1e9f;

        v::min_max(vdata, vcount, minx, maxx, miny, maxy, minz, maxz);

        glm::vec3 center = { (minx + maxx) * 0.5f, (miny + maxy) * 0.5f, (minz + maxz) * 0.5f };
        float spanx = maxx - minx;
        float spany = maxy - miny;
        float spanz = maxz - minz;
        float span = std::max({ spanx, spany, spanz, tolerance });

        return { center, span };
    }

    // Setup camera parameters from options (position, look-at, projection)
    static Camera setupCamera(const std::shared_ptr<Options>& options, const glm::vec3& center, float span)
    {
        Camera cam;
        cam.up = { 0, 1, 0 };
        cam.perspective = (options->perspective_flag != 0);
        cam.fov_deg = options->fov;

        // Camera position
        if (options->custom_cam_provided) {
            cam.position = options->custom_cam_pos;
        }
        else {
            cam.position = { center.x, center.y, center.z + span * 2.5f };
        }

        // Camera look-at target
        if (options->custom_lookat_provided) {
            cam.look_at = options->custom_look_at;
        }
        else {
            cam.look_at = center;
        }

        return cam;
    }

    // Compute orthographic projection scale based on options and bounding box span
    static float calculateOrthoScale(const std::shared_ptr<Options>& options, float span)
    {
        if (options->custom_orth_scale_provided) {
            return options->custom_orth_scale;
        }

        float aspect = static_cast<float>(options->width) / options->height;
        // Compute default orthographic scale based on mesh span, screen aspect ratio, 
        // and empirical tuning factors (0.6f and 1.2f) to ensure object fits comfortably in view.
        return span * options->orthTuneLow * std::max(1.0f / aspect, 1.0f) * options->orthTuneHi;
    }

    // Save depth map visualization as grayscale PNG
    static void saveDepthVisualization(const std::vector<float>& depth, const std::shared_ptr<Options>& options)
    {
        std::vector<uint8_t> depth_vis(options->width * options->height * 3);
        for (int i = 0; i < options->width * options->height; ++i) {
            uint8_t v = static_cast<uint8_t>(std::round(std::clamp(depth[i], 0.0f, 1.0f) * 255.0f));
            depth_vis[i * 3 + 0] = v;
            depth_vis[i * 3 + 1] = v;
            depth_vis[i * 3 + 2] = v;
        }

        std::string depth_out = options->outprefix + "_depth.png";
        stbi_write_png(depth_out.c_str(), options->width, options->height, 3,
            depth_vis.data(), options->width * 3);
        std::cout << "Wrote depth visualization: " << depth_out << "\n";
    }

    // Load texture from file if provided, otherwise fallback to random dots
    static TextureData loadTexture(const std::shared_ptr<Options>& options)
    {
        TextureData data;

        if (options->texpath != "null") {
            if (TextureSampler::loadRGB(options->texpath, data.texture,
                data.tw, data.th, data.tchan)) {
                data.hasTexture = true;
                std::cout << "Loaded texture " << options->texpath << " ("
                    << data.tw << "x" << data.th << " ch=" << data.tchan << ")\n";
            }
            else {
                std::cout << "Failed to load texture '" << options->texpath
                    << "'. Falling back to random dots.\n";
            }
        }
        else {
            std::cout << "Using random-dot texture.\n";
        }

        return data;
    }

    // Save final stereogram (SIRDS) image as PNG
    static void saveStereogram(const std::vector<uint8_t>& sirds_rgb, const std::shared_ptr<Options>& options)
    {
        std::string sirds_out = options->outprefix + "_sirds.png";
        stbi_write_png(sirds_out.c_str(), options->width, options->height, 3,
            sirds_rgb.data(), options->width * 3);
        std::cout << "Wrote stereogram: " << sirds_out << "\n";
    }
};
