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
        texX = std::fmod(texX, static_cast<float>(tw));
        if (texX < 0) texX += tw;
        texY = std::fmod(texY, static_cast<float>(th));
        if (texY < 0) texY += th;

        int x0 = static_cast<int>(std::floor(texX));
        int y0 = static_cast<int>(std::floor(texY));
        float fx = texX - x0;
        float fy = texY - y0;

        int x1 = (x0 + 1) % tw;
        int y1 = (y0 + 1) % th;
        x0 = (x0 % tw + tw) % tw;
        y0 = (y0 % th + th) % th;
        x1 = (x1 % tw + tw) % tw;
        y1 = (y1 % th + th) % th;

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

            float val = (1.0f - fx) * (1.0f - fy) * c00 +
                fx * (1.0f - fy) * c10 +
                (1.0f - fx) * fy * c01 +
                fx * fy * c11;

            color[c] = static_cast<uint8_t>(std::clamp(val + 0.5f, 0.0f, 255.0f));
        }
        return color;
    }

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
