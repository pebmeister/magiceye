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
#include <cmath> // Added for pow and abs

// Project-specific Headers
#include "TextureSampler.h"
#include "Options.h"

/**
 * @class SIRDSGenerator
 * @brief A header-only library for generating Single Image Random Dot Stereograms (SIRDS) from a depth map.
 *
 * @details
 * This class transforms a 2D grayscale depth map into a 3D stereoscopic image. The human brain
 * perceives depth by correlating the patterns between the left and right eye views, which are
 * horizontally offset according to the depth map.
 *
 * The core algorithm uses a Union-Find (Disjoint Set Union) data structure to intelligently link
 * pixels that must share the same color to create a consistent pattern for each eye, ensuring
 * the depth illusion is maintained.
 *
 * Key Features:
 * - Depth-controlled pixel separation for stereoscopic effect.
 * - Efficient Union-Find algorithm for robust, cycle-free linking.
 * - Support for texture sampling or random color generation.
 * - Color propagation for object consistency across scanlines.
 * - Edge smoothing to reduce visual noise in foreground objects.
 *
 * Usage:
 * 1. Prepare a depth map (`std::vector<float>`) where values range from 0.0 (far) to 1.0 (near).
 * 2. Optionally, prepare a texture image for coloring.
 * 3. Call `SIRDSGenerator::generate(...)` with the desired parameters.
 * 4. The resulting 3-channel RGB image will be populated in the `out_rgb` vector.
 *
 * Example:
 * @code
 * std::vector<float> depth_map(width * height, 0.5f);
 * std::vector<uint8_t> texture; // Optional
 * std::vector<uint8_t> out_rgb;
 * auto options = std::make_shared<Options>();
 * SIRDSGenerator::generate(depth_map, width, height, 160, texture, tw, th, tchan, out_rgb, 1.0f, 1.0f, 0.6f, options);
 * // out_rgb now contains the stereogram image
 * @endcode
 *
 * @note The output image is best viewed using the "parallel viewing" or "cross-eyed" method.
 */
class SIRDSGenerator {
public:
    /// Shared pointer to the global options/configuration for the generator.
    static std::shared_ptr<Options> options;

public:
    /**
     * @enum Method
     * @brief Algorithm selection for SIRDS generation.
     *
     * - UnionFind: Uses a disjoint-set data structure for robust, cycle-free pixel linking.
     * - Recursive: [Not implemented] Placeholder for future alternative algorithms.
     */
    enum class Method {
        UnionFind,  ///< The primary, robust method using a Union-Find data structure. Efficient and prevents cycles.
        Recursive   ///< [Placeholder] A potential alternative method. Not currently implemented.
    };

    /**
     * @brief Main entry point for generating a SIRDS image.
     * @details
     * Selects the generation method (currently only UnionFind is implemented) and produces a stereogram
     * from a normalized depth map. See generateUnionFind for algorithm details.
     * @param depth Normalized depth map [0.0, 1.0], 1.0 = foreground, 0.0 = background.
     * @param width Image width in pixels.
     * @param height Image height in pixels.
     * @param eye_separation Maximum horizontal separation for stereoscopic effect (pixels).
     * @param texture Optional texture image for coloring. If empty, random colors are used.
     * @param tw Texture width.
     * @param th Texture height.
     * @param tchan Texture channels (e.g., 3=RGB).
     * @param[out] out_rgb Output RGB image buffer, resized to width*height*3.
     * @param texture_brightness Brightness multiplier for texture sampling.
     * @param texture_contrast Contrast adjustment for texture sampling.
     * @param bg_separation Background separation scaling factor.
     * @param opt Shared pointer to Options object (algorithm parameters).
     * @param method Generation method (default: UnionFind).
     */
    static void generate(const std::vector<float>& depth, int width, int height,
        int eye_separation, const std::vector<uint8_t>& texture,
        int tw, int th, int tchan, std::vector<uint8_t>& out_rgb,
        float texture_brightness, float texture_contrast,
        float bg_separation, const std::shared_ptr<Options>& opt, Method method = Method::UnionFind)
    {
        options = opt; // Set the global options for this generation run.

        // For now, always use the proven UnionFind method.
        // The Recursive method can be implemented later when time permits.
        generateUnionFind(depth, width, height, eye_separation, texture,
            tw, th, tchan, out_rgb, texture_brightness,
            texture_contrast, bg_separation);
    }

