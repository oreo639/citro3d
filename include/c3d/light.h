/**
 * @file light.h
 * @brief Configure dynamic light, shading, and shadows
 *
 * \section frag_eqn Fragment Light Equations
 * The equations used for calculating fragment lighting appears to be as follows:
 *
 * \f[ C_{pri} = s^{(a)} + \sum_{i = 0}^{\#lights} a * SpotlightLut(d_0) * o * (l_i^{(d)} * f_i(L_i \cdot N) + l_i^{(a)}) \f]
 * \f[ C_{sec} = \sum_{i = 0}^{\#lights} a * SpotlightLut(d_0) * h * o * (l_i^{(s_0)}LutD_0(d_1)*G_i^{(0)} + l_i^{(s_1)}LutD_1(d_3)*G_i^{(1)}*ReflectionLutsRGB(d_2)) \f]
 * \f[ C_{alpha} = FresnelLut(d_4) \f]
 *
 * Outputs:
 * - \f$C_{pri}\f$ - \ref GPU_FRAGMENT_PRIMARY_COLOR
 * - \f$C_{sec}\f$ - \ref GPU_FRAGMENT_SECONDARY_COLOR
 * - \f$C_{alpha}\f$ - Primary and/or secondary alpha. Output is selectable using \ref C3D_LightEnvFresnel().
 *
 * Inputs, per-fragment:
 * - \f$a\f$ - Distance attenuation factor calculated from distance attenuation LUT.
 * - \f$N\f$ - Interpolated normal
 * - \f$V\f$ - View direction vector (fragment <-> camera)
 * - \f$T\f$ - Tangent direction vector
 *
 * Inputs, per-pass:
 * - \f$d_{0...4}\f$ - Configurable LUT inputs - one of the following: \f$N \cdot H\f$, \f$V \cdot H_i\f$, \f$N \cdot V\f$, \f$L_i \cdot N\f$, \f$-L_i \cdot P\f$, \f$\cos \phi_i\f$.
 * - \f$s^{(a)}\f$ - Scene ambient color
 * - \f$o\f$ - Shadow attenuation from the shadow map (if there is one). Output is selectable using \ref C3D_LightEnvShadowMode().
 * - \f$h\f$ - Clamps lighting for \f$N \cdot L_i < 0\f$ if \ref C3D_LightEnvClampHighlights() is enabled
 *
 * Inputs, per-Light:
 * - \f$P_i\f$ - Spotlight direction
 * - \f$L_i\f$ - Light vector (just lightPosition when positional lighting is enabled, lightPosition + view when directional lighting is enabled)
 * - \f$H_i\f$ - Half-vector between \f$L_i\f$ and \f$V\f$
 * - \f$\phi_i\f$ - Angle between the projection of \f$H_i\f$ into the tangent plane and \f$T\f$
 * - \f$f_i\f$ - Clamps product of \f$N \cdot L_i\f$ to zero if \ref C3D_LightTwoSideDiffuse() is disabled for the light source, otherwise gets the absolute value
 * - \f$l_i^{(a)}\f$ - Light ambient color
 * - \f$l_i^{(d)}\f$ - Light diffuse color
 * - \f$l_i^{(s_0)}\f$ - Light specular0 color
 * - \f$l_i^{(s_1)}\f$ - Light specular1 color
 * - \f$G_i^{(0)},G_i^{(1)}\f$ - Cook-Torrance geometric factor, or 1 when disabled
 *
 * In citro3d, some inputs may be configured by multiple variables, for example:
 * - \f$s^{(a)}\f$ = mtl.emission + mtl.ambient * env.ambient
 * - \f$l_i^{(a)}\f$ = mtl.ambient * light.ambient
 * - \f$l_i^{(d)}\f$ = mtl.diffuse * light.diffuse
 * - \f$l_i^{(s_0)}\f$ = mtl.specular0 * light.specular0
 * - \f$l_i^{(s_1)}\f$ = mtl.specular1 * light.specular1
 *
 * \sa https://github.com/PabloMK7/citra/blob/baca2bfc6bd0af97ce74b911d69af2391815c9d7/src/video_core/renderer_software/sw_lighting.cpp#L26-L332
 */

#pragma once
#include "lightlut.h"
#include "maths.h"

/// Material
typedef struct
{
	float ambient[3];   ///< Color used for global illumination for the material, multiplied by \ref C3D_LightEnvAmbient()
	float diffuse[3];   ///< Color used when calculating directional lighting
	float specular0[3]; ///< Specular color, multiplied by lutD0
	float specular1[3]; ///< Specular color, multiplied by lutD1
	float emission[3];  ///< Color used for global illumination for the material, added to the product of the ambient illumination
} C3D_Material;

