#pragma once
#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/** Gamma transform constants **/
#define GAMMA_VAL 0.42f // Note: This is 1/2.4 in the simplified sRGB
#define TRANSITION 0.0031308f
#define SLOPE 12.92f
#define OFFSET 0.055f

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } RGB_color_t;

    /** D65 Matrix for converting from CIE 1931 XYZ to linear RGB (sRGB) **/
    static const float XYZ_TO_RGB[3][3] = {
        {3.2404542f, -1.5371385f, -0.4985314f},
        {-0.9692660f, 1.8760108f, 0.0415560f},
        {0.0556434f, -0.2040259f, 1.0572252f}};

    /**
     * @brief Converts Mired value to Correlated Color Temperature (CCT) in Kelvin.
     *
     * Kelvin = 1 000 000 / Mired
     *
     * @param mired Mired value
     * @return CCT in Kelvin
     **/
    float mired_to_cct(uint16_t mired);

    /**
     * @brief Converts Correlated Color Temperature (CCT) in Mireds to CIE 1931 xy coordinates.
     *
     * This uses a set of piecewise polynomial approximations of the Planckian Locus.
     * The output xy values are scaled to a 16-bit integer range (0-65535).
     *
     * @param mired The color temperature in Mireds (Micro Reciprocal Degree).
     * @param x_out Pointer to store the scaled CIE x coordinate (0-65535).
     * @param y_out Pointer to store the scaled CIE y coordinate (0-65535).
     */
    void cct_to_xy(uint32_t mired, uint16_t *x_out, uint16_t *y_out);

    /**
     * @brief Converts Linear Value -> Gamma Corrected Value (Linear RGB -> sRGB)
     *
     * @param value Linear RGB value (0.0 - 1.0)
     * @return Gamma corrected sRGB value (0.0 - 1.0)
     **/
    float gamma_transform(float value);

    /**
     * @brief Converts CIE XYZ coordinates to non-linear sRGB components (0.0 to 1.0).
     *
     * @param X, Y, Z The CIE XYZ coordinates.
     * @param r, g, b Pointers to store the sRGB components.
     **/
    void xyz_to_srgb(float X, float Y, float Z, float *r, float *g, float *b);

    /**
     * @brief Converts CIE xyY coordinates to sRGB components (0.0 to 1.0).
     *
     * @param x CIE 1931 x coordinate (0.0 - 1.0)
     * @param y CIE 1931 y coordinate (0.0 - 1.0)
     * @param Y_lum Luminance value (0.0 - 1.0)
     * @param r Pointer to store Red component (0.0 - 1.0)
     * @param g Pointer to store Green component (0.0 - 1.0)
     * @param b Pointer to store Blue component (0.0 - 1.0)
     **/
    void xyy_to_srgb(float x, float y, float Y_lum, float *r, float *g, float *b);

    /**
     * @brief Converts 16-bit CIE xy coordinates to 8-bit sRGB (R, G, B),
     * performing Gamut Mapping to maximize luminance (Y) within the sRGB gamut.
     *
     * @param x_in, y_in Scaled CIE xy coordinates (0-65535).
     * @param rgb_out Pointer to the output RGB_color_t structure.
     **/
    void xy_to_rgb(uint16_t x, uint16_t y, RGB_color_t *rgb_out);

#ifdef __cplusplus
}
#endif
