// written by Paul Baxter

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// include your stl header (adjust path if necessary)
#include "stl.h"

// default values
constexpr int defaultWidth = 1200;
constexpr int defaultHeight = 800;
constexpr float defaultFov = 45.0;
constexpr int defaultEyeSep = 256;

constexpr float tolerance = 1e-6f;

// ---------- Camera ----------
struct Camera {
    glm::vec3 position;
    glm::vec3 look_at;
    glm::vec3 up;
    float fov_deg;     // used if perspective
    bool perspective;
};

// compute camera basis: right, up_cam, forward
void computeCameraBasis(const Camera &cam, glm::vec3 &right, glm::vec3 &up_cam, glm::vec3 &forward) {
    forward = glm::normalize(cam.look_at - cam.position); // forward points toward scene
    right = glm::normalize(glm::cross(forward, cam.up));
    up_cam = glm::cross(right, forward);
}

// project a camera-space point to NDC X,Y in [-1,1] (Z returned as camera-space z)
bool projectToNDC(const glm::vec3 &p_cam, float aspect, const Camera &cam, float &ndc_x, float &ndc_y, float &zcam) {
    zcam = p_cam.z; // positive = in front of camera (we require > 0)
    if (zcam <= tolerance) return false; // behind or too close

    if (cam.perspective) {
        // x_ndc = (x_cam / (z_cam * tan(fov/2))) / aspect? We'll scale with aspect so image doesn't distort
        float fov_rad = cam.fov_deg * (3.14159265358979323846f / 180.0f);
        float scale = std::tan(fov_rad * 0.5f); // >0
        ndc_x = (p_cam.x / (zcam * scale)) / aspect;
        ndc_y = (p_cam.y / (zcam * scale));
    } else {
        // Orthographic: assume camera-space units map directly to NDC. We will later choose a scale externally.
        ndc_x = p_cam.x;
        ndc_y = p_cam.y;
    }
    return true;
}

// ---------- Barycentric / raster ----------
bool barycentric2D(float px, float py,
                float ax, float ay,
                float bx, float by,
                float cx, float cy,
                float &u, float &v, float &w)
{
    float denom = (by - cy)*(ax - cx) + (cx - bx)*(ay - cy);
    if (fabs(denom) < tolerance) return false;
    u = ((by - cy)*(px - cx) + (cx - bx)*(py - cy)) / denom;
    v = ((cy - ay)*(px - cx) + (ax - cx)*(py - cy)) / denom;
    w = 1.0f - u - v;
    return true;
}

