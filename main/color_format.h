#pragma once

#include <math.h>
#include <stdint.h>

#include "pixeltypes.h"

CHSV rgb2hsv_approximate( const CRGB& rgb);

/** * @brief Constrains a floating point value between a lower and upper bound.
 * @param val The input value to check.
 * @param low The minimum allowed value.
 * @param high The maximum allowed value.
 * @return The constrained value.
 */
static inline float app_constrain(float val, float low, float high) {
    return (val < low) ? low : (val > high ? high : val);
}

/**
 * @brief Converts Mired value to Correlated Color Temperature (CCT) in Kelvin.
 * Calculation: Kelvin = 1,000,000 / Mired.
 * @param mired Mired value (Micro Reciprocal Degree).
 * @return Temperature in Kelvin.
 */
float mired_to_cct(uint16_t mired);

/**
 * @brief Converts Mireds to CIE 1931 xy coordinates.
 * Uses piecewise polynomial approximations of the Planckian Locus.
 * @param mired The color temperature in Mireds.
 * @param x_out Pointer to store the CIE x coordinate scaled to 0-65535.
 * @param y_out Pointer to store the CIE y coordinate scaled to 0-65535.
 */
void cct_to_xy(uint32_t mired, uint16_t* x_out, uint16_t* y_out);

/**
 * @brief Converts 16-bit CIE xy coordinates to 8-bit sRGB (CRGB).
 * Performs iterative gamut mapping to find the highest possible luminance (Y)
 * that remains within the sRGB color space.
 * @param x_in Scaled CIE x coordinate (0-65535).
 * @param y_in Scaled CIE y coordinate (0-65535).
 * @param rgb_out Pointer to the CRGB structure to populate.
 */
void xy_to_rgb(uint16_t x_in, uint16_t y_in, CRGB* rgb_out);

/**
 * @brief Converts Mireds directly to CRGB using a logarithmic algorithm.
 * Best for simulating natural light temperatures.
 * @param mired Mired value.
 * @param rgb_out Pointer to the CRGB structure to populate.
 */
void cct_to_rgb(uint16_t mired, CRGB* rgb_out);

/**
 * @brief Alternative Mired to CRGB conversion using a piecewise lookup table.
 * @param mired Mired value.
 * @param rgb_out Pointer to the CRGB structure to populate.
 */
void cct_to_rgb_alt(uint16_t mired, CRGB* rgb_out);

/**
 * @brief Alternative CIE xy to CRGB conversion using a single-pass matrix.
 * @param x_in Scaled CIE x coordinate (0-65535).
 * @param y_in Scaled CIE y coordinate (0-65535).
 * @param rgb_out Pointer to the CRGB structure to populate.
 */
void xy_to_rgb_alt(uint16_t x_in, uint16_t y_in, CRGB* rgb_out);