// written by Paul Baxter
#pragma once
#include <array>
#include <vector>
#include <random>
#include <stack>
#include <algorithm>
#include <unordered_map>

#include "TextureSampler.h"
#include "Options.h"
#include <queue>

class SIRDSGenerator {
public:
    enum class Method {
        UnionFind,
        Recursive // Placeholder for future implementation
    };

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
            return parent[x] == x ? x : (parent[x] = find(parent[x]));
        }

        void unite(int a, int b)
        {
            a = find(a);
            b = find(b);
            if (a != b) parent[b] = a;
        }
    };

public:
    static void generate(const std::vector<float>& depth, int width, int height,
        int eye_separation, const std::vector<uint8_t>& texture,
        int tw, int th, int tchan, std::vector<uint8_t>& out_rgb,
        float texture_brightness, float texture_contrast,
        float bg_separation, Method method = Method::UnionFind)
    {
        // For now, always use the proven UnionFind method
        // The Recursive method can be implemented later when time permits
        generateUnionFind(depth, width, height, eye_separation, texture,
            tw, th, tchan, out_rgb, texture_brightness,
            texture_contrast, bg_separation);
    }

    // Separate method if you want to call UnionFind directly
    static void generateUnionFind(const std::vector<float>& depth, int width, int height,
        int eye_separation, const std::vector<uint8_t>& texture,
        int tw, int th, int tchan, std::vector<uint8_t>& out_rgb,
        float texture_brightness, float texture_contrast,
        float bg_separation)
    {
        std::vector<float> adjusted_depth = adjustDepthRange(depth, bg_separation);
        out_rgb.assign(static_cast<size_t>(width) * height * 3, 0);

        std::mt19937 rng(123456);
        std::uniform_int_distribution<int> distr(0, 255);

        std::vector<int> separation_map = calculateSeparationMap(adjusted_depth, width, height, eye_separation);

        UnionFind uf(width);
        for (int y = 0; y < height; ++y) {
            processScanline(y, width, height, adjusted_depth, separation_map, uf,
                texture, tw, th, tchan, out_rgb, rng, distr,
                texture_brightness, texture_contrast);
        }

        applyEdgeSmoothing(adjusted_depth, out_rgb, width, height);
    }

