#include "codol_render/code_tables.hpp"

namespace codol::render
{
	namespace
	{
		constexpr bool Y = true;	// pairing seen in the game's zones by `codol_render_check names`
		constexpr bool N = false;	// not used by any scanned zone: IW4 name, inferred

		// index -> { HLSL name, IW4 CONST_SRC_CODE_ name, observed }
		// Iw5ShaderTables.cs:11-152 gives the 1.1.1.13 index of every IW5 name; 56 is the constant 1.1.1.13 adds
		// ( CodeConstantMap.cs:10 "overlayTexCoord at 56" ).
		constexpr code_name CONSTANTS[build::CODE_CONSTANT_COUNT] =
		{
			{ "lightPosition", "LIGHT_POSITION", N },											// 0
			{ "lightDiffuse", "LIGHT_DIFFUSE", N },
			{ "lightSpecular", "LIGHT_SPECULAR", N },
			{ "lightSpotDir", "LIGHT_SPOTDIR", N },
			{ "lightSpotFactors", "LIGHT_SPOTFACTORS", N },
			{ "lightFalloffPlacement", "LIGHT_FALLOFF_PLACEMENT", N },							// 5
			{ "particleCloudColor", "PARTICLE_CLOUD_COLOR", N },
			{ "gameTime", "GAMETIME", N },
			{ "ssaoParms", "SSAO_PARMS", N },
			{ "pixelCostFracs", "PIXEL_COST_FRACS", N },
			{ "pixelCostDecode", "PIXEL_COST_DECODE", N },										// 10
			{ "filterTap0", "FILTER_TAP_0", N },
			{ "filterTap1", "FILTER_TAP_1", N },
			{ "filterTap2", "FILTER_TAP_2", N },
			{ "filterTap3", "FILTER_TAP_3", N },
			{ "filterTap4", "FILTER_TAP_4", N },												// 15
			{ "filterTap5", "FILTER_TAP_5", N },
			{ "filterTap6", "FILTER_TAP_6", N },
			{ "filterTap7", "FILTER_TAP_7", N },
			{ "colorMatrixR", "COLOR_MATRIX_R", N },
			{ "colorMatrixG", "COLOR_MATRIX_G", N },											// 20
			{ "colorMatrixB", "COLOR_MATRIX_B", N },
			{ "shadowmapPolygonOffset", "SHADOWMAP_POLYGON_OFFSET", N },
			{ "renderTargetSize", "RENDER_TARGET_SIZE", N },
			{ "renderSourceSize", "RENDER_SOURCE_SIZE", N },
			{ "dofEquationViewModelAndFarBlur", "DOF_EQUATION_VIEWMODEL_AND_FAR_BLUR", N },	// 25
			{ "dofEquationScene", "DOF_EQUATION_SCENE", N },
			{ "dofLerpScale", "DOF_LERP_SCALE", N },
			{ "dofLerpBias", "DOF_LERP_BIAS", N },
			{ "dofRowDelta", "DOF_ROW_DELTA", N },
			{ "motionMatrixX", "MOTION_MATRIX_X", N },											// 30
			{ "motionMatrixY", "MOTION_MATRIX_Y", N },
			{ "motionMatrixW", "MOTION_MATRIX_W", N },
			{ "shadowmapSwitchPartition", "SHADOWMAP_SWITCH_PARTITION", N },
			{ "shadowmapScale", "SHADOWMAP_SCALE", N },
			{ "zNear", "ZNEAR", N },															// 35
			{ "lightingLookupScale", "LIGHTING_LOOKUP_SCALE", N },
			{ "debugBumpmap", "DEBUG_BUMPMAP", N },
			{ "materialColor", "MATERIAL_COLOR", N },
			{ "fogConsts", "FOG", N },
			{ "fogColorLinear", "FOG_COLOR_LINEAR", N },										// 40
			{ "fogColorGamma", "FOG_COLOR_GAMMA", N },
			{ "fogSunConsts", "FOG_SUN_CONSTS", N },
			{ "fogSunColorLinear", "FOG_SUN_COLOR_LINEAR", N },
			{ "fogSunColorGamma", "FOG_SUN_COLOR_GAMMA", N },
			{ "fogSunDir", "FOG_SUN_DIR", N },													// 45
			{ "glowSetup", "GLOW_SETUP", N },
			{ "glowApply", "GLOW_APPLY", N },
			{ "colorBias", "COLOR_BIAS", N },
			{ "colorTintBase", "COLOR_TINT_BASE", N },
			{ "colorTintDelta", "COLOR_TINT_DELTA", N },										// 50
			{ "colorTintQuadraticDelta", "COLOR_TINT_QUADRATIC_DELTA", N },
			{ "outdoorFeatherParms", "OUTDOOR_FEATHER_PARMS", N },
			{ "envMapParms", "ENVMAP_PARMS", N },
			{ "sunShadowmapPixelAdjust", "SUN_SHADOWMAP_PIXEL_ADJUST", N },
			{ "spotShadowmapPixelAdjust", "SPOT_SHADOWMAP_PIXEL_ADJUST", N },					// 55
			{ "overlayTexCoord", "OVERLAY_TEXCOORD", N },
			{ "compositeFXDistortion", "COMPOSITE_FX_DISTORTION", N },
			{ "postFxFadeEffect", "POSTFX_FADE_EFFECT", N },
			{ "viewportDimensions", "VIEWPORT_DIMENSIONS", N },
			{ "framebufferRead", "FRAMEBUFFER_READ", N },										// 60
			{ "baseLightingCoords", "BASE_LIGHTING_COORDS", N },
			{ "lightprobeAmbient", "LIGHT_PROBE_AMBIENT", N },
			{ "nearPlaneOrg", "NEARPLANE_ORG", N },
			{ "nearPlaneDx", "NEARPLANE_DX", N },
			{ "nearPlaneDy", "NEARPLANE_DY", N },												// 65
			{ "clipSpaceLookupScale", "CLIP_SPACE_LOOKUP_SCALE", N },
			{ "clipSpaceLookupOffset", "CLIP_SPACE_LOOKUP_OFFSET", N },
			{ "particleCloudMatrix0", "PARTICLE_CLOUD_MATRIX0", N },
			{ "particleCloudMatrix1", "PARTICLE_CLOUD_MATRIX1", N },
			{ "particleCloudMatrix2", "PARTICLE_CLOUD_MATRIX2", N },							// 70
			{ "particleCloudSparkColor0", "PARTICLE_CLOUD_SPARK_COLOR0", N },
			{ "particleCloudSparkColor1", "PARTICLE_CLOUD_SPARK_COLOR1", N },
			{ "particleCloudSparkColor2", "PARTICLE_CLOUD_SPARK_COLOR2", N },
			{ "particleFountainParms0", "PARTICLE_FOUNTAIN_PARM0", N },
			{ "particleFountainParms1", "PARTICLE_FOUNTAIN_PARM1", N },							// 75
			{ "depthFromClip", "DEPTH_FROM_CLIP", N },
			{ "codeMeshArg0", "CODE_MESH_ARG_0", N },
			{ "codeMeshArg1", "CODE_MESH_ARG_1", N },
			// 79: FIRST_CODE_MATRIX, 14 groups of { matrix, inverse, transpose, inverse transpose }
			{ "viewMatrix", "VIEW_MATRIX", N },
			{ "inverseViewMatrix", "INVERSE_VIEW_MATRIX", N },
			{ "transposeViewMatrix", "TRANSPOSE_VIEW_MATRIX", N },
			{ "inverseTransposeViewMatrix", "INVERSE_TRANSPOSE_VIEW_MATRIX", N },
			{ "projectionMatrix", "PROJECTION_MATRIX", N },										// 83
			{ "inverseProjectionMatrix", "INVERSE_PROJECTION_MATRIX", N },
			{ "transposeProjectionMatrix", "TRANSPOSE_PROJECTION_MATRIX", N },
			{ "inverseTransposeProjectionMatrix", "INVERSE_TRANSPOSE_PROJECTION_MATRIX", N },
			{ "viewProjectionMatrix", "VIEW_PROJECTION_MATRIX", N },							// 87
			{ "inverseViewProjectionMatrix", "INVERSE_VIEW_PROJECTION_MATRIX", N },
			{ "transposeViewProjectionMatrix", "TRANSPOSE_VIEW_PROJECTION_MATRIX", N },
			{ "inverseTransposeViewProjectionMatrix", "INVERSE_TRANSPOSE_VIEW_PROJECTION_MATRIX", N },
			{ "shadowLookupMatrix", "SHADOW_LOOKUP_MATRIX", N },								// 91
			{ "inverseShadowLookupMatrix", "INVERSE_SHADOW_LOOKUP_MATRIX", N },
			{ "transposeShadowLookupMatrix", "TRANSPOSE_SHADOW_LOOKUP_MATRIX", N },
			{ "inverseTransposeShadowLookupMatrix", "INVERSE_TRANSPOSE_SHADOW_LOOKUP_MATRIX", N },
			{ "worldOutdoorLookupMatrix", "WORLD_OUTDOOR_LOOKUP_MATRIX", N },					// 95
			{ "inverseWorldOutdoorLookupMatrix", "INVERSE_WORLD_OUTDOOR_LOOKUP_MATRIX", N },
			{ "transposeWorldOutdoorLookupMatrix", "TRANSPOSE_WORLD_OUTDOOR_LOOKUP_MATRIX", N },
			{ "inverseTransposeWorldOutdoorLookupMatrix", "INVERSE_TRANSPOSE_WORLD_OUTDOOR_LOOKUP_MATRIX", N },
			{ "worldMatrix", "WORLD_MATRIX0", N },												// 99
			{ "inverseWorldMatrix", "INVERSE_WORLD_MATRIX0", N },
			{ "transposeWorldMatrix", "TRANSPOSE_WORLD_MATRIX0", N },
			{ "inverseTransposeWorldMatrix", "INVERSE_TRANSPOSE_WORLD_MATRIX0", N },
			{ "worldViewMatrix", "WORLD_VIEW_MATRIX0", N },										// 103
			{ "inverseWorldViewMatrix", "INVERSE_WORLD_VIEW_MATRIX0", N },
			{ "transposeWorldViewMatrix", "TRANSPOSE_WORLD_VIEW_MATRIX0", N },
			{ "inverseTransposeWorldViewMatrix", "INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX0", N },
			{ "worldViewProjectionMatrix", "WORLD_VIEW_PROJECTION_MATRIX0", N },				// 107
			{ "inverseWorldViewProjectionMatrix", "INVERSE_WORLD_VIEW_PROJECTION_MATRIX0", N },
			{ "transposeWorldViewProjectionMatrix", "TRANSPOSE_WORLD_VIEW_PROJECTION_MATRIX0", N },
			{ "inverseTransposeWorldViewProjectionMatrix", "INVERSE_TRANSPOSE_WORLD_VIEW_PROJECTION_MATRIX0", N },
			{ "worldMatrix1", "WORLD_MATRIX1", N },												// 111
			{ "inverseWorldMatrix1", "INVERSE_WORLD_MATRIX1", N },
			{ "transposeWorldMatrix1", "TRANSPOSE_WORLD_MATRIX1", N },
			{ "inverseTransposeWorldMatrix1", "INVERSE_TRANSPOSE_WORLD_MATRIX1", N },
			{ "worldViewMatrix1", "WORLD_VIEW_MATRIX1", N },									// 115
			{ "inverseWorldViewMatrix1", "INVERSE_WORLD_VIEW_MATRIX1", N },
			{ "transposeWorldViewMatrix1", "TRANSPOSE_WORLD_VIEW_MATRIX1", N },
			{ "inverseTransposeWorldViewMatrix1", "INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX1", N },
			{ "worldViewProjectionMatrix1", "WORLD_VIEW_PROJECTION_MATRIX1", N },				// 119
			{ "inverseWorldViewProjectionMatrix1", "INVERSE_WORLD_VIEW_PROJECTION_MATRIX1", N },
			{ "transposeWorldViewProjectionMatrix1", "TRANSPOSE_WORLD_VIEW_PROJECTION_MATRIX1", N },
			{ "inverseTransposeWorldViewProjectionMatrix1", "INVERSE_TRANSPOSE_WORLD_VIEW_PROJECTION_MATRIX1", N },
			{ "worldMatrix2", "WORLD_MATRIX2", N },												// 123
			{ "inverseWorldMatrix2", "INVERSE_WORLD_MATRIX2", N },
			{ "transposeWorldMatrix2", "TRANSPOSE_WORLD_MATRIX2", N },
			{ "inverseTransposeWorldMatrix2", "INVERSE_TRANSPOSE_WORLD_MATRIX2", N },
			{ "worldViewMatrix2", "WORLD_VIEW_MATRIX2", N },									// 127
			{ "inverseWorldViewMatrix2", "INVERSE_WORLD_VIEW_MATRIX2", N },
			{ "transposeWorldViewMatrix2", "TRANSPOSE_WORLD_VIEW_MATRIX2", N },
			{ "inverseTransposeWorldViewMatrix2", "INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX2", N },
			{ "worldViewProjectionMatrix2", "WORLD_VIEW_PROJECTION_MATRIX2", N },				// 131
			{ "inverseWorldViewProjectionMatrix2", "INVERSE_WORLD_VIEW_PROJECTION_MATRIX2", N },
			{ "transposeWorldViewProjectionMatrix2", "TRANSPOSE_WORLD_VIEW_PROJECTION_MATRIX2", N },
			{ "inverseTransposeWorldViewProjectionMatrix2", "INVERSE_TRANSPOSE_WORLD_VIEW_PROJECTION_MATRIX2", N },
		};

