// Written by Paul Baxter (revised)
#pragma once
#include <stl.h>
#include <iostream>
#include <exception>
#include <filesystem>
#include <algorithm>
#include <cmath>

#include "Camera.h"
#include "DepthMapGenerator.h"
#include "SIRDSGenerator.h"
#include "Options.h"
#include "objtostl.h"
#include "Stlsmoother.h"

#include "stb_image_impl.h"

class StereogramGenerator {
public:
    StereogramGenerator(std::shared_ptr<Options>& opt) : options(opt) {}

    int create()
    {
        if (!options) throw std::runtime_error("StereogramGenerator::create: Options is null.");

        stl mesh;

        // Detect OBJ vs STL using filesystem::path
        std::filesystem::path p(options->stlpath);
        std::string ext = p.has_extension() ? p.extension().string() : std::string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (ext == ".obj") {
            if (!OBJToSTL::convert(options->stlpath, mesh)) {
                throw std::runtime_error("Failed to read OBJ: " + options->stlpath);
            }
        }
        else {
            if (mesh.read_stl(options->stlpath.c_str()) != 0) {
                throw std::runtime_error("Failed to read STL: " + options->stlpath);
            }
        }

        mesh.normalizeAndCenter();

        std::cout << "Loaded triangles: " << mesh.m_num_triangles << "\n";

        transformMesh(mesh, options);

        if (options->laplace_smoothing) {
            smoothSTL(mesh, options->laplace_smooth_layers);
        }

        auto [center, span] = calculateMeshBounds(mesh.m_vectors.data(), mesh.m_num_triangles * 3);

        // Optional floor
        if (options->add_floor && options->rampWidth > 0.0f && options->rampHeight > 0.0f) {
            float floor_z = center.z - span * 0.5f;
            float floor_size_x = span * options->rampWidth;
            float floor_size_y = span * options->rampWidth;
            addFloorMesh(mesh, center.x, center.y, floor_z, floor_size_x, floor_size_y, options->rampHeight);
        }

        Camera cam = setupCamera(options, center, span);

        float ortho_scale = calculateOrthoScale(options, span);

        float zmin = 0.0f, zmax = 0.0f;
        auto depth = DepthMapGenerator::generate(mesh, options->width, options->height,
            cam, ortho_scale, zmin, zmax,
            options->depth_near, options->depth_far,
            options->bg_separation);

        std::cout << "Depth zmin=" << zmin << " zmax=" << zmax << "\n";

        saveDepthVisualization(depth, options);

        auto textureData = loadTexture(options);

        std::vector<uint8_t> sirds_rgb;
        SIRDSGenerator::generate(depth, options->width, options->height, options->eye_sep,
            textureData.texture, textureData.tw, textureData.th, textureData.tchan,
            sirds_rgb, options->texture_brightness, options->texture_contrast,
            options->bg_separation, options);

        saveStereogram(sirds_rgb, options);

        return 0;
    }

private:
    std::shared_ptr<Options> options;

    struct TextureData {
        std::vector<uint8_t> texture;
        int tw = 0;
        int th = 0;
        int tchan = 0;
        bool hasTexture = false;
    };

    void transformMesh(stl& mesh, const std::shared_ptr<Options>& options)
    {
        float* vdata = mesh.m_vectors.data();
        size_t vcount = static_cast<size_t>(mesh.m_num_triangles) * 3;

        v::scale(vdata, static_cast<uint32_t>(vcount), options->sc.x, options->sc.y, options->sc.z);
        v::shear_mesh(vdata, static_cast<uint32_t>(vcount), options->shear.x, options->shear.y, options->shear.z);
        v::rotateQuaternion(vdata, static_cast<uint32_t>(vcount), options->rot_deg.x, options->rot_deg.y, options->rot_deg.z, glm::vec3(0, 0, 0));
        v::translate(vdata, static_cast<uint32_t>(vcount), options->trans.x, options->trans.y, options->trans.z);
    }