// ---------- Depth map generation ----------
// We'll transform world vertices into camera space, project to NDC, then map to pixel coords.
// For orthographic, user-defined ortho_scale controls how many camera units map to NDC range.
std::vector<float> generate_depth_map(const stl &mesh,
    int width, int height,
    const Camera &cam,
    float ortho_scale,
    float &out_zmin, float &out_zmax,
    float depth_near, float depth_far)
{
    // initialize z-buffer to +inf (we will store minimal zcam = distance along forward)
    constexpr float INF = std::numeric_limits<float>::infinity();
    std::vector<float> zbuffer(width * height, INF);

    // camera basis
    glm::vec3 right, up_cam, forward;
    computeCameraBasis(cam, right, up_cam, forward);
    float aspect = float(width) / float(height);

    // loop triangles
    size_t triCount = mesh.m_num_triangles;
    const float *vdata = mesh.m_vectors.data(); // assumed layout: per triangle 9 floats: v0,v1,v2
    for (size_t t = 0; t < triCount; ++t) {
        const float *tri = vdata + t * 9;
        glm::vec3 vworld[3] = { {tri[0], tri[1], tri[2]}, {tri[3], tri[4], tri[5]}, {tri[6], tri[7], tri[8]} };
        glm::vec3 vcam[3];
        float ndc_x[3], ndc_y[3], zcam[3];
        bool validProj[3];

        for (int i=0;i<3;i++) {
            glm::vec3 rel = vworld[i] - cam.position;
            // coordinates in camera basis
            vcam[i].x = dot(rel, right);
            vcam[i].y = dot(rel, up_cam);
            vcam[i].z = dot(rel, forward); // forward axis
            glm::vec3 p_cam = vcam[i];
            // For orthographic we scale NDC by ortho_scale; do that by temporarily scaling p_cam.x/y
            glm::vec3 p_for_ndc = p_cam;
            if (!cam.perspective) {

                if (!cam.perspective) {
                    p_for_ndc.x = p_cam.x / (ortho_scale * aspect);
                    p_for_ndc.y = p_cam.y / ortho_scale;
                }
                p_for_ndc.z = p_cam.z;
            }
            validProj[i] = projectToNDC(p_for_ndc, aspect, cam, ndc_x[i], ndc_y[i], zcam[i]);
        }

        // if all three are invalid (behind camera), skip tri
        if (!validProj[0] && !validProj[1] && !validProj[2]) continue;

        // Map NDC -1..1) to pixel coords. If orthographic we already scaled NDC by ortho_scale above.
        float px[3], py[3];
        for (int i=0;i<3;i++) {
            // clamp ndc to -1..1 to avoid huge coordinates from perspective
            float clx = std::max(-1.0f, std::min(1.0f, ndc_x[i]));
            float cly = std::max(-1.0f, std::min(1.0f, ndc_y[i]));
            px[i] = (clx * 0.5f + 0.5f) * (width - 1);
            py[i] = ( -cly * 0.5f + 0.5f) * (height - 1); // note Y flip: NDC y up -> image y down
        }

        // bounding box in pixel coordinates
        int minx = std::max(0, (int)std::floor(std::min({px[0], px[1], px[2]})));
        int maxx = std::min(width - 1, (int)std::ceil(std::max({px[0], px[1], px[2]})));
        int miny = std::max(0, (int)std::floor(std::min({py[0], py[1], py[2]})));
        int maxy = std::min(height - 1, (int)std::ceil(std::max({py[0], py[1], py[2]})));

        // skip degenerate
        float denom = (py[1] - py[2])*(px[0] - px[2]) + (px[2] - px[1])*(py[0] - py[2]);
        if (fabs(denom) < tolerance) continue;

        // rasterize triangle
        for (int y = miny; y <= maxy; ++y) {
            for (int x = minx; x <= maxx; ++x) {
                float u, v, w;
                // sample at pixel center
                if (!barycentric2D((float)x + 0.5f, (float)y + 0.5f,
                                px[0], py[0], px[1], py[1], px[2], py[2],
                                u, v, w)) continue;
                if (u < 0 || v < 0 || w < 0) continue;
                // interpolate zcam (camera-space forward distance)
                // If a vertex was not projectable (behind), we still have zcam from projectToNDC for others; barycentric should still work.
                float z_interp = u * zcam[0] + v * zcam[1] + w * zcam[2];
                if (!(z_interp > 0.0f)) continue; // behind camera or invalid

                int idx = y * width + x;
                if (z_interp < zbuffer[idx]) {
                    zbuffer[idx] = z_interp;
                }
            }
        }
    }

    // find zmin/zmax among finite entries
    out_zmin = std::numeric_limits<float>::infinity();
    out_zmax = -std::numeric_limits<float>::infinity();
    for (int i = 0; i < width * height; ++i) {
        float z = zbuffer[i];
        if (std::isfinite(z)) {
            out_zmin = std::min(out_zmin, z);
            out_zmax = std::max(out_zmax, z);
        }
    }

    //  produce normalized depth map 0..1 where 1 = nearest
    std::vector<float> depth(width * height, 0.0f);
    if (!std::isfinite(out_zmin) || !std::isfinite(out_zmax)) {
        // nothing visible; return zeros
        return depth;
    }
    float range = out_zmax - out_zmin;
    if (range < tolerance) range = 1.0f;
    for (int i = 0; i < width * height; ++i) {
        float z = zbuffer[i];
        if (!std::isfinite(z)) depth[i] = depth_far;
        else {
            float t = (z - out_zmin) / range;
            // Map: z = out_zmin (closest) -> depth_near ; z = out_zmax (farthest) -> depth_far
            depth[i] = depth_near + (depth_far - depth_near) * t;
        }
    }
    return depth;
}

