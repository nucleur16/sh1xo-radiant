#include "codol_render/code_tables.hpp"

namespace codol::render
{
	namespace
	{
		// Y: the pairing index <-> register name was MEASURED with `codol_render_check names` over eight stock zones
		//    ( mp_killhouse_sh, common_mp, code_post_gfx_mp, code_post_gfx_default_mp, common_gametype_mp, mp_crash_tropical,
		//    mp_underpass, zm_asylum_sh: 114692 code arguments, every one feeds a register its shader's constant table
		//    names, and no index was seen under a name other than the one below ).
		// N: no shader of those zones uses the index; the name is the IW4 one ( INFERRED ).
		constexpr bool Y = true;
		constexpr bool N = false;

		// index -> { unique name, name in the shaders' constant tables, IW4 CONST_SRC_CODE_ name, observed }
		// Iw5ShaderTables.cs:11-152 gives the 1.1.1.13 index of every IW5 name; 56 is the constant 1.1.1.13 adds
		// ( CodeConstantMap.cs:10 "overlayTexCoord at 56" ). filterTap, particleCloudMatrix and codeMeshArg are
		// HLSL arrays: the shader names the whole array, the indices are its elements.
		constexpr code_name CONSTANTS[build::CODE_CONSTANT_COUNT] =
		{
			{ "lightPosition", "lightPosition", "LIGHT_POSITION", Y },	// 0
			{ "lightDiffuse", "lightDiffuse", "LIGHT_DIFFUSE", Y },
			{ "lightSpecular", "lightSpecular", "LIGHT_SPECULAR", Y },
			{ "lightSpotDir", "lightSpotDir", "LIGHT_SPOTDIR", Y },
			{ "lightSpotFactors", "lightSpotFactors", "LIGHT_SPOTFACTORS", Y },
			{ "lightFalloffPlacement", "lightFalloffPlacement", "LIGHT_FALLOFF_PLACEMENT", Y },	// 5
			{ "particleCloudColor", "particleCloudColor", "PARTICLE_CLOUD_COLOR", Y },
			{ "gameTime", "gameTime", "GAMETIME", Y },
			{ "ssaoParms", "ssaoParms", "SSAO_PARMS", N },
			{ "pixelCostFracs", "pixelCostFracs", "PIXEL_COST_FRACS", Y },
			{ "pixelCostDecode", "pixelCostDecode", "PIXEL_COST_DECODE", N },	// 10
			{ "filterTap0", "filterTap", "FILTER_TAP_0", Y },
			{ "filterTap1", "filterTap", "FILTER_TAP_1", Y },
			{ "filterTap2", "filterTap", "FILTER_TAP_2", Y },
			{ "filterTap3", "filterTap", "FILTER_TAP_3", Y },
			{ "filterTap4", "filterTap", "FILTER_TAP_4", Y },	// 15
			{ "filterTap5", "filterTap", "FILTER_TAP_5", Y },
			{ "filterTap6", "filterTap", "FILTER_TAP_6", Y },
			{ "filterTap7", "filterTap", "FILTER_TAP_7", Y },
			{ "colorMatrixR", "colorMatrixR", "COLOR_MATRIX_R", N },
			{ "colorMatrixG", "colorMatrixG", "COLOR_MATRIX_G", N },	// 20
			{ "colorMatrixB", "colorMatrixB", "COLOR_MATRIX_B", N },
			{ "shadowmapPolygonOffset", "shadowmapPolygonOffset", "SHADOWMAP_POLYGON_OFFSET", Y },
			{ "renderTargetSize", "renderTargetSize", "RENDER_TARGET_SIZE", Y },
			{ "renderSourceSize", "renderSourceSize", "RENDER_SOURCE_SIZE", Y },
			{ "dofEquationViewModelAndFarBlur", "dofEquationViewModelAndFarBlur", "DOF_EQUATION_VIEWMODEL_AND_FAR_BLUR", Y },	// 25
			{ "dofEquationScene", "dofEquationScene", "DOF_EQUATION_SCENE", Y },
			{ "dofLerpScale", "dofLerpScale", "DOF_LERP_SCALE", Y },
			{ "dofLerpBias", "dofLerpBias", "DOF_LERP_BIAS", Y },
			{ "dofRowDelta", "dofRowDelta", "DOF_ROW_DELTA", N },
			{ "motionMatrixX", "motionMatrixX", "MOTION_MATRIX_X", N },	// 30
			{ "motionMatrixY", "motionMatrixY", "MOTION_MATRIX_Y", N },
			{ "motionMatrixW", "motionMatrixW", "MOTION_MATRIX_W", N },
			{ "shadowmapSwitchPartition", "shadowmapSwitchPartition", "SHADOWMAP_SWITCH_PARTITION", Y },
			{ "shadowmapScale", "shadowmapScale", "SHADOWMAP_SCALE", Y },
			{ "zNear", "zNear", "ZNEAR", N },	// 35
			{ "lightingLookupScale", "lightingLookupScale", "LIGHTING_LOOKUP_SCALE", Y },
			{ "debugBumpmap", "debugBumpmap", "DEBUG_BUMPMAP", Y },
			{ "materialColor", "materialColor", "MATERIAL_COLOR", Y },
			{ "fogConsts", "fogConsts", "FOG", Y },
			{ "fogColorLinear", "fogColorLinear", "FOG_COLOR_LINEAR", Y },	// 40
			{ "fogColorGamma", "fogColorGamma", "FOG_COLOR_GAMMA", Y },
			{ "fogSunConsts", "fogSunConsts", "FOG_SUN_CONSTS", Y },
			{ "fogSunColorLinear", "fogSunColorLinear", "FOG_SUN_COLOR_LINEAR", Y },
			{ "fogSunColorGamma", "fogSunColorGamma", "FOG_SUN_COLOR_GAMMA", Y },
			{ "fogSunDir", "fogSunDir", "FOG_SUN_DIR", Y },	// 45
			{ "glowSetup", "glowSetup", "GLOW_SETUP", Y },
			{ "glowApply", "glowApply", "GLOW_APPLY", N },
			{ "colorBias", "colorBias", "COLOR_BIAS", Y },
			{ "colorTintBase", "colorTintBase", "COLOR_TINT_BASE", Y },
			{ "colorTintDelta", "colorTintDelta", "COLOR_TINT_DELTA", Y },	// 50
			{ "colorTintQuadraticDelta", "colorTintQuadraticDelta", "COLOR_TINT_QUADRATIC_DELTA", Y },
			{ "outdoorFeatherParms", "outdoorFeatherParms", "OUTDOOR_FEATHER_PARMS", Y },
			{ "envMapParms", "envMapParms", "ENVMAP_PARMS", N },
			{ "sunShadowmapPixelAdjust", "sunShadowmapPixelAdjust", "SUN_SHADOWMAP_PIXEL_ADJUST", Y },
			{ "spotShadowmapPixelAdjust", "spotShadowmapPixelAdjust", "SPOT_SHADOWMAP_PIXEL_ADJUST", Y },	// 55
			{ "overlayTexCoord", "overlayTexCoord", "OVERLAY_TEXCOORD ( 1.1.1.13 only )", Y },
			{ "compositeFXDistortion", "compositeFXDistortion", "COMPOSITE_FX_DISTORTION", N },
			{ "postFxFadeEffect", "postFxFadeEffect", "POSTFX_FADE_EFFECT", N },
			{ "viewportDimensions", "viewportDimensions", "VIEWPORT_DIMENSIONS", Y },
			{ "framebufferRead", "framebufferRead", "FRAMEBUFFER_READ", N },	// 60
			{ "baseLightingCoords", "baseLightingCoords", "BASE_LIGHTING_COORDS", Y },
			{ "lightprobeAmbient", "lightprobeAmbient", "LIGHT_PROBE_AMBIENT", Y },
			{ "nearPlaneOrg", "nearPlaneOrg", "NEARPLANE_ORG", N },
			{ "nearPlaneDx", "nearPlaneDx", "NEARPLANE_DX", N },
			{ "nearPlaneDy", "nearPlaneDy", "NEARPLANE_DY", N },	// 65
			{ "clipSpaceLookupScale", "clipSpaceLookupScale", "CLIP_SPACE_LOOKUP_SCALE", Y },
			{ "clipSpaceLookupOffset", "clipSpaceLookupOffset", "CLIP_SPACE_LOOKUP_OFFSET", Y },
			{ "particleCloudMatrix0", "particleCloudMatrix", "PARTICLE_CLOUD_MATRIX0", Y },
			{ "particleCloudMatrix1", "particleCloudMatrix1", "PARTICLE_CLOUD_MATRIX1", Y },
			{ "particleCloudMatrix2", "particleCloudMatrix2", "PARTICLE_CLOUD_MATRIX2", Y },	// 70
			{ "particleCloudSparkColor0", "particleCloudSparkColor0", "PARTICLE_CLOUD_SPARK_COLOR0", Y },
			{ "particleCloudSparkColor1", "particleCloudSparkColor1", "PARTICLE_CLOUD_SPARK_COLOR1", Y },
			{ "particleCloudSparkColor2", "particleCloudSparkColor2", "PARTICLE_CLOUD_SPARK_COLOR2", Y },
			{ "particleFountainParms0", "particleFountainParms0", "PARTICLE_FOUNTAIN_PARM0", N },
			{ "particleFountainParms1", "particleFountainParms1", "PARTICLE_FOUNTAIN_PARM1", N },	// 75
			{ "depthFromClip", "depthFromClip", "DEPTH_FROM_CLIP", Y },
			{ "codeMeshArg0", "codeMeshArg", "CODE_MESH_ARG_0", Y },
			{ "codeMeshArg1", "codeMeshArg", "CODE_MESH_ARG_1", Y },
			// 79 = FIRST_CODE_MATRIX: 14 groups x { matrix, inverse, transpose, inverse transpose } ( the enum order ). The name is what the
			// shaders call the register: an HLSL float4x4 is stored by columns, so "worldMatrix" is fed by TRANSPOSE_WORLD_MATRIX.
			{ "transposeViewMatrix", "transposeViewMatrix", "VIEW_MATRIX", N },	// 79
			{ "inverseTransposeViewMatrix", "inverseTransposeViewMatrix", "INVERSE_VIEW_MATRIX", N },
			{ "viewMatrix", "viewMatrix", "TRANSPOSE_VIEW_MATRIX", N },
			{ "inverseViewMatrix", "inverseViewMatrix", "INVERSE_TRANSPOSE_VIEW_MATRIX", N },
			{ "transposeProjectionMatrix", "transposeProjectionMatrix", "PROJECTION_MATRIX", N },	// 83
			{ "inverseTransposeProjectionMatrix", "inverseTransposeProjectionMatrix", "INVERSE_PROJECTION_MATRIX", N },
			{ "projectionMatrix", "projectionMatrix", "TRANSPOSE_PROJECTION_MATRIX", Y },
			{ "inverseProjectionMatrix", "inverseProjectionMatrix", "INVERSE_TRANSPOSE_PROJECTION_MATRIX", N },
			{ "transposeViewProjectionMatrix", "transposeViewProjectionMatrix", "VIEW_PROJECTION_MATRIX", N },	// 87
			{ "inverseTransposeViewProjectionMatrix", "inverseTransposeViewProjectionMatrix", "INVERSE_VIEW_PROJECTION_MATRIX", N },
			{ "viewProjectionMatrix", "viewProjectionMatrix", "TRANSPOSE_VIEW_PROJECTION_MATRIX", Y },
			{ "inverseViewProjectionMatrix", "inverseViewProjectionMatrix", "INVERSE_TRANSPOSE_VIEW_PROJECTION_MATRIX", Y },
			{ "transposeShadowLookupMatrix", "transposeShadowLookupMatrix", "SHADOW_LOOKUP_MATRIX", N },	// 91
			{ "inverseTransposeShadowLookupMatrix", "inverseTransposeShadowLookupMatrix", "INVERSE_SHADOW_LOOKUP_MATRIX", N },
			{ "shadowLookupMatrix", "shadowLookupMatrix", "TRANSPOSE_SHADOW_LOOKUP_MATRIX", Y },
			{ "inverseShadowLookupMatrix", "inverseShadowLookupMatrix", "INVERSE_TRANSPOSE_SHADOW_LOOKUP_MATRIX", N },
			{ "transposeWorldOutdoorLookupMatrix", "transposeWorldOutdoorLookupMatrix", "WORLD_OUTDOOR_LOOKUP_MATRIX", N },	// 95
			{ "inverseTransposeWorldOutdoorLookupMatrix", "inverseTransposeWorldOutdoorLookupMatrix", "INVERSE_WORLD_OUTDOOR_LOOKUP_MATRIX", N },
			{ "worldOutdoorLookupMatrix", "worldOutdoorLookupMatrix", "TRANSPOSE_WORLD_OUTDOOR_LOOKUP_MATRIX", Y },
			{ "inverseWorldOutdoorLookupMatrix", "inverseWorldOutdoorLookupMatrix", "INVERSE_TRANSPOSE_WORLD_OUTDOOR_LOOKUP_MATRIX", N },
			{ "transposeWorldMatrix", "transposeWorldMatrix", "WORLD_MATRIX0", N },	// 99
			{ "inverseTransposeWorldMatrix", "inverseTransposeWorldMatrix", "INVERSE_WORLD_MATRIX0", N },
			{ "worldMatrix", "worldMatrix", "TRANSPOSE_WORLD_MATRIX0", Y },
			{ "inverseWorldMatrix", "inverseWorldMatrix", "INVERSE_TRANSPOSE_WORLD_MATRIX0", N },
			{ "transposeWorldViewMatrix", "transposeWorldViewMatrix", "WORLD_VIEW_MATRIX0", N },	// 103
			{ "inverseTransposeWorldViewMatrix", "inverseTransposeWorldViewMatrix", "INVERSE_WORLD_VIEW_MATRIX0", Y },
			{ "worldViewMatrix", "worldViewMatrix", "TRANSPOSE_WORLD_VIEW_MATRIX0", Y },
			{ "inverseWorldViewMatrix", "inverseWorldViewMatrix", "INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX0", Y },
			{ "transposeWorldViewProjectionMatrix", "transposeWorldViewProjectionMatrix", "WORLD_VIEW_PROJECTION_MATRIX0", N },	// 107
			{ "inverseTransposeWorldViewProjectionMatrix", "inverseTransposeWorldViewProjectionMatrix", "INVERSE_WORLD_VIEW_PROJECTION_MATRIX0", N },
			{ "worldViewProjectionMatrix", "worldViewProjectionMatrix", "TRANSPOSE_WORLD_VIEW_PROJECTION_MATRIX0", Y },
			{ "inverseWorldViewProjectionMatrix", "inverseWorldViewProjectionMatrix", "INVERSE_TRANSPOSE_WORLD_VIEW_PROJECTION_MATRIX0", N },
			{ "transposeWorldMatrix1", "transposeWorldMatrix1", "WORLD_MATRIX1", N },	// 111
			{ "inverseTransposeWorldMatrix1", "inverseTransposeWorldMatrix1", "INVERSE_WORLD_MATRIX1", N },
			{ "worldMatrix1", "worldMatrix1", "TRANSPOSE_WORLD_MATRIX1", N },
			{ "inverseWorldMatrix1", "inverseWorldMatrix1", "INVERSE_TRANSPOSE_WORLD_MATRIX1", N },
			{ "transposeWorldViewMatrix1", "transposeWorldViewMatrix1", "WORLD_VIEW_MATRIX1", N },	// 115
			{ "inverseTransposeWorldViewMatrix1", "inverseTransposeWorldViewMatrix1", "INVERSE_WORLD_VIEW_MATRIX1", N },
			{ "worldViewMatrix1", "worldViewMatrix1", "TRANSPOSE_WORLD_VIEW_MATRIX1", Y },
			{ "inverseWorldViewMatrix1", "inverseWorldViewMatrix1", "INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX1", N },
			{ "transposeWorldViewProjectionMatrix1", "transposeWorldViewProjectionMatrix1", "WORLD_VIEW_PROJECTION_MATRIX1", N },	// 119
			{ "inverseTransposeWorldViewProjectionMatrix1", "inverseTransposeWorldViewProjectionMatrix1", "INVERSE_WORLD_VIEW_PROJECTION_MATRIX1", N },
			{ "worldViewProjectionMatrix1", "worldViewProjectionMatrix1", "TRANSPOSE_WORLD_VIEW_PROJECTION_MATRIX1", N },
			{ "inverseWorldViewProjectionMatrix1", "inverseWorldViewProjectionMatrix1", "INVERSE_TRANSPOSE_WORLD_VIEW_PROJECTION_MATRIX1", N },
			{ "transposeWorldMatrix2", "transposeWorldMatrix2", "WORLD_MATRIX2", N },	// 123
			{ "inverseTransposeWorldMatrix2", "inverseTransposeWorldMatrix2", "INVERSE_WORLD_MATRIX2", N },
			{ "worldMatrix2", "worldMatrix2", "TRANSPOSE_WORLD_MATRIX2", N },
			{ "inverseWorldMatrix2", "inverseWorldMatrix2", "INVERSE_TRANSPOSE_WORLD_MATRIX2", N },
			{ "transposeWorldViewMatrix2", "transposeWorldViewMatrix2", "WORLD_VIEW_MATRIX2", N },	// 127
			{ "inverseTransposeWorldViewMatrix2", "inverseTransposeWorldViewMatrix2", "INVERSE_WORLD_VIEW_MATRIX2", N },
			{ "worldViewMatrix2", "worldViewMatrix2", "TRANSPOSE_WORLD_VIEW_MATRIX2", Y },
			{ "inverseWorldViewMatrix2", "inverseWorldViewMatrix2", "INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX2", N },
			{ "transposeWorldViewProjectionMatrix2", "transposeWorldViewProjectionMatrix2", "WORLD_VIEW_PROJECTION_MATRIX2", N },	// 131
			{ "inverseTransposeWorldViewProjectionMatrix2", "inverseTransposeWorldViewProjectionMatrix2", "INVERSE_WORLD_VIEW_PROJECTION_MATRIX2", N },
			{ "worldViewProjectionMatrix2", "worldViewProjectionMatrix2", "TRANSPOSE_WORLD_VIEW_PROJECTION_MATRIX2", N },
			{ "inverseWorldViewProjectionMatrix2", "inverseWorldViewProjectionMatrix2", "INVERSE_TRANSPOSE_WORLD_VIEW_PROJECTION_MATRIX2", N },
		};