/**
 * @name Light Environment
 * @{
 */

// Forward declarations
typedef struct C3D_Light_t C3D_Light;
typedef struct C3D_LightEnv_t C3D_LightEnv;

typedef struct
{
	u32 abs, select, scale;
} C3D_LightLutInputConf;

typedef struct
{
	u32 ambient;
	u32 numLights;
	u32 config[2];
	C3D_LightLutInputConf lutInput;
	u32 permutation;
} C3D_LightEnvConf;

enum
{
	C3DF_LightEnv_Dirty    = BIT(0),
	C3DF_LightEnv_MtlDirty = BIT(1),
	C3DF_LightEnv_LCDirty  = BIT(2),

#define C3DF_LightEnv_IsCP(n)     BIT(18+(n))
#define C3DF_LightEnv_IsCP_Any    (0xFF<<18)
#define C3DF_LightEnv_LutDirty(n) BIT(26+(n))
#define C3DF_LightEnv_LutDirtyAll (0x3F<<26)
};

struct C3D_LightEnv_t
{
	u32 flags;
	C3D_LightLut* luts[6];
	float ambient[3];
	C3D_Light* lights[8];
	C3D_LightEnvConf conf;
	C3D_Material material;
};

/**
 * @brief Resets and initializes \ref C3D_LightEnv structure to default values.
 * @note Using fragment lighting without at least one light source configured and enabled
 *       may result in undefined behavior.
 * @param[out] env Pointer to light environment structure.
 * @return Light id on success, negative on failure.
 * @sa \ref C3D_LightInit()
 * @sa \ref frag_eqn
 */
void C3D_LightEnvInit(C3D_LightEnv* env);

/**
 * @brief Binds \ref C3D_LightEnv pointer to be used for configuring internal state.
 * @param[in] env Pointer to light environment structure or NULL to disable the fragment lighting stage altogether.
 */
void C3D_LightEnvBind(C3D_LightEnv* env);

/**
 * @brief Coppies material properties to \ref C3D_LightEnv.
 * @param[out] env Pointer to light environment structure.
 * @param[in]  mtl Pointer to material properties structure.
 */
void C3D_LightEnvMaterial(C3D_LightEnv* env, const C3D_Material* mtl);

/**
 * @brief Sets global ambient lighting.
 * @param[out] env Pointer to light environment structure.
 * @param[in]  r   Red component.
 * @param[in]  g   Green component.
 * @param[in]  b   Blue component.
 */
void C3D_LightEnvAmbient(C3D_LightEnv* env, float r, float g, float b);

/**
 * @brief Uploads pre-calculated lighting lookup table for specified function.
 * @note For the full lighting equation see the \ref frag_eqn section.
 *       This is used to calculate the values of \ref GPU_FRAGMENT_PRIMARY_COLOR
 *       and \ref GPU_FRAGMENT_SECONDARY_COLOR.
 * @param[out] env      Pointer to light environment structure.
 * @param[in]  lutId    Specify function of the lookup table.
 * @param[in]  input    Specify arguments of the function.
 * @param[in]  negative If true, the LUT inputs can be read as positive or negative, if false the absolute value of the lut inputs will be used.
 * @param[in]  lut      Pointer to pre-computed lookup table, or NULL to disable the function.
 * @sa \ref LightLut_FromArray()
 * @sa \ref LightLut_FromFunc()
 */
void C3D_LightEnvLut(C3D_LightEnv* env, GPU_LIGHTLUTID lutId, GPU_LIGHTLUTINPUT input, bool negative, C3D_LightLut* lut);

enum
{
	GPU_SHADOW_PRIMARY   = BIT(16),
	GPU_SHADOW_SECONDARY = BIT(17),
	GPU_INVERT_SHADOW    = BIT(18),
	GPU_SHADOW_ALPHA     = BIT(19),
};

/**
 * @brief Enables or disables writing fresnel and shadow alpha component to \ref GPU_FRAGMENT_PRIMARY_COLOR and \ref GPU_FRAGMENT_SECONDARY_COLOR.
 * @param[out] env      Light environment.
 * @param[in]  selector Output selector, or \ref GPU_NO_FRESNEL to disable writing the alpha
 *                      component to both \ref GPU_FRAGMENT_PRIMARY_COLOR and \ref GPU_FRAGMENT_SECONDARY_COLOR.
 * @sa \ref frag_eqn
 */
