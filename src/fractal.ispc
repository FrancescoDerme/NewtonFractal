// This must be exactly equal to the struct defined in main.cpp
struct Complex { float r, i; };

// Helper functions for complex arithmetic
// These will be compiled to efficient SIMD instructions
inline Complex c_add(Complex a, Complex b) {
    Complex result = {a.r + b.r, a.i + b.i};
    return result;
}

inline Complex c_mul(Complex a, Complex b) {
    Complex result = {a.r * b.r - a.i * b.i, a.r * b.i + a.i * b.r};
    return result;
}

inline Complex c_div(Complex a, Complex b) {
    float denom = b.r * b.r + b.i * b.i;

    // Handle potential division by zero
    Complex result = {0.0, 0.0};
    if (denom == 0.0f) return result;

    result.r = (a.r * b.r + a.i * b.i) / denom;
    result.i = (a.i * b.r - a.r * b.i) / denom;
    return result;
}

// Since k is uniform, i.e. the same for all parallel instances,
// this loop is not divergent
inline Complex c_pow(Complex z, uniform int k) {
    Complex result = {1.0f, 0.0f};
    if (k == 0) return result;

    if (k < 0) {
        // Handle negative power
        Complex pos_pow = c_pow(z, -k);
        Complex one = {1.0f, 0.0f};
        return c_div(one, pos_pow);
    } else {
        // Handle positive power
        for (uniform int i = 0; i < k; ++i) {
            result = c_mul(result, z);
        }

        return result;
    }
}

inline float c_abs_sq(Complex z) {
    return z.r * z.r + z.i * z.i;
}

export void newton_fractal_ispc(
    uniform int width,              // Image width
    uniform int height,             // Image height
    uniform int n,                  // The 'n' in z^n - 1
    uniform const Complex roots[],  // Pre-calculated roots of unity
    uniform int max_iterations,     // Max iterations
    uniform float tolerance_sq,     // Squared tolerance
    uniform float x_min,            // Complex plane bounds
    uniform float x_max,            
    uniform float y_min,
    uniform float y_max,
    uniform int output_root[],      // Output array: which root was found (0 to n-1, or -1)
    uniform int output_iters[]      // Output array: how many iterations it took
) {
    // Calculate scaling factors
    uniform float dx = (x_max - x_min) / width;
    uniform float dy = (y_max - y_min) / height;

    // Pre-calculate uniform constants for the iteration formula
    uniform Complex n_minus_1 = {(float)(n - 1), 0.0f};
    uniform Complex one_over_n = {1.0f / (float)n, 0.0f};
    uniform int power = 1 - n;

    // This loops over all pixels is automatically
    // parallelized by ISCP
    foreach (y = 0 ... height, x = 0 ... width) {
        // Map this pixel to a starting complex number z0
        Complex z = {x_min + x * dx, y_min + y * dy};

        int iter = 0;
        int found_root = -1;

        // Newton method
        for (; iter < max_iterations; ++iter) {
            // 1. Check for convergence
            for (uniform int k = 0; k < n; ++k) {
                Complex diff = {z.r - roots[k].r, z.i - roots[k].i};
                if (c_abs_sq(diff) < tolerance_sq) {
                    found_root = k;
                    break;
                }
            }

            if (found_root != -1) {
                break;
            }
            
            // 2. Treat points close to the origin as diverged
            // (f'(z) is zero at z=0, causing division by zero)
            if (c_abs_sq(z) < 1e-12) {
                break;
            }

            // 3. Iterate
            // z = ( (n-1)*z + z^(1-n) ) / n
            Complex z_pow = c_pow(z, power);
            Complex term1 = c_mul(n_minus_1, z);
            Complex numer = c_add(term1, z_pow);
            z = c_mul(numer, one_over_n);
        }

        // Store the results for this pixel
        int index = y * width + x;
        output_root[index] = found_root;
        output_iters[index] = iter;
    }
}