private:
    // ... (keep all your existing helper methods unchanged)
    static std::vector<float> adjustDepthRange(const std::vector<float>& depth, float bg_separation)
    {
        std::vector<float> adjusted_depth(depth.size());
        for (size_t i = 0; i < depth.size(); i++) {
            adjusted_depth[i] = depth[i] * (1.0f - bg_separation);
        }
        return adjusted_depth;
    }

    static std::vector<int> calculateSeparationMap(const std::vector<float>& adjusted_depth,
        int width, int height, int eye_separation)
    {
        const float depth_gamma = 0.6f;
        const int min_separation = 3;
        const int max_separation = eye_separation;
        const float focus_depth = 0.5f;

        std::vector<int> separation_map(width * height);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float d = adjusted_depth[y * width + x];
                float t = pow(std::abs(d - focus_depth) * 2.0f, 1.5f);
                float sep_scale = 1.0f + t * 0.5f;

                float sep_float = min_separation + (max_separation - min_separation) *
                    pow(1.0f - d, depth_gamma) * sep_scale;

                separation_map[y * width + x] = std::max(min_separation,
                    std::min(static_cast<int>(std::round(sep_float)), max_separation));
            }
        }
        return separation_map;
    }

    static void processScanline(int y, int width, int height,
        const std::vector<float>& adjusted_depth,
        const std::vector<int>& separation_map,
        UnionFind& uf, const std::vector<uint8_t>& texture,
        int tw, int th, int tchan, std::vector<uint8_t>& out_rgb,
        std::mt19937& rng, std::uniform_int_distribution<int>& distr,
        float brightness, float contrast)
    {
        uf.reset(width);
        buildUnions(y, width, adjusted_depth, separation_map, uf);

        std::vector<std::array<uint8_t, 3>> rootColor(width);
        std::vector<bool> is_root(width, false);

        identifyRoots(width, uf, is_root);
        assignColors(y, width, height, adjusted_depth, uf, is_root, rootColor, texture,
            tw, th, tchan, out_rgb, rng, distr, brightness, contrast);
        applyColors(y, width, uf, rootColor, out_rgb);
    }

    static void buildUnions(int y, int width, const std::vector<float>& adjusted_depth,
        const std::vector<int>& separation_map, UnionFind& uf)
    {
        const float foreground_threshold = 0.75f;

        for (int x = 0; x < width; ++x) {
            int sep = separation_map[y * width + x];
            int left = x - sep / 2;
            int right = left + sep;

            if (left >= 0 && right < width) {
                float d = adjusted_depth[y * width + x];
                if (d > foreground_threshold && x > 0) {
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
        std::vector<std::array<uint8_t, 3>>& rootColor,
        const std::vector<uint8_t>& texture, int tw, int th, int tchan,
        std::vector<uint8_t>& out_rgb, std::mt19937& rng,
        std::uniform_int_distribution<int>& distr,
        float brightness, float contrast)
    {
        const float foreground_threshold = 0.75f;

        for (int x = 0; x < width; ++x) {
            if (!is_root[x]) continue;

            float d = adjusted_depth[y * width + x];
            std::array<uint8_t, 3> color;
            bool propagated = false;

            if (d > foreground_threshold) {
                propagated = tryPropagateFromNeighbors(x, y, width, uf, is_root, rootColor, out_rgb, color);
            }

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

    static bool tryPropagateFromNeighbors(int x, int y, int width, UnionFind& uf,
        const std::vector<bool>& is_root,
        const std::vector<std::array<uint8_t, 3>>& rootColor,
        const std::vector<uint8_t>& out_rgb,
        std::array<uint8_t, 3>& color)
    {
        if (x > 0) {
            int left_root = uf.find(x - 1);
            if (left_root != x && is_root[left_root]) {
                color = rootColor[left_root];
                return true;
            }
        }

        if (y > 0) {
            int above_root = uf.find(x);
            if (above_root != x && is_root[above_root]) {
                int above_idx = ((y - 1) * width + x) * 3;
                color = { out_rgb[above_idx], out_rgb[above_idx + 1], out_rgb[above_idx + 2] };
                return true;
            }
        }

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

    static std::array<uint8_t, 3> getTextureColor(int x, int y, int width, int height,
        const std::vector<uint8_t>& texture,
        int tw, int th, int tchan,
        float brightness, float contrast)
    {
        float texX = static_cast<float>(x) * (static_cast<float>(tw) / width);
        float texY = static_cast<float>(y) * (static_cast<float>(th) / height);

        auto color = TextureSampler::sampleBilinear(texture, tw, th, tchan, texX, texY);

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

    static void applyEdgeSmoothing(const std::vector<float>& adjusted_depth,
        std::vector<uint8_t>& out_rgb, int width, int height)
    {
        const float foreground_threshold = 0.75f;

        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                float d = adjusted_depth[y * width + x];
                if (d > foreground_threshold) {
                    smoothPixel(x, y, width, out_rgb);
                }
            }
        }
    }

    static void smoothPixel(int x, int y, int width, std::vector<uint8_t>& out_rgb)
    {
        int idx = (y * width + x) * 3;
        for (int c = 0; c < 3; ++c) {
            int sum = out_rgb[idx + c] * 6;
            sum += out_rgb[((y - 1) * width + x) * 3 + c];
            sum += out_rgb[((y + 1) * width + x) * 3 + c];
            sum += out_rgb[(y * width + (x - 1)) * 3 + c];
            sum += out_rgb[(y * width + (x + 1)) * 3 + c];
            sum += out_rgb[((y - 1) * width + (x - 1)) * 3 + c];
            sum += out_rgb[((y + 1) * width + (x + 1)) * 3 + c];
            out_rgb[idx + c] = sum / 10;
        }
    }
};