void C3D_LightEnvFresnel(C3D_LightEnv* env, GPU_FRESNELSEL selector);

/**
 * @brief Configures bump map texture properties.
 * @param[out] env  Pointer to light environment structure.
 * @param[in]  mode Bump map type. Use \ref GPU_BUMP_AS_BUMP to specify normal map,
 *             use \ref GPU_BUMP_AS_TANG to specify tangent map, or use \ref GPU_BUMP_NOT_USED to disable bump mapping.
 * @sa C3D_LightEnvBumpSel()
 */
void C3D_LightEnvBumpMode(C3D_LightEnv* env, GPU_BUMPMODE mode);

/**
 * @brief Configures bump map texture unit id.
 * @param[out] env     Pointer to light environment structure.
 * @param[in]  texUnit Id of texture unit the bump texture is bound to (IDs 0 through 2).
 * @sa C3D_LightEnvBumpMode()
 * @sa C3D_TexBind()
 */
void C3D_LightEnvBumpSel(C3D_LightEnv* env, int texUnit);

/**
 * @brief Configures whether to use the z component of the normal map.
 * @param[out] env    Pointer to light environment structure.
 * @param[in]  enable true enables using the z component from the normal map,
 *                    false enables z component reconstruction from the xy components of the normal map.
 */
void C3D_LightEnvBumpNormalZ(C3D_LightEnv *env, bool enable);

/**
 * @brief Configures shadow mapping behavior.
 * @param[out] env  Pointer to light environment structure.
 * @param[in]  mode One or more of the following bitflags, \ref GPU_SHADOW_PRIMARY,
 *                  \ref GPU_SHADOW_SECONDARY, \ref GPU_INVERT_SHADOW, and \ref GPU_SHADOW_ALPHA.
 */
void C3D_LightEnvShadowMode(C3D_LightEnv* env, u32 mode);

/**
 * @brief Configures shadow mapping texture.
 * @note Shadow depth textures must be assigned to texture unit 0.
 * @param[out] env     Pointer to light environment structure.
 * @param[in]  texUnit Id of texture unit the shadow texture is bound to (IDs 0 through 2).
 */
void C3D_LightEnvShadowSel(C3D_LightEnv* env, int texUnit);

/**
 * @brief Enables or disables clamping specular highlights.
 * @param[out] env   Pointer to light environment structure.
 * @param[in]  clamp true to enable clamping specular highlights based on the normal vector,
 *                   false to disable clamping specular highlights.
 */
void C3D_LightEnvClampHighlights(C3D_LightEnv* env, bool clamp);

/** @} */

/**
 * @name Light
 * @{
 */

typedef struct
{
	u32 specular0, specular1, diffuse, ambient;
} C3D_LightMatConf;

typedef struct
{
	C3D_LightMatConf material;
	u16 position[3]; u16 padding0;
	u16 spotDir[3];  u16 padding1;
	u32 padding2;
	u32 config;
	u32 distAttnBias, distAttnScale;
} C3D_LightConf;

enum
{
	C3DF_Light_Enabled  = BIT(0),
	C3DF_Light_Dirty    = BIT(1),
	C3DF_Light_MatDirty = BIT(2),
	//C3DF_Light_Shadow   = BIT(3),
	//C3DF_Light_Spot     = BIT(4),
	//C3DF_Light_DistAttn = BIT(5),

	C3DF_Light_SPDirty  = BIT(14),
	C3DF_Light_DADirty  = BIT(15),
};

struct C3D_Light_t
{
	u16 flags, id;
	C3D_LightEnv* parent;
	C3D_LightLut *lut_SP, *lut_DA;
	float ambient[3];
	float diffuse[3];
	float specular0[3];
	float specular1[3];
	C3D_LightConf conf;
};

/**
 * @brief Adds light to \ref C3D_LightEnv.
 * @note Only 8 lights can be configured simultaneously.
 * @param[in]  light Light struct to add to light environment.
 * @param[out] env   Light environment.
 * @return Light id on success, negative on failure.
 * @sa \ref frag_eqn
 */
int  C3D_LightInit(C3D_Light* light, C3D_LightEnv* env);

/**
 * @brief Enables or disables light source.
 * @note At least one light source must be enabled at all times. Disabling all
 *       light sources will result in undefined behavior.
 * @param[out] light  Light source structure.
 * @param[in]  enable true to enable light source, false to disable light source.
 */
void C3D_LightEnable(C3D_Light* light, bool enable);