    /**
     * @brief Generates a SIRDS image using the Union-Find algorithm.
     * @details
     * Implements the core stereogram generation algorithm:
     *  - Adjusts depth values for background separation.
     *  - Calculates pixel separation based on depth and gamma.
     *  - Links pixels using Union-Find to enforce stereoscopic constraints.
     *  - Assigns colors via texture sampling or random generation.
     *  - Smooths edges in the foreground to reduce noise.
     * @param depth Normalized depth map.
     * @param width Image width.
     * @param height Image height.
     * @param eye_separation Maximum separation for stereoscopic effect.
     * @param texture Optional texture image.
     * @param tw Texture width.
     * @param th Texture height.
     * @param tchan Texture channels.
     * @param[out] out_rgb Output RGB image buffer.
     * @param texture_brightness Brightness multiplier.
     * @param texture_contrast Contrast adjustment.
     * @param bg_separation Background separation scaling factor.
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
        // A fixed seed ensures the same input always produces the same output, which is good for debugging.
        std::mt19937 rng(123456);
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
    /**
     * @class UnionFind
     * @brief A simple, efficient implementation of the Union-Find (Disjoint Set Union) data structure.
     *
     * @details
     * This internal class is used to group pixels on a single scanline that must share the same color.
     * It provides two operations:
     * - `find(x)`: Determines the root representative of the set containing element `x`.
     * - `unite(a, b)`: Merges the sets containing elements `a` and `b`.
     *
     * Path compression is used in `find()` to keep the tree flat, ensuring near-constant time
     * amortized complexity for these operations.
     */
    class UnionFind {
    private:
        std::vector<int> parent; ///< Tracks the parent of each element. The root of a set points to itself.

    public:
        /// Constructor initializes the structure for `n` elements, each in its own set.
        UnionFind(int n = 0) { reset(n); }

        /// Resets the structure for `n` elements, making each element its own parent.
        void reset(int n)
        {
            parent.resize(n);
            for (int i = 0; i < n; ++i) parent[i] = i;
        }

        /// Finds the root parent of element `x`, applying path compression for efficiency.
        /// @return The root representative of the set containing `x`.
        int find(int x)
        {
            return parent[x] == x ? x : (parent[x] = find(parent[x]));
        }

        /// Merges the sets containing elements `a` and `b`.
        /// After this operation, `find(a)` will equal `find(b)`.
        void unite(int a, int b)
        {
            a = find(a);
            b = find(b);
            if (a != b) parent[b] = a; // Attach the root of `b` to the root of `a`.
        }
    };

    /// Scales depth values to control the overall depth effect and background separation.
    static std::vector<float> adjustDepthRange(const std::vector<float>& depth, float bg_separation)
    {
        std::vector<float> adjusted_depth(depth.size());
        for (size_t i = 0; i < depth.size(); i++) {
            adjusted_depth[i] = depth[i] * (1.0f - bg_separation);
        }
        return adjusted_depth;
    }

    /// Calculates the horizontal separation (in pixels) for each pixel based on its depth.
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
                float t = pow(std::abs(d - focus_depth) * 2.0f, 1.5f);
                float sep_scale = 1.0f + t * 0.5f;

                // Core formula: separation increases as the object gets closer (1.0f - d).
                float sep_float = min_separation + (max_separation - min_separation) *
                    pow(1.0f - d, options->depth_gamma) * sep_scale;

                // Clamp the final value to the valid range and store it.
                // BUG FIX: Corrected order of std::min and std::max arguments.
                separation_map[y * width + x] = std::clamp(static_cast<int>(std::round(sep_float)), min_separation, max_separation);
            }
        }
        return separation_map;
    }

    /// Processes a single horizontal scanline (row) of the image.
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

    /// Links pixels together on the current scanline based on depth and separation.
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
                if (d > options->foreground_threshold && x > 0) {
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

    /// Assigns a color to each root pixel, propagating from neighbors if possible.
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
            if (d > options->foreground_threshold) {
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

    /// Attempts to find a color for the current root from a nearby, already-colored root.
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

    /// Applies the root's color to all pixels in its group for this scanline.
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

    /// Applies a simple low-pass filter to foreground pixels to reduce visual noise.
    static void applyEdgeSmoothing(const std::vector<float>& adjusted_depth,
        std::vector<uint8_t>& out_rgb, int width, int height)
    {
        // Iterate through interior pixels (ignore the border for simplicity).
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                float d = adjusted_depth[y * width + x];
                // Only smooth pixels that are in the foreground.
                if (d > options->smoothThreshold) {
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
            // Sum of center pixel (weight 6) and 6 neighbors (weight 1 each) = total weight 12.
            int sum = out_rgb[idx + c] * 6;
            // Add the four cardinal neighbors
            sum += out_rgb[((y - 1) * width + x) * 3 + c]; // Up
            sum += out_rgb[((y + 1) * width + x) * 3 + c]; // Down
            sum += out_rgb[(y * width + (x - 1)) * 3 + c]; // Left
            sum += out_rgb[(y * width + (x + 1)) * 3 + c]; // Right
            // Add two diagonal neighbors (optional, improves smoothness)
            sum += out_rgb[((y - 1) * width + (x - 1)) * 3 + c]; // Up-Left
            sum += out_rgb[((y + 1) * width + (x + 1)) * 3 + c]; // Down-Right

            out_rgb[idx + c] = static_cast<uint8_t>(sum / options->smoothWeight);
        }
    }
};

// Definition and initialization of the static class member.
// 'inline' is required in C++17 to avoid One Definition Rule (ODR) violations in header-only libraries.
inline std::shared_ptr<Options> SIRDSGenerator::options = nullptr;
