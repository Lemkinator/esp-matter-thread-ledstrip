#include "color_format.h"

float mired_to_cct(uint16_t mired) {
    return (mired == 0) ? 0.0f : 1000000.0f / (float)mired;
}

void cct_to_xy(uint32_t mired, uint16_t *x_out, uint16_t *y_out) {
    // 1. Convert Mired to CCT (Kelvin)
    float T = mired_to_cct(mired);
    if (T == 0.0f) {
        *x_out = 0; *y_out = 0; return;
    }
    
    // Calculate the normalized reciprocal temperature term (1000/T)^n
    const float u = 1000.0f / T;
    const float u2 = u * u;
    const float u3 = u2 * u;
    float x;

    // 2. Calculate x coordinate (Based on CCT piecewise approximation)
    if (T <= 4000.0f) { // 1667 K <= T <= 4000 K
        x = -0.2661239f * u3 - 0.2343580f * u2 + 0.8776956f * u + 0.179910f;
    } else { // 4000 K < T <= 25000 K
        x = -3.0258469f * u3 + 2.1070379f * u2 + 0.2226347f * u + 0.240390f;
    }

    // Prepare powers of x for y calculation
    const float x2 = x * x;
    const float x3 = x2 * x;
    float y;

    // 3. Calculate y coordinate (Based on x piecewise approximation)
    if (T <= 2222.0f) { // 1667 K <= T <= 2222 K
        y = -1.1063814f * x3 - 1.34811020f * x2 + 2.18555832f * x - 0.20219683f;
    } else if (T <= 4000.0f) { // 2222 K < T <= 4000 K
        y = -0.9549476f * x3 - 1.37418593f * x2 + 2.09137015f * x - 0.16748867f;
    } else { // 4000 K < T <= 25000 K
        y = +3.0817580f * x3 - 5.87338670f * x2 + 3.75112997f * x - 0.37001483f;
    }

    // 4. Clamp xy values to the valid range [0.0, 1.0]
    x = fmaxf(0.0f, fminf(1.0f, x));
    y = fmaxf(0.0f, fminf(1.0f, y));

    // 5. Convert to 16-bit integer (0-65535) and set output
    // The +0.5f is for proper rounding.
    *x_out = (uint16_t)(x * 65535.0f + 0.5f);
    *y_out = (uint16_t)(y * 65535.0f + 0.5f);
}

float gamma_transform(float value) {
    return (value <= TRANSITION) 
        ? (SLOPE * value) 
        : ((1.0f + OFFSET) * powf(value, GAMMA_VAL) - OFFSET);
}

void xyz_to_srgb(float X, float Y, float Z, float *r, float *g, float *b)
{
    // 1. Matrix multiplication (XYZ -> Linear RGB)
    float r_lin = X * XYZ_TO_RGB[0][0] + Y * XYZ_TO_RGB[0][1] + Z * XYZ_TO_RGB[0][2];
    float g_lin = X * XYZ_TO_RGB[1][0] + Y * XYZ_TO_RGB[1][1] + Z * XYZ_TO_RGB[1][2];
    float b_lin = X * XYZ_TO_RGB[2][0] + Y * XYZ_TO_RGB[2][1] + Z * XYZ_TO_RGB[2][2];

    // 2. Gamma Correction
    *r = (r_lin > 0.0f) ? gamma_transform(r_lin) : 0.0f; // Simple clamping for negative linear values
    *g = (g_lin > 0.0f) ? gamma_transform(g_lin) : 0.0f;
    *b = (b_lin > 0.0f) ? gamma_transform(b_lin) : 0.0f;
}

void xyy_to_srgb(float x, float y, float Y_lum, float *r, float *g, float *b) {
    // Prevent division by zero: if y is near 0, treat it as a tiny positive value.
    const float cy = fmaxf(1e-9f, y);
    
    // Calculate the third chromaticity coordinate: z = 1 - x - y
    const float z = 1.0f - x - cy; 

    // Convert to XYZ: X = (Y/y)*x, Z = (Y/y)*z
    const float factor = Y_lum / cy;
    const float X = factor * x;
    const float Z = factor * z;

    // Use the existing function to complete the conversion
    xyz_to_srgb(X, Y_lum, Z, r, g, b);
}

void xy_to_rgb(uint16_t x_in, uint16_t y_in, RGB_color_t *rgb_out) {
    // 1. Normalize xy coordinates from 16-bit to float [0.0, 1.0]
    const float x = (float)x_in / 65535.0f;
    const float y = (float)y_in / 65535.0f;
    
    float Y_max = 1.0f; // Initial guess for maximum luminance
    float r, g, b;

    // 2. Iterative Gamut Mapping: Scale Y_max down until all RGB components are <= 1.0
    // This loop effectively finds the maximum Y (luminance) that fits in the sRGB gamut.
    for (int i = 0; i < 10; i++) {
        // Convert to sRGB using the current Y_max
        xyy_to_srgb(x, y, Y_max, &r, &g, &b);

        // Find the component that exceeds the gamut (max(r, g, b))
        float max_comp = fmaxf(r, fmaxf(g, b));

        if (max_comp <= 1.0f) {
            // Optimization: We are within the gamut, stop iterating (or continue for precision)
            // Break early as 10 iterations is usually more than enough.
            break;
        }
        
        // Scale down Y_max by the excess amount: Y_new = Y_old / max_comp
        Y_max /= max_comp;
    }

    // 3. Final conversion and clamping
    // Recalculate with the determined Y_max (Y_max is now the final sRGB luminance)
    xyy_to_srgb(x, y, Y_max, &r, &g, &b);

    // Clamp final float RGB values to [0.0, 1.0]
    r = fmaxf(0.0f, fminf(1.0f, r));
    g = fmaxf(0.0f, fminf(1.0f, g));
    b = fmaxf(0.0f, fminf(1.0f, b));
    
    // 4. Scale to 8-bit integer (0-255) with rounding
    // The addition of 0.5f before casting provides simple rounding.
    rgb_out->red   = (uint8_t)(r * 255.0f + 0.5f);
    rgb_out->green = (uint8_t)(g * 255.0f + 0.5f);
    rgb_out->blue  = (uint8_t)(b * 255.0f + 0.5f);
}