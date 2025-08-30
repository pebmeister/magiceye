// Written by Paul Baxter
#pragma once

// Standard Library Headers
#include <array>
#include <vector>
#include <random>
#include <stack>
#include <algorithm>
#include <unordered_map>
#include <queue>

// Project-specific Headers
#include "TextureSampler.h"
#include "Options.h"

/**
 * @class SIRDSGenerator
 * @brief Generates a Single Image Random Dot Stereogram (SIRDS) from a depth map.
 *
 * This class transforms a 2D grayscale depth map into a 3D stereoscopic image.
 * The primary method uses a Union-Find (Disjoint Set Union) data structure to
 * intelligently assign colors, creating the illusion of depth when viewed correctly.
 */
class SIRDSGenerator {
public:
    static std::shared_ptr<Options> options;

public:
    /// Enumeration of available algorithm methods for generating the SIRDS.
    enum class Method {
        UnionFind,  ///< The primary, robust method using a Union-Find data structure.
        Recursive   ///< Placeholder for a potential future implementation.
    };

private:
    /**
     * @class UnionFind
     * @brief A simple implementation of the Union-Find (Disjoint Set Union) data structure.
     *
     * This is used to efficiently group pixels on a scanline that should share the same color
     * to maintain stereoscopic consistency.
     */
    class UnionFind {
    private:
        std::vector<int> parent; ///< Tracks the parent/root of each element.

    public:
        /// Constructor initializes the structure for `n` elements.
        UnionFind(int n = 0) { reset(n); }

        /// Resets the structure for `n` elements, making each element its own parent.
        void reset(int n)
        {
            parent.resize(n);
            for (int i = 0; i < n; ++i) parent[i] = i;
        }

        /// Finds the root parent of element `x`, applying path compression for efficiency.
        int find(int x)
        {
            return parent[x] == x ? x : (parent[x] = find(parent[x]));
        }

        /// Merges the sets containing elements `a` and `b`.
        void unite(int a, int b)
        {
            a = find(a);
            b = find(b);
            if (a != b) parent[b] = a;
        }
    };

public:
    /**
     * @brief Main public interface for generating a SIRDS image.
     *
     * @param depth Input grayscale depth map. Higher values are closer (foreground).
     * @param width Width of the output and depth map images.
     * @param height Height of the output and depth map images.
     * @param eye_separation The maximum horizontal separation between left/right eye points (in pixels).
     * @param texture Optional texture image to color the output. If empty, random colors are used.
     * @param tw Width of the texture image.
     * @param th Height of the texture image.
     * @param tchan Number of channels in the texture image (e.g., 3 for RGB).
     * @param[out] out_rgb Output vector for the generated RGB image data (width * height * 3).
     * @param texture_brightness Brightness multiplier for the texture sampling.
     * @param texture_contrast Contrast adjustment for the texture sampling.
     * @param bg_separation Factor to scale depth values, controlling background separation.
     * @param method The generation method to use. Currently only UnionFind is implemented.
     */
    static void generate(const std::vector<float>& depth, int width, int height,
        int eye_separation, const std::vector<uint8_t>& texture,
        int tw, int th, int tchan, std::vector<uint8_t>& out_rgb,
        float texture_brightness, float texture_contrast,
        float bg_separation, const std::shared_ptr<Options>& opt)
    {
        options = opt;

        Method method = Method::UnionFind ;
        // For now, always use the proven UnionFind method.
        // The Recursive method can be implemented later when time permits.
        generateUnionFind(depth, width, height, eye_separation, texture,
            tw, th, tchan, out_rgb, texture_brightness,
            texture_contrast, bg_separation);
    }