/**
 * @brief Enables or disables light source.
 * @param[out] light  Light source structure.
 * @param[in]  enable true to enable two sided lighting (illuminates both the inside and outside of a mesh),
 *                    false to disable two sided lighting.
 */
void C3D_LightTwoSideDiffuse(C3D_Light* light, bool enable);

/**
 * @brief Enables or disables cock-torrance geometric factor.
 * @param[out] light  Light source structure.
 * @param[in]  id     Geometric factor id. (0 or 1)
 * @param[in]  enable true to enable geometric factor id, false to disable it.
 */
void C3D_LightGeoFactor(C3D_Light* light, int id, bool enable);

/**
 * @brief Configures global ambient color emitted by light source.
 * @param[out] light  Light source structure.
 * @param[in]  r      Red component.
 * @param[in]  g      Green component.
 * @param[in]  b      Blue component.
 */
void C3D_LightAmbient(C3D_Light* light, float r, float g, float b);

/**
 * @brief Configures diffuse lighting color emitted by light source.
 * @param[out] light  Light source structure.
 * @param[in]  r      Red component.
 * @param[in]  g      Green component.
 * @param[in]  b      Blue component.
 */
void C3D_LightDiffuse(C3D_Light* light, float r, float g, float b);

/**
 * @brief Configures specular0 lighting color emitted by light source.
 * @param[out] light  Light source structure.
 * @param[in]  r      Red component.
 * @param[in]  g      Green component.
 * @param[in]  b      Blue component.
 */
void C3D_LightSpecular0(C3D_Light* light, float r, float g, float b);

/**
 * @brief Configures specular1 lighting color emitted by light source.
 * @param[out] light  Light source structure.
 * @param[in]  r      Red component.
 * @param[in]  g      Green component.
 * @param[in]  b      Blue component.
 */
void C3D_LightSpecular1(C3D_Light* light, float r, float g, float b);

/**
 * @brief Configures light position vector.
 * @note The w-component of the position vector is a flag where
 *       0 specifies positional lighting and any other value specifies directional lighting.
 * @param[out] light  Light source structure.
 * @param[in]  pos    Position vector.
 */
void C3D_LightPosition(C3D_Light* light, C3D_FVec* pos);

/**
 * @brief Enables or disables shadow mapping on light source.
 * @note The shadow mapping texture can be specified using \ref C3D_LightEnvShadowSel().
 * @param[out] light  Light source structure.
 * @param[in]  enable true to enable shadow mapping, false to disable shadow mapping.
 */
void C3D_LightShadowEnable(C3D_Light* light, bool enable);

/**
 * @brief Enables or disables spot light for specified light source.
 * @param[out] light  Light source structure.
 * @param[in]  enable true to enable spot light, false to disable spot light.
 */
void C3D_LightSpotEnable(C3D_Light* light, bool enable);

/**
 * @brief Configures spot light direction vector for specified light source.
 * @param[out] light Light source structure.
 * @param[in]  x     X component.
 * @param[in]  y     Y component.
 * @param[in]  z     Z component.
 */
void C3D_LightSpotDir(C3D_Light* light, float x, float y, float z);

/**
 * @brief Configures spotlight lookup table.
 * @param[out] light Light source structure.
 * @param[in]  lut   Pointer to pre-computed lighting lookup table.
 */
void C3D_LightSpotLut(C3D_Light* light, C3D_LightLut* lut);

/**
 * @brief Enables or disables distance attenuation for specified light source.
 * @param[out] light  Light source structure.
 * @param[in]  enable Enable distance attenuation factor for light source.
 */
void C3D_LightDistAttnEnable(C3D_Light* light, bool enable);

/**
 * @brief Uploads pre-calculated distance attenuation lookup table for specified light source.
 * @param[out] light Light source structure.
 * @param[in]  lut   Pointer to pre-computed distance attenuation lookup table.
 */
void C3D_LightDistAttn(C3D_Light* light, C3D_LightLutDA* lut);

/**
 * @brief Configures diffuse and specular0/1 color emitted by light source.
 * @param[out] light  Light source structure.
 * @param[in]  r      Red component.
 * @param[in]  g      Green component.
 * @param[in]  b      Blue component.
 */
static inline void C3D_LightColor(C3D_Light* light, float r, float g, float b)
{
	C3D_LightDiffuse(light, r, g, b);
	C3D_LightSpecular0(light, r, g, b);
	C3D_LightSpecular1(light, r, g, b);
}

/** @} */
