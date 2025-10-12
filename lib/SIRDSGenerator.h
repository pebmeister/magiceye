#pragma once

#include <array>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

#include "TextureSampler.h"
#include "Options.h"
#include "EdgeSmoother.h"
#include "BlueNoise.h"
#include "SeparationCalibrator.h"

    class SIRDSGenerator {
    public:
        enum class Method {
            UnionFind,
        };

        static void generate(const std::vector<float>& depth, int width, int height,
            int eye_separation, const std::vector<uint8_t>& texture,
            int tw, int th, int tchan, std::vector<uint8_t>& out_rgb,
            float texture_brightness, float texture_contrast,
            float bg_separation, const std::shared_ptr<Options>& opt,
            Method method = Method::UnionFind)
        {
            if (!opt) throw std::runtime_error("SIRDSGenerator::generate: Options is null.");

            generateUnionFind(depth, width, height, eye_separation, texture,
                tw, th, tchan, out_rgb, texture_brightness,
                texture_contrast, bg_separation, *opt);
        }

    private:
        class UnionFind {
        private:
            std::vector<int> parent;
        public:
            UnionFind(int n = 0) { reset(n); }
            void reset(int n)
            {
                parent.resize(n);
                for (int i = 0; i < n; ++i) parent[i] = i;
            }
            int find(int x)
            {
                int r = x;
                while (parent[r] != r) r = parent[r];
                while (parent[x] != x) {
                    int p = parent[x];
                    parent[x] = r;
                    x = p;
                }
                return r;
            }
            void unite(int a, int b)
            {
                a = find(a);
                b = find(b);
                if (a != b) parent[b] = a;
            }
        };

        static void generateUnionFind(const std::vector<float>& depth, int width, int height,
            int eye_separation, const std::vector<uint8_t>& texture,
            int tw, int th, int tchan, std::vector<uint8_t>& out_rgb,
            float texture_brightness, float texture_contrast,
            float bg_separation, const Options& options)
        {
            std::vector<float> adjusted_depth = adjustDepthRange(depth, bg_separation);
            out_rgb.assign(static_cast<size_t>(width) * height * 3, 0);

            std::mt19937 rng;
            if (options.rng_seed >= 0) {
                rng.seed(static_cast<uint32_t>(options.rng_seed));
            }
            else {
                std::random_device rd;
                rng.seed(rd());
            }
            std::uniform_int_distribution<int> distr(0, 255);

            std::vector<uint8_t> noiseRGB;
            if (texture.empty()) {
                noiseRGB = BlueNoise::generateRGB(width, height, static_cast<uint32_t>(options.rng_seed < 0 ? rng() : options.rng_seed));
            }

            float focus_depth = SeparationCalibrator::estimateFocusDepth(adjusted_depth, width, height);
            std::vector<int> separation_map = calculateSeparationMap(adjusted_depth, width, height, eye_separation, options, focus_depth);

            UnionFind uf(width);

            // Single-row cache; index as x*3
            std::vector<uint8_t> prev_row_colors(static_cast<size_t>(width) * 3, 0);
            bool have_prev = false;

            for (int y = 0; y < height; ++y) {
                processScanline(y, width, height, adjusted_depth, separation_map, uf,
                    texture, tw, th, tchan, noiseRGB, out_rgb, prev_row_colors, have_prev, rng, distr,
                    texture_brightness, texture_contrast, options);

                std::copy(out_rgb.begin() + static_cast<size_t>(y) * width * 3,
                    out_rgb.begin() + static_cast<size_t>(y + 1) * width * 3,
                    prev_row_colors.begin());
                have_prev = true;
            }

            if (options.smoothEdges) {
                EdgeSmoother::applyEdgeSmoothing(adjusted_depth, out_rgb, options.smoothThreshold, options.smoothWeight, width, height);
            }
        }

        static std::vector<float> adjustDepthRange(const std::vector<float>& depth, float bg_separation)
        {
            std::vector<float> adjusted_depth(depth.size());
            const float scale = std::max(0.0f, 1.0f - bg_separation);
            for (size_t i = 0; i < depth.size(); i++) {
                // Preserve original upper range behavior (no clamp to 1.0)
                adjusted_depth[i] = std::max(0.0f, depth[i] * scale);
            }
            return adjusted_depth;
        }

        static std::vector<int> calculateSeparationMap(const std::vector<float>& adjusted_depth,
            int width, int height, int eye_separation,
            const Options& options, float focus_depth)
        {
            const int min_separation = 2;
            const int max_separation = eye_separation;

            std::vector<int> separation_map(static_cast<size_t>(width) * height);

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    float d = adjusted_depth[y * width + x];

                    float t = std::pow(std::abs(d - focus_depth) * 2.0f, 1.5f);
                    float sep_scale = 1.0f + t * 0.5f;

                    // Inverted z: larger depth -> closer -> smaller separation
                    float sep_float = min_separation +
                        (max_separation - min_separation) *
                        std::pow(1.0f - d, options.depth_gamma) * sep_scale;

                    separation_map[y * width + x] = std::clamp(
                        static_cast<int>(std::round(sep_float)),
                        min_separation,
                        max_separation
                    );
                }
            }
            return separation_map;
        }

        static void processScanline(int y, int width, int height,
            const std::vector<float>& adjusted_depth,
            const std::vector<int>& separation_map,
            UnionFind& uf, const std::vector<uint8_t>& texture,
            int tw, int th, int tchan, const std::vector<uint8_t>& noiseRGB,
            std::vector<uint8_t>& out_rgb, const std::vector<uint8_t>& prev_row_colors,
            bool have_prev, std::mt19937& rng, std::uniform_int_distribution<int>& distr,
            float brightness, float contrast, const Options& options)
        {
            uf.reset(width);
            buildUnions(y, width, adjusted_depth, separation_map, uf, options);

            std::vector<std::array<uint8_t, 3>> rootColor(width);
            std::vector<bool> is_root(width, false);
            std::vector<bool> rootHasColor(width, false);

            identifyRoots(width, uf, is_root);
            assignColors(y, width, height, adjusted_depth, uf, is_root, rootHasColor, rootColor, texture,
                tw, th, tchan, noiseRGB, out_rgb, prev_row_colors, have_prev, rng, distr, brightness, contrast, options);
            applyColors(y, width, uf, rootColor, out_rgb);
        }

        static void buildUnions(int y, int width, const std::vector<float>& adjusted_depth,
            const std::vector<int>& separation_map, UnionFind& uf,
            const Options& options)
        {
            const int rowOffset = y * width;

            for (int x = 0; x < width; ++x) {
                int sep = separation_map[rowOffset + x];
                int left = x - sep / 2;
                int right = left + sep;

                if (left >= 0 && right < width) {
                    const float d = adjusted_depth[rowOffset + x];

                    if (options.occlusion) {
                        const float dl = adjusted_depth[rowOffset + left];
                        const float dr = adjusted_depth[rowOffset + right];

                        // IMPORTANT: Only skip linking when BOTH sides are nearer (inverted z: larger is closer).
                        // Using OR here fragments unions and creates the vertical banding seen in your screenshot.
                        const bool occluded_left = (dl > d + options.occlusion_epsilon);
                        const bool occluded_right = (dr > d + options.occlusion_epsilon);
                        if (occluded_left && occluded_right) {
                            continue;
                        }
                    }

                    // Foreground cohesion
                    if (d > options.foreground_threshold && x > 0) {
                        uf.unite(x - 1, x);
                    }

                    uf.unite(left, right);
                }
            }
        }

        static void identifyRoots(int width, UnionFind& uf, std::vector<bool>& is_root)
        {
            for (int x = 0; x < width; ++x) {
                is_root[x] = (uf.find(x) == x);
            }
        }

        static void assignColors(int y, int width, int height,
            const std::vector<float>& adjusted_depth,
            UnionFind& uf, const std::vector<bool>& is_root,
            std::vector<bool>& rootHasColor,
            std::vector<std::array<uint8_t, 3>>& rootColor,
            const std::vector<uint8_t>& texture, int tw, int th, int tchan,
            const std::vector<uint8_t>& noiseRGB,
            std::vector<uint8_t>& out_rgb, const std::vector<uint8_t>& prev_row_colors,
            bool have_prev, std::mt19937& rng,
            std::uniform_int_distribution<int>& distr,
            float brightness, float contrast, const Options& options)
        {
            for (int x = 0; x < width; ++x) {
                if (!is_root[x]) continue;

                float d = adjusted_depth[y * width + x];
                std::array<uint8_t, 3> color{};
                bool propagated = false;

                if (d > options.foreground_threshold) {
                    propagated = tryPropagateFromNeighbors(x, y, width, uf, is_root, rootHasColor, rootColor, out_rgb, prev_row_colors, have_prev, color);
                }


                if (!propagated) {
                    if (!texture.empty()) {
                        color = getTextureColor(x, y, width, height, texture, tw, th, tchan, brightness, contrast, options.tile_texture);
                    }
                    else if (!noiseRGB.empty()) {
                        size_t idx = (static_cast<size_t>(y) * width + x) * 3;
                        color = { noiseRGB[idx + 0], noiseRGB[idx + 1], noiseRGB[idx + 2] };
                    }
                    else {
                        color = getRandomColor(distr, rng);
                    }
                }

                rootColor[x] = color;
                rootHasColor[x] = true;
            }
        }

        static bool tryPropagateFromNeighbors(int x, int y, int width, UnionFind& uf,
            const std::vector<bool>& is_root,
            const std::vector<bool>& rootHasColor,
            const std::vector<std::array<uint8_t, 3>>& rootColor,
            const std::vector<uint8_t>& out_rgb,
            const std::vector<uint8_t>& prev_row_colors,
            bool have_prev,
            std::array<uint8_t, 3>& color)
        {
            if (x > 0) {
                int left_root = uf.find(x - 1);
                if (left_root != x && is_root[left_root] && rootHasColor[left_root]) {
                    color = rootColor[left_root];
                    return true;
                }
            }

            if (have_prev && y > 0) {
                int above_idx = x * 3;
                color = { prev_row_colors[above_idx + 0], prev_row_colors[above_idx + 1], prev_row_colors[above_idx + 2] };
                return true;
            }

            if (x > 0 && y > 0 && have_prev) {
                int diag_idx = (x - 1) * 3;
                color = { prev_row_colors[diag_idx + 0], prev_row_colors[diag_idx + 1], prev_row_colors[diag_idx + 2] };
                return true;
            }

            return false;
        }

        static std::array<uint8_t, 3> getTextureColor(int x, int y, int width, int height,
            const std::vector<uint8_t>& texture,
            int tw, int th, int tchan,
            float brightness, float contrast,
            bool tileTexture)
        {
            if (tw <= 0 || th <= 0 || tchan < 3 || texture.empty()) {
                return { 128, 128, 128 };
            }

            float texX = static_cast<float>(x) * (static_cast<float>(tw) / width);
            float texY = static_cast<float>(y) * (static_cast<float>(th) / height);

            std::array<uint8_t, 3> color;
            if (tileTexture) {
                color = TextureSampler::sampleBilinearTiled(texture, tw, th, tchan, texX, texY);
            }
            else {
                texX = std::clamp(texX, 0.0f, static_cast<float>(tw - 1));
                texY = std::clamp(texY, 0.0f, static_cast<float>(th - 1));
                color = TextureSampler::sampleBilinear(texture, tw, th, tchan, texX, texY);
            }

            for (int c = 0; c < 3; c++) {
                float val = color[c] / 255.0f;
                val = ((val - 0.5f) * contrast) + 0.5f;
                val *= brightness;
                color[c] = static_cast<uint8_t>(std::clamp(val * 255.0f, 0.0f, 255.0f));
            }

            return color;
        }

        static std::array<uint8_t, 3> getRandomColor(std::uniform_int_distribution<int>& distr,
            std::mt19937& rng)
        {
            return { static_cast<uint8_t>(distr(rng)),
                     static_cast<uint8_t>(distr(rng)),
                     static_cast<uint8_t>(distr(rng)) };
        }

        static void applyColors(int y, int width, UnionFind& uf,
            const std::vector<std::array<uint8_t, 3>>& rootColor,
            std::vector<uint8_t>& out_rgb)
        {
            for (int x = 0; x < width; ++x) {
                int root = uf.find(x);
                int idx = (y * width + x) * 3;
                out_rgb[idx + 0] = rootColor[root][0];
                out_rgb[idx + 1] = rootColor[root][1];
                out_rgb[idx + 2] = rootColor[root][2];
            }
        }
};
