// written by Paul Baxter
#pragma once
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
#include "stl.h"
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
#ifdef STL_CLI
        std::cout << "Loaded triangles: " << mesh.m_num_triangles << "\n";
#endif
        transformMesh(mesh, options);

        if (options->laplace_smoothing) {
            smoothSTL(mesh, options->laplace_smooth_layers);
        }

        auto [center, xyzspan] = calculateMeshBounds(mesh.m_vectors.data(), mesh.m_num_triangles * 3);

        float span = std::max({ xyzspan[0], xyzspan[1], xyzspan[2], 1e-6f});

        Camera cam = setupCamera(options, center, span);

        // Optional floor
        if (options->add_floor && options->rampWidth > 0.0f) {
            addFloorRampFacingCamera(mesh, cam, center, xyzspan,
                options->rampWidth, options->rampSep, options->rampAngle);
        }

        float ortho_scale = calculateOrthoScale(options, span);

        float zmin = 0.0f, zmax = 0.0f;
        auto depth = DepthMapGenerator::generate(mesh, options->width, options->height,
            cam, ortho_scale, zmin, zmax,
            options->depth_near, options->depth_far,
            options->bg_separation);

#ifdef STL_CLI
        std::cout << "Depth zmin=" << zmin << " zmax=" << zmax << "\n";