		// Iw5ShaderTables.cs:154-187: the IW4 TEXTURE_SRC_CODE_ numbering.
		// Observed register names: 3, 6, 7, 13, 14, 15 as below. The post effect shaders read 8 .. 12 through registers
		// called colorMapSampler / colorMapSampler1 / colorMapSampler2, so for those ( and for the indices no scanned
		// shader uses ) the name is a description, not something to look up in a constant table: bind them by index.
		constexpr code_name SAMPLERS[build::CODE_SAMPLER_COUNT] =
		{
			{ "black", "", "BLACK", N },													// 0
			{ "white", "", "WHITE", N },
			{ "identityNormalMap", "", "IDENTITY_NORMAL_MAP", N },
			{ "modelLightingSampler", "modelLightingSampler", "MODEL_LIGHTING", Y },
			{ "lightmapSamplerPrimary", "", "LIGHTMAP_PRIMARY", N },
			{ "lightmapSamplerSecondary", "", "LIGHTMAP_SECONDARY", N },				// 5
			{ "shadowmapSamplerSun", "shadowmapSamplerSun", "SHADOWMAP_SUN", Y },
			{ "shadowmapSamplerSpot", "shadowmapSamplerSpot", "SHADOWMAP_SPOT", Y },
			{ "feedbackSampler", "colorMapSampler", "FEEDBACK", Y },
			{ "resolvedPostSun", "colorMapSampler", "RESOLVED_POST_SUN", Y },
			{ "resolvedScene", "colorMapSampler", "RESOLVED_SCENE", Y },				// 10
			{ "postEffect0", "colorMapSampler / colorMapSampler2", "POST_EFFECT_0", Y },
			{ "postEffect1", "colorMapSampler1", "POST_EFFECT_1", Y },
			{ "attenuationSampler", "attenuationSampler", "LIGHT_ATTENUATION", Y },
			{ "outdoorMapSampler", "outdoorMapSampler", "OUTDOOR", Y },
			{ "floatZSampler", "floatZSampler", "FLOATZ", Y },							// 15
			{ "processedFloatZSampler", "", "PROCESSED_FLOATZ", N },
			{ "rawFloatZSampler", "", "RAW_FLOATZ", N },
			{ "halfParticleColorSampler", "", "HALF_PARTICLES", N },
			{ "halfParticleDepthSampler", "", "HALF_PARTICLES_Z", N },
			{ "caseTexture", "", "CASE_TEXTURE", N },										// 20
			{ "cinematicYSampler", "", "CINEMATIC_Y", N },
			{ "cinematicCrSampler", "", "CINEMATIC_CR", N },
			{ "cinematicCbSampler", "", "CINEMATIC_CB", N },
			{ "cinematicASampler", "", "CINEMATIC_A", N },
			{ "reflectionProbeSampler", "", "REFLECTION_PROBE", N },						// 25
		};
	}

