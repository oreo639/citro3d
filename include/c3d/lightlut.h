/**
 * @file lightlut.h
 * @brief Generate lighting lookup tables
 */
#pragma once
#include "types.h"
#include <math.h>

typedef struct
{
	u32 data[256];
} C3D_LightLut;

typedef struct
{
	C3D_LightLut lut;
	float bias, scale;
} C3D_LightLutDA;

typedef float (* C3D_LightLutFunc)(float x, float param);
typedef float (* C3D_LightLutFuncDA)(float dist, float arg0, float arg1);

static inline float quadratic_dist_attn(float dist, float linear, float quad)
{
	return 1.0f / (1.0f + linear*dist + quad*dist*dist);
}

static inline float spot_step(float angle, float cutoff)
{
	return angle >= cutoff ? 1.0f : 0.0f;
}

/**
 * @brief Generates lighting lookup table from pre-computed float array.
 * @param[out] lut  Pointer to lighting lookup table structure.
 * @param[in]  data Pointer to pre-computed float array.
 */
void LightLut_FromArray(C3D_LightLut* lut, float* data);

/**
 * @brief Generates lighting lookup table using specified callback function.
 * @param[out] lut      Pointer to light environment structure.
 * @param[in]  func     Callback function.
 * @param[in]  param    User-specified parameter to callback function.
 * @param[in]  negative If false, x argument of callback function will be in range [0,256]. If true, x argument will be in range [-128,128].
 */
void LightLut_FromFunc(C3D_LightLut* lut, C3D_LightLutFunc func, float param, bool negative);

/**
 * @brief Generates distance attenuation lookup table using specified callback function.
 * @param[out] lut  Pointer to light environment structure.
 * @param[in]  func Callback function.
 * @param[in]  from Starting distance of lighting.
 * @param[in]  to   Ending distance of lighting.
 * @param[in]  arg0 User-specified parameter to callback function.
 * @param[in]  arg1 User-specified parameter to callback function.
 */
void LightLutDA_Create(C3D_LightLutDA* lut, C3D_LightLutFuncDA func, float from, float to, float arg0, float arg1);

/**
 * @brief Generates lighting lookup table using powf function (phong lighting).
 * @param[out] lut       Pointer to light environment structure.
 * @param[in]  shininess Shininess value of specular highlights (pow function exponent).
 */
#define LightLut_Phong(lut, shininess) LightLut_FromFunc((lut), powf, (shininess), false)

/**
 * @brief Generates lighting lookup table for spotlights.
 * @param[out] lut   Pointer to light environment structure.
 * @param[in]  angle Beam angle of the spotlight.
 */
#define LightLut_Spotlight(lut, angle) LightLut_FromFunc((lut), spot_step, cosf(angle), true)

/**
 * @brief Generates distance attenuation lookup table.
 * @param[out] lut    Pointer to light environment structure.
 * @param[in]  from   Minimum distance from light source.
 * @param[in]  to     Maximum distance from light source.
 * @param[in]  linear Linear coefficient.
 * @param[in]  quad   Quadratic coefficient.
 */
#define LightLutDA_Quadratic(lut, from, to, linear, quad) LightLutDA_Create((lut), quadratic_dist_attn, (from), (to), (linear), (quad))
