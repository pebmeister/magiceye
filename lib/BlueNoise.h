// written bu Paul Baxter

#pragma once
#include <vector>
#include <cstdint>
#include <random>
#include <algorithm>

class BlueNoise {
public:
    // Generate RGB "blue-ish" noise using 8x8 Bayer matrix modulation + hash noise.
    // Width/height sized to output image for row-coherent, high-frequency pattern.
    static std::vector<uint8_t> generateRGB(int width, int height, uint32_t seed = 1337u)
    {
        std::vector<uint8_t> tex(static_cast<size_t>(width) * height * 3);

        static const int bayer8[64] = {
             0, 32,  8, 40,  2, 34, 10, 42,
            48, 16, 56, 24, 50, 18, 58, 26,
            12, 44,  4, 36, 14, 46,  6, 38,
            60, 28, 52, 20, 62, 30, 54, 22,
             3, 35, 11, 43,  1, 33,  9, 41,
            51, 19, 59, 27, 49, 17, 57, 25,
            15, 47,  7, 39, 13, 45,  5, 37,
            63, 31, 55, 23, 61, 29, 53, 21
        };

        auto h32 = [](uint32_t x) {
            x ^= x >> 17; x *= 0xED5AD4BBu;
            x ^= x >> 11; x *= 0xAC4C1B51u;
            x ^= x >> 15; x *= 0x31848BABu;
            x ^= x >> 14;
            return x;
        };

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int b = bayer8[(y & 7) * 8 + (x & 7)];
                uint32_t base = h32((uint32_t(x) * 73856093u) ^ (uint32_t(y) * 19349663u) ^ seed);
                uint8_t r = static_cast<uint8_t>((base & 0xffu));
                uint8_t g = static_cast<uint8_t>(((base >> 8) & 0xffu));
                uint8_t bch = static_cast<uint8_t>(((base >> 16) & 0xffu));

                // Modulate by Bayer to push energy out of low frequencies
                float factor = (b + 1) / 64.0f; // 1..64 normalized
                r = static_cast<uint8_t>(std::clamp(float(r) * factor, 0.0f, 255.0f));
                g = static_cast<uint8_t>(std::clamp(float(g) * factor, 0.0f, 255.0f));
                bch = static_cast<uint8_t>(std::clamp(float(bch) * factor, 0.0f, 255.0f));

                size_t idx = (static_cast<size_t>(y) * width + x) * 3;
                tex[idx + 0] = r;
                tex[idx + 1] = g;
                tex[idx + 2] = bch;
            }
        }
        return tex;
    }
};