		// Iw5ShaderTables.cs:154-187: the IW4 TEXTURE_SRC_CODE_ numbering
		constexpr code_name SAMPLERS[build::CODE_SAMPLER_COUNT] =
		{
			{ "black", "BLACK", N },											// 0
			{ "white", "WHITE", N },
			{ "identityNormalMap", "IDENTITY_NORMAL_MAP", N },
			{ "modelLightingSampler", "MODEL_LIGHTING", N },
			{ "lightmapSamplerPrimary", "LIGHTMAP_PRIMARY", N },
			{ "lightmapSamplerSecondary", "LIGHTMAP_SECONDARY", N },			// 5
			{ "shadowmapSamplerSun", "SHADOWMAP_SUN", N },
			{ "shadowmapSamplerSpot", "SHADOWMAP_SPOT", N },
			{ "feedbackSampler", "FEEDBACK", N },
			{ "resolvedPostSun", "RESOLVED_POST_SUN", N },
			{ "resolvedScene", "RESOLVED_SCENE", N },							// 10
			{ "postEffect0", "POST_EFFECT_0", N },
			{ "postEffect1", "POST_EFFECT_1", N },
			{ "attenuationSampler", "LIGHT_ATTENUATION", N },
			{ "outdoorMapSampler", "OUTDOOR", N },
			{ "floatZSampler", "FLOATZ", N },									// 15
			{ "processedFloatZSampler", "PROCESSED_FLOATZ", N },
			{ "rawFloatZSampler", "RAW_FLOATZ", N },
			{ "halfParticleColorSampler", "HALF_PARTICLES", N },
			{ "halfParticleDepthSampler", "HALF_PARTICLES_Z", N },
			{ "caseTexture", "CASE_TEXTURE", N },								// 20
			{ "cinematicYSampler", "CINEMATIC_Y", N },
			{ "cinematicCrSampler", "CINEMATIC_CR", N },
			{ "cinematicCbSampler", "CINEMATIC_CB", N },
			{ "cinematicASampler", "CINEMATIC_A", N },
			{ "reflectionProbeSampler", "REFLECTION_PROBE", N },				// 25
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
