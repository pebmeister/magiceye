// written by Paul Baxter
#pragma once
#include <vector>
#include <algorithm>

class EdgeSmoother {
public:
    /// Applies a 3x3 blur to foreground pixels and blends with original.
    static void applyEdgeSmoothing(const std::vector<float>& adjusted_depth,
        std::vector<uint8_t>& out_rgb, float smoothThreshold, float smoothWeight, int width, int height)
    {
        if (width < 3 || height < 3) return;

        // Strength: larger smoothWeight -> milder smoothing
        float alpha = 1.0f / std::max(1.0f, smoothWeight);

        // Read from a stable source buffer while writing to out_rgb
        std::vector<uint8_t> src = out_rgb;

        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                float d = adjusted_depth[y * width + x];
                if (d <= smoothThreshold) continue;

                int basePix = y * width + x;
                int neighbors[9] = {
                    basePix - width - 1, basePix - width, basePix - width + 1,
                    basePix - 1,         basePix,         basePix + 1,
                    basePix + width - 1, basePix + width, basePix + width + 1
                };

                int idx_center = basePix * 3;
                for (int c = 0; c < 3; ++c) {
                    float sum = 0.0f;
                    for (int k = 0; k < 9; ++k) {
                        int idxn = neighbors[k] * 3 + c;
                        sum += src[idxn];
                    }
                    float mean = sum / 9.0f;
                    float orig = src[idx_center + c];
                    float blended = orig * (1.0f - alpha) + mean * alpha;
                    blended = std::clamp(blended, 0.0f, 255.0f);
                    out_rgb[idx_center + c] = static_cast<uint8_t>(blended);
                }
            }
        }
    }
};