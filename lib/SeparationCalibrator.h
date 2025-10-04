
#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>

class SeparationCalibrator {
public:
    // Estimate a focus plane from the current depth map (0..1 assumed)
    // Uses a simple histogram mode; falls back to median if sparse.
    static float estimateFocusDepth(const std::vector<float>& depth, int width, int height)
    {
        if (depth.empty()) return 0.5f;
        const int bins = 256;
        std::vector<int> hist(bins, 0);

        int count = 0;
        for (float d : depth) {
            if (!std::isfinite(d)) continue;
            float clamped = std::clamp(d, 0.0f, 1.0f);
            int idx = std::clamp(static_cast<int>(std::round(clamped * (bins - 1))), 0, bins - 1);
            hist[idx]++;
            count++;
        }
        if (count == 0) return 0.5f;

        // Mode
        int maxBin = std::max_element(hist.begin(), hist.end()) - hist.begin();
        float modeDepth = static_cast<float>(maxBin) / float(bins - 1);

        // Simple stability: avoid extremes
        modeDepth = std::clamp(modeDepth, 0.1f, 0.9f);
        return modeDepth;
    }
};