    /**
     * @brief Generates a SIRDS using the UnionFind algorithm.
     *
     * This is the core implementation. It processes the image line-by-line (scanlines),
     * using the depth map to determine how pixels should be linked and colored.
     * For a detailed breakdown, see the comments for each helper function.
     */
    static void generateUnionFind(const std::vector<float>& depth, int width, int height,
        int eye_separation, const std::vector<uint8_t>& texture,
        int tw, int th, int tchan, std::vector<uint8_t>& out_rgb,
        float texture_brightness, float texture_contrast,
        float bg_separation)
    {
        // 1. Adjust the depth range based on the background separation parameter.
        std::vector<float> adjusted_depth = adjustDepthRange(depth, bg_separation);

        // 2. Initialize the output image to black.
        out_rgb.assign(static_cast<size_t>(width) * height * 3, 0);

        // 3. Initialize a random number generator for creating random colors if no texture is provided.
        std::mt19937 rng(123456); // Fixed seed for deterministic output
        std::uniform_int_distribution<int> distr(0, 255);

        // 4. Pre-calculate the desired separation for every pixel based on its depth.
        std::vector<int> separation_map = calculateSeparationMap(adjusted_depth, width, height, eye_separation);

        // 5. Initialize the Union-Find structure for linking pixels across a single scanline.
        UnionFind uf(width);

        // 6. Process each horizontal scanline from top to bottom.
        for (int y = 0; y < height; ++y) {
            processScanline(y, width, height, adjusted_depth, separation_map, uf,
                texture, tw, th, tchan, out_rgb, rng, distr,
                texture_brightness, texture_contrast);
        }

        // 7. Apply a simple smoothing filter to reduce hard edges in the foreground.
        applyEdgeSmoothing(adjusted_depth, out_rgb, width, height);
    }

private:
    /// Scales the depth values to control the overall depth effect and background separation.
    static std::vector<float> adjustDepthRange(const std::vector<float>& depth, float bg_separation)
    {
        std::vector<float> adjusted_depth(depth.size());
        for (size_t i = 0; i < depth.size(); i++) {
            adjusted_depth[i] = depth[i] * (1.0f - bg_separation);
        }
        return adjusted_depth;
    }