// ---------- Union-Find ----------
struct UnionFind {
    std::vector<int> parent;
    UnionFind(int n = 0) { reset(n); }
    void reset(int n)
    {
        parent.resize(n);
        for (int i = 0; i < n; ++i) parent[i] = i;
    }
    int find(int x)
    {
        return parent[x] == x ? x : (parent[x] = find(parent[x]));
    }
    void unite(int a, int b)
    {
        a = find(a);
        b = find(b);
        if (a != b) parent[b] = a;
    }
};

// ---------- Helper: bilinear texture lookup with tiling ----------
static std::array<uint8_t, 3> sample_texture_bilinear(const std::vector<uint8_t>& texture,
    int tw, int th, int tchan,
    float texX, float texY)
{
    // Wrap texture coordinates for tiling
    texX = std::fmod(texX, static_cast<float>(tw));
    if (texX < 0) texX += tw;
    texY = std::fmod(texY, static_cast<float>(th));
    if (texY < 0) texY += th;

    // Calculate integer and fractional parts
    int x0 = static_cast<int>(std::floor(texX));
    int y0 = static_cast<int>(std::floor(texY));
    float fx = texX - x0;
    float fy = texY - y0;

    // Wrap coordinates for tiling
    int x1 = (x0 + 1) % tw;
    int y1 = (y0 + 1) % th;
    x0 = (x0 % tw + tw) % tw;  // Ensure positive index
    y0 = (y0 % th + th) % th;
    x1 = (x1 % tw + tw) % tw;
    y1 = (y1 % th + th) % th;

    // Helper function to safely get texel values
    auto get_texel = [&](int x, int y, int c) -> float
        {
            return static_cast<float>(texture[(y * tw + x) * tchan + c]);
        };

    // Sample 4 texels and interpolate
    std::array<uint8_t, 3> color{ 0, 0, 0 };
    for (int c = 0; c < 3; c++) {
        // Get 4 surrounding texels
        float c00 = get_texel(x0, y0, c);
        float c10 = get_texel(x1, y0, c);
        float c01 = get_texel(x0, y1, c);
        float c11 = get_texel(x1, y1, c);

        // Bilinear interpolation
        float val = (1.0f - fx) * (1.0f - fy) * c00 +
            fx * (1.0f - fy) * c10 +
            (1.0f - fx) * fy * c01 +
            fx * fy * c11;

        color[c] = static_cast<uint8_t>(std::clamp(val + 0.5f, 0.0f, 255.0f));
    }
    return color;
}