	const code_name* code_constant(const int index)
	{
		return index >= 0 && index < build::CODE_CONSTANT_COUNT ? &CONSTANTS[index] : nullptr;
	}

	int code_constant_index(const std::string_view name)
	{
		for (int i = 0; i < build::CODE_CONSTANT_COUNT; i++)
		{
			if (name == CONSTANTS[i].name)
			{
				return i;
			}
		}
		return -1;
	}

	const code_name* code_sampler(const int index)
	{
		return index >= 0 && index < build::CODE_SAMPLER_COUNT ? &SAMPLERS[index] : nullptr;
	}

	bool code_sampler_state(const int code_index, const std::string_view ctab_name, const std::uint8_t light_def_sampler_state, std::uint8_t& sampler_state)
	{
		// GameMapRenderer.cs:1689-1721
		if (code_index == code_sampler_index::FEEDBACK || code_index == code_sampler_index::RESOLVED_POST_SUN
			|| code_index == code_sampler_index::RESOLVED_SCENE)
		{
			sampler_state = 0x02 | 0x60;
			return true;
		}
		if (ctab_name == "modelLightingSampler")
		{
			sampler_state = 0x02 | 0xE0;
			return true;
		}
		if (ctab_name == "attenuationSampler")
		{
			sampler_state = light_def_sampler_state;
			return true;
		}
		if (ctab_name == "shadowmapSamplerSun" || ctab_name == "shadowmapSamplerSpot" || ctab_name == "floatZSampler")
		{
			sampler_state = 0x01 | 0x60;
			return true;
		}
		if (ctab_name == "outdoorMapSampler")
		{
			sampler_state = 0x02 | 0x60;
			return true;
		}
		sampler_state = 0x02;
		return false;
	}
}