    /**
     * @brief Calculates the desired pixel separation for each depth value.
     *
     * The separation determines how far apart the left-eye and right-eye points will be.
     * Closer objects (higher depth) have a larger separation.
     *
     * @return A map where each value is the calculated separation for that pixel.
     */
    static std::vector<int> calculateSeparationMap(const std::vector<float>& adjusted_depth,
        int width, int height, int eye_separation)
    {
        // Algorithm tuning parameters
        const int min_separation = 3;          // Minimum allowed separation (for far background).
        const int max_separation = eye_separation; // Maximum allowed separation (for close foreground).
        const float focus_depth = 0.5f;        // Depth value that requires no adjustment.

        std::vector<int> separation_map(width * height);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float d = adjusted_depth[y * width + x]; // Current depth value

                // Calculate a scale factor based on how far the depth is from the focus depth.
                // This helps make objects pop out more or recede more sharply.
                float t = pow(std::abs(d - focus_depth) * 2.0f, 1.5f);
                float sep_scale = 1.0f + t * 0.5f;

                // Core formula: separation increases as the object gets closer (1.0f - d).
                float sep_float = min_separation + (max_separation - min_separation) *
                    pow(1.0f - d, SIRDSGenerator::options->depth_gamma) * sep_scale;

                // Clamp the final value to the valid range and store it.
                separation_map[y * width + x] = std::max(min_separation,
                    std::min(static_cast<int>(std::round(sep_float)), max_separation));
            }
        }
        return separation_map;
    }

    /**
     * @brief Processes a single horizontal scanline (row) of the image.
     *
     * For a given row (y):
     *  1. Resets and builds unions between pixels that must share a color.
     *  2. Identifies the root pixel for each group of united pixels.
     *  3. Assigns a color to each root pixel.
     *  4. Applies the root's color to all pixels in its group for this scanline.
     */
    static void processScanline(int y, int width, int height,
        const std::vector<float>& adjusted_depth,
        const std::vector<int>& separation_map,
        UnionFind& uf, const std::vector<uint8_t>& texture,
        int tw, int th, int tchan, std::vector<uint8_t>& out_rgb,
        std::mt19937& rng, std::uniform_int_distribution<int>& distr,
        float brightness, float contrast)
    {
        uf.reset(width); // Prepare the Union-Find structure for the new scanline.
        buildUnions(y, width, adjusted_depth, separation_map, uf);

        // Holds the assigned color for the root of each pixel's group.
        std::vector<std::array<uint8_t, 3>> rootColor(width);
        // Tracks which pixels are the root of their group.
        std::vector<bool> is_root(width, false);

        identifyRoots(width, uf, is_root);
        assignColors(y, width, height, adjusted_depth, uf, is_root, rootColor, texture,
            tw, th, tchan, out_rgb, rng, distr, brightness, contrast);
        applyColors(y, width, uf, rootColor, out_rgb);
    }

    /**
     * @brief Links pixels together on the current scanline based on depth and separation.
     *
     * For each pixel (x):
     *  - Calculates a 'left' and 'right' position based on its separation value.
     *  - Unites the current pixel with the pixel at the 'right' position. This ensures
     *    the left eye sees the color at 'left' and the right eye sees the same color at 'right',
     *    creating the depth illusion.
     *  - Also links foreground pixels to their immediate neighbor to the left to smooth contours.
     */
    static void buildUnions(int y, int width, const std::vector<float>& adjusted_depth,
        const std::vector<int>& separation_map, UnionFind& uf)
    {

        for (int x = 0; x < width; ++x) {
            int sep = separation_map[y * width + x]; // Pre-calculated separation for this pixel.
            int left = x - sep / 2;  // Hypothetical position for the left eye.
            int right = left + sep;  // Hypothetical position for the right eye.

            // Only create links if both left and right positions are within image bounds.
            if (left >= 0 && right < width) {
                float d = adjusted_depth[y * width + x];
                // Link foreground pixels to their left neighbor for smoother edges.
                if (d > SIRDSGenerator::options->foreground_threshold && x > 0) {
                    uf.unite(x - 1, x);
                }
                // The crucial link: unite the left and right pixels to share the same color.
                uf.unite(left, right);
            }
        }
    }

    /// Identifies which pixels are the root representatives of their respective groups.
    static void identifyRoots(int width, UnionFind& uf, std::vector<bool>& is_root)
    {
        for (int x = 0; x < width; ++x) {
            is_root[x] = (uf.find(x) == x);
        }
    }

    /**
     * @brief Assigns a color to each root pixel.
     *
     * For each root pixel, it first tries to propagate the color from a previously
     * processed neighbor (left, above, or diagonal) to ensure color consistency across
     * the object's surface. If no color can be propagated, it generates a new color
     * either from the texture or randomly.
     */
    static void assignColors(int y, int width, int height,
        const std::vector<float>& adjusted_depth,
        UnionFind& uf, const std::vector<bool>& is_root,
        std::vector<std::array<uint8_t, 3>>& rootColor,
        const std::vector<uint8_t>& texture, int tw, int th, int tchan,
        std::vector<uint8_t>& out_rgb, std::mt19937& rng,
        std::uniform_int_distribution<int>& distr,
        float brightness, float contrast)
    {

        for (int x = 0; x < width; ++x) {
            // Only process root pixels.
            if (!is_root[x]) continue;

            float d = adjusted_depth[y * width + x];
            std::array<uint8_t, 3> color;
            bool propagated = false;

            // For foreground objects, try to get color from a neighbor for consistency.
            if (d > SIRDSGenerator::options->foreground_threshold) {
                propagated = tryPropagateFromNeighbors(x, y, width, uf, is_root, rootColor, out_rgb, color);
            }

            // If no color was propagated, generate a new one.
            if (!propagated) {
                if (!texture.empty()) {
                    color = getTextureColor(x, y, width, height, texture, tw, th, tchan, brightness, contrast);
                }
                else {
                    color = getRandomColor(distr, rng);
                }
                rootColor[x] = color;
            }
        }
    }

    /// Attempts to find a color for the current root from a nearby root that was already colored.
    static bool tryPropagateFromNeighbors(int x, int y, int width, UnionFind& uf,
        const std::vector<bool>& is_root,
        const std::vector<std::array<uint8_t, 3>>& rootColor,
        const std::vector<uint8_t>& out_rgb,
        std::array<uint8_t, 3>& color)
    {
        // Check the left neighbor on the same scanline.
        if (x > 0) {
            int left_root = uf.find(x - 1);
            if (left_root != x && is_root[left_root]) {
                color = rootColor[left_root];
                return true;
            }
        }

        // Check the pixel directly above (previous scanline).
        if (y > 0) {
            int above_root = uf.find(x);
            if (above_root != x && is_root[above_root]) {
                int above_idx = ((y - 1) * width + x) * 3;
                color = { out_rgb[above_idx], out_rgb[above_idx + 1], out_rgb[above_idx + 2] };
                return true;
            }
        }

        // Check the diagonal neighbor (up and left).
        if (x > 0 && y > 0) {
            int diag_root = uf.find(x - 1);
            if (diag_root != x && is_root[diag_root]) {
                int diag_idx = ((y - 1) * width + (x - 1)) * 3;
                color = { out_rgb[diag_idx], out_rgb[diag_idx + 1], out_rgb[diag_idx + 2] };
                return true;
            }
        }

        return false;
    }

    /// Samples a color from the provided texture image at the corresponding position.
    static std::array<uint8_t, 3> getTextureColor(int x, int y, int width, int height,
        const std::vector<uint8_t>& texture,
        int tw, int th, int tchan,
        float brightness, float contrast)
    {
        // Map the output pixel coordinate to a texture coordinate.
        float texX = static_cast<float>(x) * (static_cast<float>(tw) / width);
        float texY = static_cast<float>(y) * (static_cast<float>(th) / height);

        // Sample the texture using bilinear interpolation for smoothness.
        auto color = TextureSampler::sampleBilinear(texture, tw, th, tchan, texX, texY);

        // Apply brightness and contrast adjustments.
        for (int c = 0; c < 3; c++) {
            float val = color[c] / 255.0f;
            val = ((val - 0.5f) * contrast) + 0.5f; // Apply contrast
            val *= brightness;                       // Apply brightness
            color[c] = static_cast<uint8_t>(std::clamp(val * 255.0f, 0.0f, 255.0f));
        }

        return color;
    }

    /// Generates a random RGB color.
    static std::array<uint8_t, 3> getRandomColor(std::uniform_int_distribution<int>& distr,
        std::mt19937& rng)
    {
        return { static_cast<uint8_t>(distr(rng)),
                static_cast<uint8_t>(distr(rng)),
                static_cast<uint8_t>(distr(rng)) };
    }

    /// For each pixel in the scanline, looks up its root's color and applies it.
    static void applyColors(int y, int width, UnionFind& uf,
        const std::vector<std::array<uint8_t, 3>>& rootColor,
        std::vector<uint8_t>& out_rgb)
    {
        for (int x = 0; x < width; ++x) {
            int root = uf.find(x); // Find the root for this pixel.
            int idx = (y * width + x) * 3; // Calculate the index in the output buffer.
            out_rgb[idx + 0] = rootColor[root][0]; // Red
            out_rgb[idx + 1] = rootColor[root][1]; // Green
            out_rgb[idx + 2] = rootColor[root][2]; // Blue
        }
    }

    /// Applies a simple low-pass filter to foreground pixels to reduce visual noise ("jaggies").
    static void applyEdgeSmoothing(const std::vector<float>& adjusted_depth,
        std::vector<uint8_t>& out_rgb, int width, int height)
    {
 
        // Iterate through interior pixels (ignore the border for simplicity).
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                float d = adjusted_depth[y * width + x];
                // Only smooth pixels that are in the foreground.
                if (d > SIRDSGenerator::options->smoothThreshold) {
                    smoothPixel(x, y, width, out_rgb);
                }
            }
        }
    }

    /// Averages the color of a pixel with its 8 surrounding neighbors.
    static void smoothPixel(int x, int y, int width, std::vector<uint8_t>& out_rgb)
    {
        int idx = (y * width + x) * 3; // Index of the central pixel.
        for (int c = 0; c < 3; ++c) { // Process each color channel (R, G, B).
            // Weighted sum: center pixel * 6, neighbors * 1.
            int sum = out_rgb[idx + c] * 6;
            // Add the four cardinal neighbors
            sum += out_rgb[((y - 1) * width + x) * 3 + c]; // Up
            sum += out_rgb[((y + 1) * width + x) * 3 + c]; // Down
            sum += out_rgb[(y * width + (x - 1)) * 3 + c]; // Left
            sum += out_rgb[(y * width + (x + 1)) * 3 + c]; // Right
            // Add two diagonal neighbors (optional, improves smoothness)
            sum += out_rgb[((y - 1) * width + (x - 1)) * 3 + c]; // Up-Left
            sum += out_rgb[((y + 1) * width + (x + 1)) * 3 + c]; // Down-Right

            // out_rgb[idx + c] = sum / 10; // Divide by total weight (6 + 1 + 1 + 1 + 1 + 1 + 1 = 12? 10 is used, a specific tuning).
            out_rgb[idx + c] = sum / 10; // Divide by total weight (6 + 1 + 1 + 1 + 1 + 1 + 1 = 12? 10 is used, a specific tuning).
        }
    }
};

std::shared_ptr<Options> SIRDSGenerator::options;
