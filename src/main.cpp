#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "fractal_ispc.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// This must be exactly equal to the struct defined in fractal.ispc
struct Complex {
    float r, i;
};

struct RGB {
    unsigned char r, g, b;
};

// Helper to convert HSV (Hue, Saturation, Value) to RGB
// H [0, 1], S [0, 1], V [0, 1]
RGB hsv_to_rgb(float h, float s, float v) {
    // If saturation is null, i.e. there is no color, the result is a shade of
    // gray and hue is meaningless
    if (s == 0.0f)
        return {(unsigned char)(v * 255), (unsigned char)(v * 255),
                (unsigned char)(v * 255)};

    // h represents a circle, it is divided into 6 equal sectors
    // i (integer) is the sector index and f (fractional) is a "remainder"
    // telling how far into the sector a specific color lies
    int i = (int)(h * 6.0f);
    float f = (h * 6.0f) - i;

    // v = intensity of the maximum brightness color
    // p = intensity of minimum brightness color
    float p = v * (1.0f - s);

    // q, t = intensity of intermediate brightness color
    // q is fading down from v to p with f
    // t is faing up from p to v with f
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    i %= 6;

    // Scale to [0, 255]
    v *= 255;
    p *= 255;
    q *= 255;
    t *= 255;

    // Decide which value to assing to R, G, B based on the secotor
    // In each sector, a color component is at its max (v), one at its min (p)
    // and the third is fading down (q) or up (t)
    switch (i) {
        // Red to yellow, hue: 0 - 60, red is max, green rises, blue is min
        case 0:
            return {(unsigned char)v, (unsigned char)t, (unsigned char)p};
        // Yellow to green, hue 60 - 120, red falls, green is max, blue is min
        case 1:
            return {(unsigned char)q, (unsigned char)v, (unsigned char)p};
        // Green to cyan, hue 120 - 180, red is min, green is max, blue rises
        case 2:
            return {(unsigned char)p, (unsigned char)v, (unsigned char)t};
        // Cyan to blue, hue 180 - 240, red is min, green falls, blue is max
        case 3:
            return {(unsigned char)p, (unsigned char)q, (unsigned char)v};
        // Blue to magenta, hue 240 - 300, red rises, green is min, blue is max
        case 4:
            return {(unsigned char)t, (unsigned char)p, (unsigned char)v};
        // Magenta to red, hue 300 - 360, red is max, green is min, blue falls
        case 5:
        default:
            return {(unsigned char)v, (unsigned char)p, (unsigned char)q};
    }
}

int main(int argc, char* argv[]) {
    // 1. Configuration
    // Default values
    int n = 5;
    int width = 1655;
    int height = 1655;
    int max_iterations = 100;
    double tolerance = 1e-6;
    float gamma = 4.0f;
    std::string filename = "newton_fractal.png";

    // Command-line parsing
    if (argc > 1) n = std::atoi(argv[1]);
    if (argc > 2) width = std::atoi(argv[2]);
    if (argc > 3) height = std::atoi(argv[3]);
    if (argc > 4) max_iterations = std::atoi(argv[4]);
    if (argc > 5) tolerance = std::stod(argv[5]);
    if (argc > 6) gamma = std::stof(argv[6]);

    std::cout << "Generating Newton fractal for z^" << n << "-1 = 0\n";
    std::cout << "Image size: " << width << "x" << height << "\n";
    std::cout << "Max iterations: " << max_iterations << "\n";
    std::cout << "Newton method tolerance: " << tolerance << "\n";
    std::cout << "Gamma (higher value decays colors based on iterations more "
                 "abruptly): "
              << gamma << "\n";

    // Complex plane
    const float x_min = -5.0f, x_max = 5.0f, y_min = -5.0f, y_max = 5.0f;
    const float tolerance_sq = (float)(tolerance * tolerance);

    // 2. Compute roots
    std::vector<Complex> roots(n);
    for (int k = 0; k < n; ++k) {
        double angle = 2.0 * M_PI * k / n;
        roots[k] = {(float)std::cos(angle), (float)std::sin(angle)};
    }

    // 3. Allocate memory
    std::vector<int> output_root(width * height);
    std::vector<int> output_iters(width * height);

    // 4. Launch ISPC kernel
    std::cout << "Starting ISPC computation...\n";
    auto start = std::chrono::high_resolution_clock::now();

    ispc::newton_fractal_ispc(width, height, n,
                              reinterpret_cast<ispc::Complex*>(roots.data()),
                              max_iterations, tolerance_sq, x_min, x_max, y_min,
                              y_max, output_root.data(), output_iters.data());

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    std::cout << "ISPC computation finished in " << diff.count()
              << " seconds\n";

    // 5. Post-processing (serial)
    std::vector<RGB> image_data(width * height);
    for (int i = 0; i < width * height; ++i) {
        int root_index = output_root[i];
        int iters = output_iters[i];

        if (root_index == -1) {
            // Diverged (or hit zero) -> Black
            image_data[i] = {0, 0, 0};
        }
        else {
            // Converged -> color by root and iterations
            // Hue: based on which root
            float hue = (float)root_index / (float)n;

            // Saturation: constant
            float saturation = 0.9f;

            // Value: based on iteration count
            // Fewer iterations = brighter
            // Gamma controls how fast colors decay to black
            float value = 1.0f - (float)iters / (float)max_iterations;
            value = pow(value, gamma);

            image_data[i] = hsv_to_rgb(hue, saturation, value);
        }
    }

    // 6. Write image to file
    stbi_write_png(filename.c_str(), width, height, 3,  // 3 channels (RGB)
                   image_data.data(),
                   width * sizeof(RGB)  // Stride
    );

    std::cout << "Image saved to " << filename << "\n";
    return 0;
}