    std::pair<glm::vec3, float> calculateMeshBounds(const float* vdata, size_t vcount)
    {
        float minx = 1e9f, miny = 1e9f, minz = 1e9f;
        float maxx = -1e9f, maxy = -1e9f, maxz = -1e9f;

        v::min_max(vdata, static_cast<uint32_t>(vcount), minx, maxx, miny, maxy, minz, maxz);

        glm::vec3 center = { (minx + maxx) * 0.5f, (miny + maxy) * 0.5f, (minz + maxz) * 0.5f };
        float spanx = maxx - minx;
        float spany = maxy - miny;
        float spanz = maxz - minz;
        float span = std::max({ spanx, spany, spanz, tolerance });

        return { center, span };
    }

    Camera setupCamera(const std::shared_ptr<Options>& options, const glm::vec3& center, float span)
    {
        Camera cam;
        cam.up = { 0, 1, 0 };
        cam.perspective = (options->perspective);
        cam.fov_deg = options->fov;

        if (options->custom_cam_provided) {
            cam.position = options->custom_cam_pos;
        }
        else {
            cam.position = { center.x, center.y, center.z + span * 2.5f };
        }

        if (options->custom_lookat_provided) {
            cam.look_at = options->custom_look_at;
        }
        else {
            cam.look_at = center;
        }

        return cam;
    }

    float calculateOrthoScale(const std::shared_ptr<Options>& options, float span)
    {
        if (options->custom_orth_scale_provided) {
            return options->custom_orth_scale;
        }

        float aspect = static_cast<float>(options->width) / std::max(1, options->height);
        auto scale = span * options->orthTuneLow * std::max(1.0f / aspect, 1.0f) * options->orthTuneHi;
        return scale;
    }

    void saveDepthVisualization(const std::vector<float>& depth, const std::shared_ptr<Options>& options)
    {
        std::vector<uint8_t> depth_vis(static_cast<size_t>(options->width) * options->height * 3);
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

    TextureData loadTexture(const std::shared_ptr<Options>& options)
    {
        TextureData data;

        if (!options->texpath.empty() && options->texpath != "null") {
            if (TextureSampler::loadRGB(options->texpath, data.texture, data.tw, data.th, data.tchan)) {
                data.hasTexture = true;
                std::cout << "Loaded texture " << options->texpath << " ("
                    << data.tw << "x" << data.th << " ch=" << data.tchan << ")\n";
            }
            else {
                throw std::runtime_error("Failed to load texture '" + options->texpath + "'");
            }
        }
        else {
            std::cout << "Using random-dot texture.\n";
        }

        return data;
    }

    void saveStereogram(const std::vector<uint8_t>& sirds_rgb, const std::shared_ptr<Options>& options)
    {
        std::string sirds_out = options->outprefix + "_sirds.png";
        stbi_write_png(sirds_out.c_str(), options->width, options->height, 3,
            sirds_rgb.data(), options->width * 3);
        std::cout << "Wrote stereogram: " << sirds_out << "\n";
    }

    void addFloorMesh(stl& mesh, float cx, float cy, float cz,
        float size_x, float size_y, float ramp_amount,
        const glm::vec3& color = { 0.8f, 0.8f, 0.8f })
    {
        float halfx = size_x * 0.5f;
        float halfy = size_y * 0.5f;

        float y0 = cy - halfy;
        float y1 = cy;

        float z_far = cz - 0.35f * size_y;
        float z_near = z_far + ramp_amount;

        glm::vec3 v0 = { cx - halfx, y0, z_near };
        glm::vec3 v1 = { cx + halfx, y0, z_near };
        glm::vec3 v2 = { cx + halfx, y1, z_far };
        glm::vec3 v3 = { cx - halfx, y1, z_far };

        std::vector<float> tris = {
            v0.x,v0.y,v0.z,  v1.x,v1.y,v1.z,  v2.x,v2.y,v2.z,
            v0.x,v0.y,v0.z,  v2.x,v2.y,v2.z,  v3.x,v3.y,v3.z
        };

        for (int i = 0; i < 6; ++i) {
            mesh.m_rgb_color.insert(mesh.m_rgb_color.end(), { color.r,color.g,color.b });
        }
        mesh.m_vectors.insert(mesh.m_vectors.end(), tris.begin(), tris.end());
        mesh.m_num_triangles += 2;
    }
};