#endif
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

        vectorutils::scale(vdata, static_cast<uint32_t>(vcount), options->sc.x, options->sc.y, options->sc.z);
        vectorutils::shear_mesh(vdata, static_cast<uint32_t>(vcount), options->shear.x, options->shear.y, options->shear.z);
        vectorutils::rotateQuaternion(vdata, static_cast<uint32_t>(vcount), options->rot_deg.x, options->rot_deg.y, options->rot_deg.z, glm::vec3(0, 0, 0));
        vectorutils::translate(vdata, static_cast<uint32_t>(vcount), options->trans.x, options->trans.y, options->trans.z);
    }

    std::pair<glm::vec3, glm::vec3> calculateMeshBounds(const float* vdata, size_t vcount)
    {
        float minx = 1e9f, miny = 1e9f, minz = 1e9f;
        float maxx = -1e9f, maxy = -1e9f, maxz = -1e9f;

        vectorutils::min_max(vdata, static_cast<uint32_t>(vcount), minx, maxx, miny, maxy, minz, maxz);

        glm::vec3 center = { (minx + maxx) * 0.5f, (miny + maxy) * 0.5f, (minz + maxz) * 0.5f };
        float spanx = maxx - minx;
        float spany = maxy - miny;
        float spanz = maxz - minz;
        

        return { center, {spanx, spany, spanz } };
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
#ifdef STL_CLI
        std::cout << "Wrote depth visualization: " << depth_out << "\n";
#endif
    }

    TextureData loadTexture(const std::shared_ptr<Options>& options)
    {
        TextureData data;

        if (!options->texpath.empty() && options->texpath != "null") {
            if (TextureSampler::loadRGB(options->texpath, data.texture, data.tw, data.th, data.tchan)) {
                data.hasTexture = true;
#ifdef STL_CLI
                std::cout << "Loaded texture " << options->texpath << " ("
                    << data.tw << "x" << data.th << " ch=" << data.tchan << ")\n";
#endif
            }
            else {
                throw std::runtime_error("Failed to load texture '" + options->texpath + "'");
            }
        }
        else {
#ifdef STL_CLI
            std::cout << "Using random-dot texture.\n";
#endif
        }

        return data;
    }

    void saveStereogram(const std::vector<uint8_t>& sirds_rgb, const std::shared_ptr<Options>& options)
    {
        std::string sirds_out = options->outprefix + "_sirds.png";
        stbi_write_png(sirds_out.c_str(), options->width, options->height, 3,
            sirds_rgb.data(), options->width * 3);
#ifdef STL_CLI
        std::cout << "Wrote stereogram: " << sirds_out << "\n";
#endif
    }

    void addFloorRampFacingCamera(
        stl& mesh,
        const Camera& cam,
        const glm::vec3& center, // world-space center of the image mesh
        const glm::vec3& xyzspan,   // image mesh span (as before)
        float rampWidth,         // horizontal width fraction [0..1]
        float rampSep,
        float floorAngleDeg,     // 90 = straight toward camera, >90 = slopes down along -up
        const glm::vec3& color = { 0.8f, 0.8f, 0.8f }

    )
    {
        auto xspan = xyzspan[0];
        auto yspan = xyzspan[1];
        auto zspan = xyzspan[2];
        float span = std::max({ xspan, yspan, zspan, 1e-6f });

        glm::vec3 right, up, forward;
        cam.computeBasis(right, up, forward);
        xspan += xspan * 0.05f;
        yspan += yspan * 0.05f;
        zspan += zspan * 0.05f;
        span += span * 0.05f;

        // Ensure 'forward' points from camera into the scene
        if (glm::dot(center - cam.position, forward) < 0.0f)
            forward = -forward;

        float halfx = xspan * rampWidth * 0.5f;
        float gap = yspan * rampSep;   // small gap under the image

        // The line we visually attach to (bottom of the image mesh)
        glm::vec3 topCenter = center - up * (0.5f * yspan + gap);

        // Scan current mesh to find frontmost/farthest distances along 'forward'
        float dMin = std::numeric_limits<float>::infinity();      // closest to camera
        float dMax = -std::numeric_limits<float>::infinity();     // farthest from camera
        const auto& V = mesh.m_vectors;
        for (size_t i = 0; i + 2 < V.size(); i += 3) {
            glm::vec3 p(V[i + 0], V[i + 1], V[i + 2]);
            float d = glm::dot(p - cam.position, forward); // distance along 'forward'
            dMin = std::min(dMin, d);
            dMax = std::max(dMax, d);
        }

        // Fallback if the mesh is empty
        float dTop = glm::dot(topCenter - cam.position, forward);
        if (!std::isfinite(dMin) || !std::isfinite(dMax)) {
            dMin = dTop;
            dMax = dTop;
        }

        if (dMax < dMin) std::swap(dMax, dMin);

        // Optional: avoid near-plane clipping (keeps the ramp inside the camera frustum)
        float eps = 1e-3f;
        float dFront = std::max(dMin, cam.near_plane + eps); // do not exceed mesh's frontmost; clamp to near plane if needed

        float forwardAdvance = glm::max(0.0f, dMax - dFront); // how far we move toward the camera
        // Angle semantics: 90 = no drop; >90 drops toward -up as we come forward; <90 slopes upward
        float drop = forwardAdvance * std::tan(glm::radians(floorAngleDeg - 90.0f));

        // Build quad
        // Back (top) edge: align to the FAR side of the mesh (dMax), so it "starts at the back"
        glm::vec3 deltaBack = forward * (dMax - dTop);
        glm::vec3 v0 = topCenter - right * halfx + deltaBack;              // back/top-left
        glm::vec3 v1 = topCenter + right * halfx + deltaBack;              // back/top-right

        // Front (bottom) edge: at the mesh's FRONT-most distance (dFront), with the requested tilt
        glm::vec3 v2 = v1 - forward * forwardAdvance - up * drop;          // front/bottom-right
        glm::vec3 v3 = v0 - forward * forwardAdvance - up * drop;          // front/bottom-left

        // Emit with facing fixed toward the camera
        auto emitTriFacingCamera = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c)
            {
                glm::vec3 n = glm::cross(b - a, c - a);
                if (glm::dot(n, forward) > 0.0f) std::swap(b, c);

                mesh.m_vectors.insert(mesh.m_vectors.end(),
                    { a.x,a.y,a.z, b.x,b.y,b.z, c.x,c.y,c.z });
                for (int i = 0; i < 3; ++i)
                    mesh.m_rgb_color.insert(mesh.m_rgb_color.end(), { color.r,color.g,color.b });
                mesh.m_num_triangles++;
            };

        if (forwardAdvance > 0.0f || std::abs(drop) > 0.0f) {
            emitTriFacingCamera(v0, v1, v2);
            emitTriFacingCamera(v0, v2, v3);
        }
    }
};