// ---------- Stereogram generator (union-find), with bilinear texture sampling ----------
void generate_sirds_unionfind_color(const std::vector<float>& depth, int width, int height,
    int eye_separation,
    const std::vector<uint8_t>& texture, int tw, int th, int tchan,
    std::vector<uint8_t>& out_rgb)
{
    out_rgb.assign(width * height * 3, 0);
    UnionFind uf(width);

    std::mt19937 rng(123456);
    std::uniform_int_distribution<int> distr(0, 255);

    for (int y = 0; y < height; ++y) {
        uf.reset(width);

        // Build unions based on depth-derived separation (CORRECTED mapping)
        for (int x = 0; x < width; ++x) {
            float d = depth[y * width + x]; // 0..1, 1=nearest
            // Map to separation (0..eye_separation) - CORRECTED to use 1-d
            int sep = static_cast<int>(std::round((1.0f - d) * eye_separation));
            //int sep = static_cast<int>(std::round(d * eye_separation));

            if (sep < 1) continue;  // Skip background/zero separation

            float leftF = x - sep * 0.5f;
            int left = static_cast<int>(std::floor(leftF + 0.5f));
            int right = left + sep;
            if (left >= 0 && right < width) {
                uf.unite(left, right);
            }
        }

        // Precompute colors for root pixels
        std::vector<std::array<uint8_t, 3>> rootColor(width);
        for (int x = 0; x < width; ++x) {
            if (uf.find(x) != x) continue;  // Only process root nodes

            if (!texture.empty() && tchan >= 3 && tw > 0 && th > 0) {
                // Calculate continuous texture coordinates (tiling)
                float texX = static_cast<float>(x) * (static_cast<float>(tw) / width);
                float texY = static_cast<float>(y) * (static_cast<float>(th) / height);

                // Sample with bilinear interpolation
                rootColor[x] = sample_texture_bilinear(texture, tw, th, tchan, texX, texY);
            }
            else {
                // Generate random color
                rootColor[x] = {
                    static_cast<uint8_t>(distr(rng)),
                    static_cast<uint8_t>(distr(rng)),
                    static_cast<uint8_t>(distr(rng))
                };
            }
        }

        // Propagate root colors to entire row
        for (int x = 0; x < width; ++x) {
            int root = uf.find(x);
            const auto& color = rootColor[root];
            int idx = (y * width + x) * 3;
            out_rgb[idx + 0] = color[0];
            out_rgb[idx + 1] = color[1];
            out_rgb[idx + 2] = color[2];
        }
    }
}

// ---------- Helper: load texture with stb (force 3 channels RGB) ----------
bool load_texture_rgb(const std::string &path, std::vector<uint8_t> &out, int &w, int &h, int &channels) {
    int orig_ch = 0;
    unsigned char *data = stbi_load(path.c_str(), &w, &h, &orig_ch, 3);
    if (!data) return false;
    channels = 3;
    out.assign(data, data + (w * h * channels));
    stbi_image_free(data);
    return true;
}

// Parse a floating point number
float parse_float(const std::string& str)
{
    float out;
    std::stringstream ss(str);
    ss >> out;
    return out;
}

// parse 3 floats
glm::vec3 parse_vec3(const std::string& str)
{
    std::stringstream ss(str);
    glm::vec3 v{ 0,0,0 };
    char comma;
    ss >> v.x >> comma >> v.y >> comma >> v.z;
    if (ss.fail()) {
        std::cerr << "Warning: Invalid vector format '" << str << "'. Using (0,0,0)\n";
    }
    return v;
}

void min_max(const float* array, uint32_t vcount,
    float& minx, float& maxx,
    float& miny, float& maxy,
    float& minz, float& maxz)
{
    for (size_t i = 0; i < vcount; ++i) {
        float x = array[i * 3 + 0];
        float y = array[i * 3 + 1];
        float z = array[i * 3 + 2];
        minx = std::min(minx, x); maxx = std::max(maxx, x);
        miny = std::min(miny, y); maxy = std::max(maxy, y);
        minz = std::min(minz, z); maxz = std::max(maxz, z);
    }
}

// apply 3d rotation of array
void rotate(float* array, uint32_t vcount, float xrot_deg, float yrot_deg, float zrot_deg)
{
    glm::mat4 rx = glm::rotate(glm::mat4(1.0f), glm::radians(xrot_deg), glm::vec3(1,0,0));
    glm::mat4 ry = glm::rotate(glm::mat4(1.0f), glm::radians(yrot_deg), glm::vec3(0,1,0));
    glm::mat4 rz = glm::rotate(glm::mat4(1.0f), glm::radians(zrot_deg), glm::vec3(0,0,1));
    glm::mat4 rot = rz * ry * rx; // ZYX order

    for (size_t i = 0; i < vcount; ++i) {
        glm::vec4 v(array[i * 3 + 0], array[i * 3 + 1], array[i * 3 + 2], 1.0f);
        v = rot * v;
        array[i * 3 + 0] = v.x;
        array[i * 3 + 1] = v.y;
        array[i * 3 + 2] = v.z;
    }
}

