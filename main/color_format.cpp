#include "color_format.h"

// sRGB Gamma curve constants
static constexpr float GAMMA_VAL = 0.42f;
static constexpr float TRANSITION = 0.0031308f;
static constexpr float SLOPE = 12.92f;
static constexpr float OFFSET = 0.055f;

float mired_to_cct(uint16_t mired) {
    return (mired == 0) ? 0.0f : 1000000.0f / (float)mired;
}

/** * @brief Applies the sRGB gamma companding function to a linear color component.
 * If value <= 0.0031308, use linear slope; otherwise, use the power function.
 */
static inline float gamma_transform(float value) {
    return (value <= TRANSITION) ? (SLOPE * value) : ((1.0f + OFFSET) * powf(value, GAMMA_VAL) - OFFSET);
}

/** * @brief Converts CIE XYZ coordinates to Linear RGB using the D65 matrix,
 * then applies gamma correction to get sRGB.
 */
static void xyz_to_srgb(float X, float Y, float Z, float* r, float* g, float* b) {
    float r_lin = X * 3.2404542f + Y * -1.5371385f + Z * -0.4985314f;
    float g_lin = X * -0.9692660f + Y * 1.8760108f + Z * 0.0415560f;
    float b_lin = X * 0.0556434f + Y * -0.2040259f + Z * 1.0572252f;

    *r = (r_lin > 0.0f) ? gamma_transform(r_lin) : 0.0f;
    *g = (g_lin > 0.0f) ? gamma_transform(g_lin) : 0.0f;
    *b = (b_lin > 0.0f) ? gamma_transform(b_lin) : 0.0f;
}

/** * @brief Converts CIE xyY (chromaticity + luminance) to sRGB.
 * Derived from X = (Y/y)*x and Z = (Y/y)*(1-x-y).
 */
static void xyy_to_srgb(float x, float y, float Y_lum, float* r, float* g, float* b) {
    float cy = (y < 1e-9f) ? 1e-9f : y;
    float factor = Y_lum / cy;
    xyz_to_srgb(factor * x, Y_lum, factor * (1.0f - x - cy), r, g, b);
}

void xy_to_rgb(uint16_t x_in, uint16_t y_in, CRGB* rgb_out) {
    const float x = (float)x_in / 65535.0f;
    const float y = (float)y_in / 65535.0f;
    float Y_max = 1.0f;
    float r, g, b;

    // Iteratively scale down luminance (Y) until the color fits in the 0.0-1.0 RGB cube
    for (int i = 0; i < 10; i++) {
        xyy_to_srgb(x, y, Y_max, &r, &g, &b);
        float max_comp = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
        if (max_comp <= 1.0f) break;
        Y_max /= max_comp;
    }

    xyy_to_srgb(x, y, Y_max, &r, &g, &b);
    rgb_out->r = (uint8_t)(app_constrain(r, 0.0f, 1.0f) * 255.0f + 0.5f);
    rgb_out->g = (uint8_t)(app_constrain(g, 0.0f, 1.0f) * 255.0f + 0.5f);
    rgb_out->b = (uint8_t)(app_constrain(b, 0.0f, 1.0f) * 255.0f + 0.5f);
}

// mired to RGB (https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html)
void cct_to_rgb(uint16_t mired, CRGB* rgb) {
    float temp = mired_to_cct(mired) / 100.0f;
    float r, g, b;

    // Tanner Helland algorithm for mapping Kelvin to RGB
    if (temp <= 66.0f) {
        r = 255.0f;
        g = 99.4708025861f * logf(temp) - 161.1195681661f;
        b = (temp <= 19.0f) ? 0.0f : 138.5177312231f * logf(temp - 10.0f) - 305.0447927307f;
    } else {
        r = 329.698727446f * powf(temp - 60.0f, -0.1332047592f);
        g = 288.1221695283f * powf(temp - 60.0f, -0.0755148492f);
        b = 255.0f;
    }

    rgb->r = (uint8_t)app_constrain(r, 0.0f, 255.0f);
    rgb->g = (uint8_t)app_constrain(g, 0.0f, 255.0f);
    rgb->b = (uint8_t)app_constrain(b, 0.0f, 255.0f);
}
