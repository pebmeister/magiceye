// written by Paul Baxter
#pragma once
#include <vector>
#include <array>
#include <string>
#include <algorithm>
#include <cmath>

#include "stb_image.h"

class TextureSampler {
public:
    static std::array<uint8_t, 3> sampleBilinear(const std::vector<uint8_t>& texture,
        int tw, int th, int tchan,
        float texX, float texY)
    {
        // REMOVE modulo wrapping - rely on the clamping from the caller
        // Just ensure coordinates are within bounds for safety
        texX = std::clamp(texX, 0.0f, static_cast<float>(tw - 1));
        texY = std::clamp(texY, 0.0f, static_cast<float>(th - 1));

        int x0 = static_cast<int>(std::floor(texX));
        int y0 = static_cast<int>(std::floor(texY));
        float fx = texX - x0;
        float fy = texY - y0;

        // Use clamping instead of modulo to prevent edge artifacts
        int x1 = std::min(x0 + 1, tw - 1);
        int y1 = std::min(y0 + 1, th - 1);

        // Ensure original coordinates are also clamped
        x0 = std::clamp(x0, 0, tw - 1);
        y0 = std::clamp(y0, 0, th - 1);

        auto getTexel = [&](int x, int y, int c) -> float
            {
                // Add bounds checking for extra safety
                x = std::clamp(x, 0, tw - 1);
                y = std::clamp(y, 0, th - 1);
                return static_cast<float>(texture[(y * tw + x) * tchan + c]);
            };

        std::array<uint8_t, 3> color{ 0, 0, 0 };
        for (int c = 0; c < 3; c++) {
            float c00 = getTexel(x0, y0, c);
            float c10 = getTexel(x1, y0, c);
            float c01 = getTexel(x0, y1, c);
            float c11 = getTexel(x1, y1, c);

            // Standard bilinear interpolation
            float top = (1.0f - fx) * c00 + fx * c10;
            float bottom = (1.0f - fx) * c01 + fx * c11;
            float val = (1.0f - fy) * top + fy * bottom;

            color[c] = static_cast<uint8_t>(std::clamp(val, 0.0f, 255.0f));
        }
        return color;
    }

    // Alternative: If you want tiling behavior but without edge artifacts, use this version:
    static std::array<uint8_t, 3> sampleBilinearTiled(const std::vector<uint8_t>& texture,
        int tw, int th, int tchan,
        float texX, float texY)
    {
        // For tiling textures, use repeat mode but handle edges carefully
        texX = std::fmod(texX, static_cast<float>(tw));
        if (texX < 0) texX += tw;
        texY = std::fmod(texY, static_cast<float>(th));
        if (texY < 0) texY += th;

        int x0 = static_cast<int>(texX);
        int y0 = static_cast<int>(texY);
        float fx = texX - x0;
        float fy = texY - y0;

        int x1 = (x0 + 1) % tw;
        int y1 = (y0 + 1) % th;

        auto getTexel = [&](int x, int y, int c) -> float
            {
                return static_cast<float>(texture[(y * tw + x) * tchan + c]);
            };

        std::array<uint8_t, 3> color{ 0, 0, 0 };
        for (int c = 0; c < 3; c++) {
            float c00 = getTexel(x0, y0, c);
            float c10 = getTexel(x1, y0, c);
            float c01 = getTexel(x0, y1, c);
            float c11 = getTexel(x1, y1, c);

            float top = (1.0f - fx) * c00 + fx * c10;
            float bottom = (1.0f - fx) * c01 + fx * c11;
            float val = (1.0f - fy) * top + fy * bottom;

            color[c] = static_cast<uint8_t>(std::clamp(val, 0.0f, 255.0f));
        }
        return color;
    }

    /// <summary>
    /// Load a file into a 3 channel vector
    /// </summary>
    static bool loadRGB(const std::string& path, std::vector<uint8_t>& out,
        int& w, int& h, int& channels)
    {
        int orig_ch = 0;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &orig_ch, 3);
        if (!data) return false;

        channels = 3;
        out.assign(data, data + (w * h * channels));
        stbi_image_free(data);
        return true;
    }
};