// translate array in 3d
void translate(float* array, uint32_t vcount, float xoffset, float yoffset, float zoffset)
{
    for (size_t i = 0; i < vcount; ++i) {
        array[i * 3 + 0] += xoffset;
        array[i * 3 + 1] += yoffset;
        array[i * 3 + 2] += zoffset;
    }
}


// scale an array 3d
void scale(float* array, uint32_t vcount, float xscale, float yscale, float zscale)
{
    for (size_t i = 0; i < vcount; ++i) {
        array[i * 3 + 0] *= xscale;
        array[i * 3 + 1] *= yscale;
        array[i * 3 + 2] *= zscale;
    }
}

void shear_mesh(float* array, uint32_t vcount, float sh_xy, float sh_xz, float sh_yz)
{
    // Shear matrix: [1 sh_xy sh_xz; 0 1 sh_yz; 0 0 1]
    glm::mat4 shearMat(1.0f);
    shearMat[1][0] = sh_xy; // y += sh_xy * x
    shearMat[2][0] = sh_xz; // z += sh_xz * x
    shearMat[2][1] = sh_yz; // z += sh_yz * y

    for (size_t i = 0; i < vcount; ++i) {
        glm::vec4 v(array[i * 3 + 0], array[i * 3 + 1], array[i * 3 + 2], 1.0f);
        v = shearMat * v;
        array[i * 3 + 0] = v.x;
        array[i * 3 + 1] = v.y;
        array[i * 3 + 2] = v.z;
    }
}

