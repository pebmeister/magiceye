#pragma once
#include <vector>

class EdgeSmoother {
public:
    /// Applies a simple low-pass filter to foreground pixels to reduce visual noise.
    static void applyEdgeSmoothing(const std::vector<float>& adjusted_depth,
        std::vector<uint8_t>& out_rgb, float smoothThreshold, float smoothWeight, int width, int height)
    {

        // Iterate through interior pixels (ignore the border for simplicity).
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                float d = adjusted_depth[y * width + x];
                // Only smooth pixels that are in the foreground.
                if (d > smoothThreshold) {
                    smoothPixel(x, y, width, out_rgb, smoothWeight);
                }
            }
        }
    }

    /// Averages the color of a pixel with its 8 surrounding neighbors.
    static void smoothPixel(int x, int y, int width, std::vector<uint8_t>& out_rgb, float smoothWeight)
    {
        int idx = (y * width + x) * 3; // Index of the central pixel.
        for (int c = 0; c < 3; ++c) { // Process each color channel (R, G, B).
            // Sum of center pixel (weight 6) and 6 neighbors (weight 1 each) = total weight 12.
            float sum = out_rgb[idx + c] * 6.0;
            // Add the four cardinal neighbors
            sum += out_rgb[((y - 1) * width + x) * 3 + c]; // Up
            sum += out_rgb[((y + 1) * width + x) * 3 + c]; // Down
            sum += out_rgb[(y * width + (x - 1)) * 3 + c]; // Left
            sum += out_rgb[(y * width + (x + 1)) * 3 + c]; // Right

            // Add two diagonal neighbors (optional, improves smoothness)
            sum += out_rgb[((y - 1) * width + (x - 1)) * 3 + c]; // Up-Left
            sum += out_rgb[((y + 1) * width + (x + 1)) * 3 + c]; // Down-Right

            float weight = std::max(smoothWeight, 1.0f);
            out_rgb[idx + c] = static_cast<uint8_t>(sum / weight);
        }
    }
};
