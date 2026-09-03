#pragma once

#include <math.h>
#include <stdint.h>

#include "pixeltypes.h"

CHSV rgb2hsv_approximate( const CRGB& rgb);

/**
 * @brief Converts Mired value to Correlated Color Temperature (CCT) in Kelvin.
 * Calculation: Kelvin = 1,000,000 / Mired.
 * @param mired Mired value (Micro Reciprocal Degree).
 * @return Temperature in Kelvin.
 */
float mired_to_cct(uint16_t mired);

/**
 * @brief Converts 16-bit CIE xy coordinates to 8-bit sRGB (CRGB).
 * Performs iterative gamut mapping to find the highest possible luminance (Y)
 * that remains within the sRGB color space.
 * @param x_in Scaled CIE x coordinate (0-65535).
 * @param y_in Scaled CIE y coordinate (0-65535).
 * @return The resulting CRGB color.
 */
CRGB xy_to_rgb(uint16_t x_in, uint16_t y_in);

/**
 * @brief Converts Mireds directly to CRGB using a logarithmic algorithm.
 * Best for simulating natural light temperatures.
 * @param mired Mired value.
 * @return The resulting CRGB color.
 */
CRGB cct_to_rgb(uint16_t mired);