// ---------- Main with enhanced camera control ----------
int main(int argc, char** argv)
{
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " input.stl texture.png/null outprefix [options]\n";
        std::cerr << "Options (can appear in any order):\n";
        std::cerr << "  -w width         : Output width (default: " << defaultWidth << ")\n";
        std::cerr << "  -h height        : Output height (default: " << defaultHeight << ")\n";
        std::cerr << "  -sep eye_sep     : Eye separation in pixels (default: " << defaultEyeSep << ")\n";
        std::cerr << "  -fov fov_deg     : Field of view in degrees (default: " << defaultFov << ")\n";
        std::cerr << "  -persp 0|1       : 1 for perspective, 0 for orthographic (default: 1)\n";
        std::cerr << "  -cam x,y,z       : Camera position (default: auto)\n";
        std::cerr << "  -look x,y,z      : Look-at point (default: auto)\n";
        std::cerr << "  -rot x,y,z       : Rotate model (degrees, XYZ order)\n";
        std::cerr << "  -trans x,y,z     : Translate model\n";
        std::cerr << "  -sc x,y,z        : Scale model\n";
        std::cerr << "  -orthsc          : Orthographic scale\n";
        std::cerr << "  -shear x,y,z     : Shear model (XY,XZ,YZ)\n";
        std::cerr << "  -depthrange near far : Set normalized depth range (default: 1.0 0.0)\n";
        return 1;
    }

    std::string stlpath = argv[1];
    std::string texpath = argv[2];
    std::string outprefix = argv[3];

    // Default parameters
    int width = defaultWidth, height = defaultHeight;
    int eye_sep = defaultEyeSep;
    float fov = defaultFov;
    int perspective_flag = 1;
    glm::vec3 custom_cam_pos = { 0,0,0 };
    glm::vec3 custom_look_at = { 0,0,0 };
    glm::vec3 rot_deg = { 0, 0, 0 };
    glm::vec3 trans = { 0, 0, 0 };
    glm::vec3 sc = { 1, 1, 1 };
    glm::vec3 shear = {0, 0, 0}; // shear.x = XY, shear.y = XZ, shear.z = YZ
    float custom_orth_scale = 1;
    bool custom_cam_provided = false;
    bool custom_lookat_provided = false;
    bool custom_orth_scale_provided = false;
    float depth_near = 1.0f; // default: closest = 1.0
    float depth_far = 0.0f;  // default: farthest = 0.0
    float custom_depth_near = 1.0f; // default: closest = 1.0
    float custom_depth_far = 0.0f;  // default: farthest = 0.0
    bool custom_depth_range = false;

    // Parse named options
    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-w" && i + 1 < argc) {
            width = std::atoi(argv[++i]);
        } else if (arg == "-h" && i + 1 < argc) {
            height = std::atoi(argv[++i]);
        } else if (arg == "-sep" && i + 1 < argc) {
            eye_sep = std::atoi(argv[++i]);
        } else if (arg == "-fov" && i + 1 < argc) {
            fov = (float)atof(argv[++i]);
        } else if (arg == "-persp" && i + 1 < argc) {
            perspective_flag = std::atoi(argv[++i]);
        } else if (arg == "-cam" && i + 1 < argc) {
            custom_cam_pos = parse_vec3(argv[++i]);
            custom_cam_provided = true;
        } else if (arg == "-look" && i + 1 < argc) {
            custom_look_at = parse_vec3(argv[++i]);
            custom_lookat_provided = true;
        } else if (arg == "-rot" && i + 1 < argc) {
            rot_deg = parse_vec3(argv[++i]);
        } else if (arg == "-trans" && i + 1 < argc) {
            trans = parse_vec3(argv[++i]);
        } else if (arg == "-sc" && i + 1 < argc) {
            sc = parse_vec3(argv[++i]);
        }
        else if (arg == "-shear" && i + 1 < argc) {
            shear = parse_vec3(argv[++i]);
        }
        else if (arg == "-orthsc" && i + 1 < argc) {
            custom_orth_scale = parse_float(argv[++i]);
            custom_orth_scale_provided = true;
        }
        else if (arg == "-depthrange" && i + 2 < argc) {
            custom_depth_near = parse_float(argv[++i]);
            custom_depth_far = parse_float(argv[++i]);
            custom_depth_range = true;
        }
        else {
            std::cerr << "Unknown or incomplete option: " << arg << "\n";
            return 1;
        }
    }

    // Load STL
    stl mesh;
    if (mesh.read_stl(stlpath.c_str()) != 0) {
        std::cerr << "Failed to read STL: " << stlpath << "\n";
        return 1;
    }
    std::cout << "Loaded triangles: " << mesh.m_num_triangles << "\n";

    // Compute bounding box for automatic camera placement
    float minx = 1e9f, miny = 1e9f, minz = 1e9f;
    float maxx = -1e9f, maxy = -1e9f, maxz = -1e9f;
    const float* vdata = mesh.m_vectors.data();
    size_t vcount = mesh.m_num_triangles * 3;
    
    // Apply user transforms: scale, shear, rotate, translate
    scale((float*)vdata, vcount, sc.x, sc.y, sc.z);
    shear_mesh((float*)vdata, vcount, shear.x, shear.y, shear.z);
    rotate((float*)vdata, vcount, rot_deg.x, rot_deg.y, rot_deg.z);
    translate((float*)vdata, vcount, trans.x, trans.y, trans.z);
    min_max(vdata, vcount, minx, maxx, miny, maxy, minz, maxz);

    if (custom_depth_range) {
        depth_near = std::clamp(custom_depth_near, 0.0f, 1.0f);
        depth_far = std::clamp(custom_depth_far, 0.0f, 1.0f);
        if (depth_near < depth_far) {
            std::swap(depth_near, depth_far);
            std::cerr << "Note: Swapped depthrange to enforce near >= far\n";
        }
    } else {
        depth_near = std::clamp(depth_near, 0.0f, 1.0f);
        depth_far = std::clamp(depth_far, 0.0f, 1.0f);
    }

    glm::vec3 center = { (minx + maxx) * 0.5f, (miny + maxy) * 0.5f, (minz + maxz) * 0.5f };
    float spanx = maxx - minx; float spany = maxy - miny; float spanz = maxz - minz;
    float span = std::max({ spanx, spany, spanz, tolerance });

    // Setup camera
    Camera cam;
    cam.up = { 0,1,0 };
    cam.perspective = (perspective_flag != 0);
    cam.fov_deg = fov;

    // Set camera position and look-at based on user input or auto placement
    if (custom_cam_provided) {
        cam.position = custom_cam_pos;
    }
    else {
        // Auto placement: position behind model along Z-axis
        cam.position = { center.x, center.y, center.z + span * 2.5f };
    }

    if (custom_lookat_provided) {
        cam.look_at = custom_look_at;
    }
    else {
        cam.look_at = center;
    }

    // For orthographic camera
    float ortho_scale;
    if (custom_orth_scale_provided) {
        ortho_scale = custom_orth_scale;
    }
    else {

        float aspect = float(width) / height;
        float half_spanx = spanx / 2;
        float half_spany = spany / 2;
        ortho_scale = std::max(half_spanx / aspect, half_spany) * 1.2f;
    }

    std::cout << "Camera settings:\n";
    std::cout << "  Position: (" << cam.position.x << ", " << cam.position.y << ", " << cam.position.z << ")\n";
    std::cout << "  Look-at: (" << cam.look_at.x << ", " << cam.look_at.y << ", " << cam.look_at.z << ")\n";
    std::cout << "  Perspective: " << cam.perspective << "\n";
    std::cout << "  FOV: " << cam.fov_deg << " degrees\n";
    if (!cam.perspective) {
        std::cout << "  Orthographic scale: " << ortho_scale << "\n";
    }

    // Generate depth map
    float zmin, zmax;
    auto depth = generate_depth_map(mesh, width, height, cam, ortho_scale, zmin, zmax, depth_near, depth_far);
    std::cout << "Depth zmin=" << zmin << " zmax=" << zmax << "\n";

    // Save depth visualization image
    std::vector<uint8_t> depth_vis(width * height * 3);
    for (int i = 0; i < width * height; ++i) {
        uint8_t v = (uint8_t)std::round(std::clamp(depth[i], 0.0f, 1.0f) * 255.0f);
        depth_vis[i * 3 + 0] = v; depth_vis[i * 3 + 1] = v; depth_vis[i * 3 + 2] = v;
    }
    std::string depth_out = outprefix + "_depth.png";
    stbi_write_png(depth_out.c_str(), width, height, 3, depth_vis.data(), width * 3);
    std::cout << "Wrote depth visualization: " << depth_out << "\n";

    // Load texture if provided
    std::vector<uint8_t> texture;
    int tw = 0, th = 0, tchan = 0;
    bool haveTex = false;

    if (texpath != "null") {
        if (load_texture_rgb(texpath, texture, tw, th, tchan)) {
            haveTex = true;
            std::cout << "Loaded texture " << texpath << " (" << tw << "x" << th << " ch=" << tchan << ")\n";
        } else {
            std::cout << "Failed to load texture '" << texpath << "'. Falling back to random dots.\n";
        }
    }
    else {
        std::cout << "Using random-dot texture.\n";
    }

    // Generate stereogram
    std::vector<uint8_t> sirds_rgb;
    generate_sirds_unionfind_color(depth, width, height, eye_sep,
        texture, tw, th, tchan,
        sirds_rgb);

    std::string sirds_out = outprefix + "_sirds.png";
    stbi_write_png(sirds_out.c_str(), width, height, 3, sirds_rgb.data(), width * 3);
    std::cout << "Wrote stereogram: " << sirds_out << "\n";

    return 0;
}
