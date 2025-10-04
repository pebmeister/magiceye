
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>

class DepthPostProcessor {
public:
    // Simple hole fill: replace non-finite with min of 4-neighbors; multi-pass
    static void fillHoles(std::vector<float>& depth, int width, int height, int passes = 2)
    {
        auto isBad = [](float v) { return !std::isfinite(v); };
        for (int p = 0; p < passes; ++p) {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    int idx = y * width + x;
                    if (!isBad(depth[idx])) continue;
                    float best = std::numeric_limits<float>::infinity();
                    if (x > 0 && std::isfinite(depth[idx - 1])) best = std::min(best, depth[idx - 1]);
                    if (x + 1 < width && std::isfinite(depth[idx + 1])) best = std::min(best, depth[idx + 1]);
                    if (y > 0 && std::isfinite(depth[idx - width])) best = std::min(best, depth[idx - width]);
                    if (y + 1 < height && std::isfinite(depth[idx + width])) best = std::min(best, depth[idx + width]);
                    if (std::isfinite(best)) depth[idx] = best;
                }
            }
        }
    }

    // Edge-preserving bilateral filter for softening jagged depth edges
    static void bilateralSmooth(std::vector<float>& depth, int width, int height, float sigmaSpatial = 1.5f, float sigmaRange = 0.15f, int iterations = 1)
    {
        if (depth.empty()) return;
        std::vector<float> out(depth.size(), 0.0f);

        int radius = std::max(1, int(std::ceil(2.0f * sigmaSpatial)));
        auto gauss = [&](float x, float s) { return std::exp(-(x * x) / (2.0f * s * s)); };

        for (int it = 0; it < iterations; ++it) {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    int idx = y * width + x;
                    float center = depth[idx];
                    if (!std::isfinite(center)) { out[idx] = depth[idx]; continue; }

                    float wsum = 0.0f;
                    float vsum = 0.0f;
                    for (int dy = -radius; dy <= radius; ++dy) {
                        int yy = y + dy; if (yy < 0 || yy >= height) continue;
                        for (int dx = -radius; dx <= radius; ++dx) {
                            int xx = x + dx; if (xx < 0 || xx >= width) continue;
                            int j = yy * width + xx;
                            float v = depth[j];
                            if (!std::isfinite(v)) continue;

                            float w = gauss(std::sqrt(float(dx * dx + dy * dy)), sigmaSpatial) * gauss(v - center, sigmaRange);
                            wsum += w;
                            vsum += w * v;
                        }
                    }
                    out[idx] = (wsum > 0.0f) ? (vsum / wsum) : center;
                }
            }
            depth.swap(out);
        }
    }
};
