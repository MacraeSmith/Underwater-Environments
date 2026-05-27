
#include "Data/Shaders/Utils/CommonUtils.hlsli"
#include "Data/Shaders/Utils/TextureUtils.hlsli"
#include "Data/Shaders/Utils/UnderwaterLightingUtils.hlsli"
//------------------------------------------------------------------------------------------------
struct vs_input_t
{
	float3 modelPosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float3 modelTangent : TANGENT;
	float3 modelBitangent : BITANGENT;
	float3 modelNormal : NORMAL;
};

//------------------------------------------------------------------------------------------------
struct v2p_t
{
	float4 position : SV_Position;
	float4 clipPosition : TEXCOORD0;
	float4 color : TEXCOORD1;
	float2 uv : TEXCOORD2;
	float3 worldTangent : TANGENT;
	float3 worldBitangent : BITANGENT;
	float3 worldNormal : NORMAL;
	float4 worldPosition : TEXCOORD6;
	float linearDepth : TEXCOORD7;
	float3 waveDisplacement : TEXCOORD8;
    float totalWaveHeight : TEXCOORD9;
};

struct LightData
{
    float4 c_color; //alpha is intensity [0,1]
    float3 c_position;
    float c_innerRadius;
    float c_outerRadius;
    float3 c_spotForward;
    float c_penumbraInnerDotThreshold;
    float c_penumbraOuterDotThreshold;
    float2 LIGHTPADDING;
};

struct WaveAccumulation
{
    float3 displacement;
    float3 tangentDerivative;
    float3 bitangentDerivative;
    float totalWaveHeight;
};

#define MAX_LIGHTS 64

//Constant Buffers
//------------------------------------------------------------------------------------------------
cbuffer PerFrameConstants : register(b1)
{
	float c_time;
	int c_debugInt;
	float c_debugFloat;
	float PADDING;	
};

//------------------------------------------------------------------------------------------------
cbuffer CameraConstants : register(b2)
{
	float4x4 c_worldToCameraTransform;	// View transform
	float4x4 c_cameraToRenderTransform;	// Non-standard transform from game to DirectX conventions
	float4x4 c_renderToClipTransform;		// Projection transform
	float3 c_cameraPosition;
	float CAMERA_PADDING;
};

//------------------------------------------------------------------------------------------------
cbuffer ModelConstants : register(b3)
{
	float4x4 c_modelToWorldTransform;		// Model transform
	float4 c_modelColor;
};

//------------------------------------------------------------------------------------------------
cbuffer LightConstants : register(b4)
{
	float3 c_sunDirection;
	float c_sunIntensity;
	float3 c_sunColorRGB;
	float c_ambientIntensity;
    int c_numLights;
	float3 LIGHT_CONSTANTS_PADDING;
	
    LightData c_lights[ MAX_LIGHTS];
};

#define NUM_TEXTURE_SLOTS 16
#define SCENE_COLOR_SLOT 0
#define SCENE_DEPTH_SLOT 1
#define TERRAIN_COLOR_SLOT 2
#define TERRAIN_DEPTH_SLOT 3
#define TERRAIN_SEA_DEPTH_SLOT 4
#define  WATER_NORMAL_MAP_SLOT 5
#define FOAM_MASK_SLOT 6

#define SKYBOX_TEXTURE_INDEX_OFFSET 7
cbuffer TextureConstants : register(b5)
{
	uint4 textureIndexes[NUM_TEXTURE_SLOTS / 4];
};

#define FOG_INDEX_UNDERWATER 0
#define FOG_INDEX_ABOVE_WATER 1
struct FogData
{
    float4 color;
    float depth;
    float opacity;
    uint transitionExponent;
    float pad;
};

cbuffer FogConstants : register(b6)
{
    FogData c_fogData[2];
    float c_underWaterTintDepthScale;
    float c_fogVolumeHeight;
    float3 FOG_PADDING;
}; //b6

#define PI 3.14159265
#define MAX_WAVES 8
cbuffer WaterConstants : register(b8)
{
	float4 c_waveData[MAX_WAVES];
	float4 c_waveDataExtra[MAX_WAVES];
	int c_numWaves;
	float c_globalSpeedScale;
    float c_globalCrestSteepnessScale;
    float c_globalAmplitudeScale;
    float c_globalWavelengthScale;
    
    float c_directionVarianceRegionScale;
    float c_waveDirectionVariance;
    float c_amplitudeVarianceRegionScale;
    float c_waveAmplitudeVariance;
    
	float c_seaLevel;
	float c_underwaterTintDepthScale;
    float c_underwaterDepthDarkenScale;
	float c_foaminess;
    float c_reflectionStrength;
    float c_refractionStrength;
    float c_maxTerrainDepth;
	float c_waterMirkyness;
	float c_shorelineFoamStrength;
	float c_worldVoxelScale;
    float c_specularIntensity;
    float c_specularPower;
   
};

cbuffer InverseCameraConstants : register(b13)
{
    float4x4 ClipToWorldTransform;
};

#define NEAR_PlANE 0.1
#define FAR_PLANE 500

cbuffer DebugRenderConstants : register(b15)
{
    int c_default;
    int c_albedo;
    int c_normals;
    int c_ambientOcclusion;
    int c_lightStrength;
    int c_noNormals;
    int c_noAmbientOcclusion;
    int c_specularAttenuation;
    int c_noSpecularAttenuation;
    int c_sceneDepth;
    int c_emission;
    int c_biomeWeight1;
    int c_biomeWeight2;
    int c_biomeWeight3;
    int c_biomeWeight4;
    int c_strongestBiome;
    int c_fogVolumeMasks;
};

//Texture and Sampler constants
//------------------------------------------------------------------------------------------------
#define MAX_NUM_BINDLESS_TEXTURES 4096
Texture2D<float4> t_textures[MAX_NUM_BINDLESS_TEXTURES] : register(t0, space0);
SamplerState s_sampler : register(s0);
SamplerState s_wrapSampler : register(s1);

//Helper functions
//------------------------------------------------------------------------------------------------


WaveAccumulation AccumulateWaterWaves(float2 basePositionXY)
{
    WaveAccumulation waveAccumulation;
    waveAccumulation.displacement = float3(0.0f, 0.0f, 0.0f);
    waveAccumulation.tangentDerivative = float3(1.0f, 0.0f, 0.0f);
    waveAccumulation.bitangentDerivative = float3(0.0f, 1.0f, 0.0f);
    waveAccumulation.totalWaveHeight = 0.0f;
    
    float directionRegionScale = lerp(0.0025f, 0.00005f, c_directionVarianceRegionScale);
    float2 directionRegionSamplePosition = (basePositionXY * directionRegionScale);

    float amplitudeRegionScale = lerp(0.0025f, 0.00005f, c_amplitudeVarianceRegionScale);
    float2 amplitudeRegionSamplePosition = (basePositionXY * amplitudeRegionScale);

    float amplitudeNoise = snoise((amplitudeRegionSamplePosition + float2(17.13f, 9.37f)));
    amplitudeNoise = ((amplitudeNoise * 0.5f) + 0.5f);
    amplitudeNoise = smoothstep(0.6f, 0.9f, amplitudeNoise);
    float amplitudeMultiplier = (1.0f + (amplitudeNoise * c_waveAmplitudeVariance * 50.f));
    float amplitudeChoppinessReduction = (1.0f / amplitudeMultiplier);
    
    float globalHeightChoppinessReduction = c_globalAmplitudeScale - 1.0;

    
    for (int waveIndex = 0; waveIndex < c_numWaves; ++waveIndex)
    {
        float4 waveData = c_waveData[waveIndex];
        float4 waveDataExtra = c_waveDataExtra[waveIndex];

        float2 baseWaveDirection = normalize(waveData.xy);
        float baseAmplitude = waveData.z * 0.1f * c_globalAmplitudeScale;
        float waveLength = waveData.w * 0.1f * c_globalWavelengthScale;

        float waveSpeed = waveDataExtra.x;
        float waveSteepness = waveDataExtra.y;
        float waveOffset = waveDataExtra.z;
        
        // Per-wave regional variation
        float2 perWaveSampleOffset = float2(((float) waveIndex * 12.13f), ((float) waveIndex * 9.37f));
        float regionNoiseDirection = snoise((directionRegionSamplePosition + perWaveSampleOffset));

        float directionOffsetRadians = (regionNoiseDirection * c_waveDirectionVariance);
        float directionVarianceAmount = abs(directionOffsetRadians);
        float directionVarianceNormalized = saturate((directionVarianceAmount / max(c_waveDirectionVariance, 0.0001f)));
        directionVarianceNormalized = smoothstep(0.7, 1.0, directionVarianceNormalized);
        float directionChoppinessReduction = (1.0 - directionVarianceNormalized);
        float localChoppiness = ((c_globalCrestSteepnessScale * amplitudeChoppinessReduction) * directionChoppinessReduction);
        localChoppiness -= globalHeightChoppinessReduction;

        float2 waveDirection = normalize(RotateVector2D(baseWaveDirection, directionOffsetRadians));
        float localAmplitude = (baseAmplitude * amplitudeMultiplier);

        float waveNumber = ((2.0f * PI) / waveLength);
        float angularFrequency = (0.2f * sqrt(9.81f * waveNumber));
        float waveTime = ((c_time * c_globalSpeedScale) * waveSpeed);

        float waveNumberTimesAmplitude = (waveNumber * localAmplitude);
        float maxStableSteepness = (1.0f / (waveNumberTimesAmplitude + 1e-6f));

        float phasePosition = dot(waveDirection, basePositionXY);
        float wavePhase = (((phasePosition * waveNumber) - (angularFrequency * waveTime)) + waveOffset);

        float sinPhase = sin(wavePhase);
        float cosPhase = cos(wavePhase);

        // Choppiness controls only horizontal push
        float clampedSteepness = min(waveSteepness * localChoppiness, maxStableSteepness);
        float horizontalDisplacementStrength = clampedSteepness;
        float horizontalDisplacement = (horizontalDisplacementStrength * localAmplitude);
        float derivativeHorizontalFactor = (horizontalDisplacementStrength * waveNumberTimesAmplitude);
        float derivativeVerticalFactor = waveNumberTimesAmplitude;

        waveAccumulation.displacement.x += ((waveDirection.x * horizontalDisplacement) * cosPhase);
        waveAccumulation.displacement.y += ((waveDirection.y * horizontalDisplacement) * cosPhase);
        waveAccumulation.displacement.z += (localAmplitude * sinPhase);

        waveAccumulation.tangentDerivative.x += (((-derivativeHorizontalFactor * waveDirection.x) * waveDirection.x) * sinPhase);
        waveAccumulation.tangentDerivative.y += (((-derivativeHorizontalFactor * waveDirection.x) * waveDirection.y) * sinPhase);
        waveAccumulation.tangentDerivative.z += ((derivativeVerticalFactor * waveDirection.x) * cosPhase);

        waveAccumulation.bitangentDerivative.x += (((-derivativeHorizontalFactor * waveDirection.x) * waveDirection.y) * sinPhase);
        waveAccumulation.bitangentDerivative.y += (((-derivativeHorizontalFactor * waveDirection.y) * waveDirection.y) * sinPhase);
        waveAccumulation.bitangentDerivative.z += ((derivativeVerticalFactor * waveDirection.y) * cosPhase);

        waveAccumulation.totalWaveHeight += localAmplitude;
    }

    waveAccumulation.totalWaveHeight = max(waveAccumulation.totalWaveHeight, 0.0001f);
    return waveAccumulation;
}

uint GetSkyBoxFaceIndexFromDirection(float3 direction)
{
    float3 absoluteDirection = abs(direction);
    uint faceIndex = 5;
    if ((absoluteDirection.x >= absoluteDirection.y) && (absoluteDirection.x >= absoluteDirection.z))
    {
        faceIndex = (direction.x >= 0.0f) ? 0u : 3u; // +X : -X
    }

    if ((absoluteDirection.y >= absoluteDirection.x) && (absoluteDirection.y >= absoluteDirection.z))
    {
        faceIndex = (direction.y >= 0.0f) ? 1u : 4u; // +Y : -Y
    }

    if ((absoluteDirection.z >= absoluteDirection.x) && (absoluteDirection.z >= absoluteDirection.y))
    {
        faceIndex = (direction.z >= 0.0f) ? 2u : 5u; // +Z : -Z
    }

    return faceIndex;
}

float2 GetSkyBoxFaceUVFromDirection(float3 direction, uint faceIndex)
{
    float2 faceUV = float2(0.0f, 0.0f);

    // These formulas produce faceUV in [-1, 1] range, then we remap to [0, 1].
    // They assume a standard cube projection. If a face looks mirrored, you flip U or V for that face only.
    if (faceIndex == 0u) // +X
    {
        faceUV = float2((-direction.y) / abs(direction.x), (direction.z) / abs(direction.x));
    }
    else if (faceIndex == 3u) // -X
    {
        faceUV = float2((direction.y) / abs(direction.x), (direction.z) / abs(direction.x));
    }
    else if (faceIndex == 1u) // +Y
    {
        faceUV = float2((direction.x) / abs(direction.y), (direction.z) / abs(direction.y));
    }
    else if (faceIndex == 4u) // -Y
    {
        faceUV = float2((-direction.x) / abs(direction.y), (direction.z) / abs(direction.y));
    }
    else if (faceIndex == 2u) // +Z
    {
        faceUV = float2((direction.x) / abs(direction.z), (-direction.y) / abs(direction.z));
    }
    else // 5u, -Z
    {
        faceUV = float2((direction.x) / abs(direction.z), (direction.y) / abs(direction.z));
    }

    // [-1,1] to [0,1]
    faceUV = ((faceUV * 0.5f) + 0.5f);

    return faceUV;
}

float3 SampleSkyboxSixTextures(float3 direction, SamplerState samplerState)
{
    direction = normalize(direction);

    uint faceIndex = GetSkyBoxFaceIndexFromDirection(direction);
    float2 faceUV = GetSkyBoxFaceUVFromDirection(direction, faceIndex);

    uint group = 0u;
    uint offset = 0u;
    uint skyboxSlot = (SKYBOX_TEXTURE_INDEX_OFFSET + faceIndex);

    GetTextureGroupAndOffset(skyboxSlot, group, offset);

    Texture2D<float4> faceTexture = t_textures[textureIndexes[group][offset]];
    float3 faceColor = faceTexture.Sample(samplerState, faceUV).rgb;

    return faceColor;
}

float3 GetWaterPerturbedNormal(v2p_t input, uint normalTextureSlot)
{
    uint group = 0u;
    uint offset = 0u;
    GetTextureGroupAndOffset(normalTextureSlot, group, offset);

    Texture2D<float4> normalTexture = t_textures[textureIndexes[group][offset]];

    float3 surfaceNormalWorldSpace = normalize(input.worldNormal);
    float3 surfaceTangentWorldSpace = normalize(input.worldTangent - (surfaceNormalWorldSpace * dot(input.worldTangent, surfaceNormalWorldSpace)));
    float3 surfaceBitangentWorldSpace = normalize(cross(surfaceNormalWorldSpace, surfaceTangentWorldSpace));
    float3x3 tbnToWorld = float3x3(surfaceTangentWorldSpace, surfaceBitangentWorldSpace, surfaceNormalWorldSpace);

    float2 worldUVBase = (input.worldPosition.xy * (0.008f * c_worldVoxelScale));

    float2 scrollA = float2((c_time * 0.03f), (c_time * 0.017f));
    float2 scrollB = float2((-c_time * 0.02f), (c_time * 0.012f));

    float2 uvA = (worldUVBase * 1.0f) + scrollA;
    float2 uvB = (worldUVBase * 0.5f) + scrollB;

    float3 texelA = normalTexture.Sample(s_wrapSampler, uvA).rgb;
    float3 texelB = normalTexture.Sample(s_wrapSampler, uvB).rgb;

    float3 normalA = DecodeRGBtoXYZ(texelA);
    float3 normalB = DecodeRGBtoXYZ(texelB);

    float3 tangentSpaceNormal = normalize(normalA + normalB);
    float3 perturbedNormal = normalize(mul(tangentSpaceNormal, tbnToWorld));

    float wobbleNoise = (snoise(((input.worldPosition.xy * 0.15f) + (c_time * 0.3f))) * 0.05f);
    perturbedNormal += float3(wobbleNoise, (wobbleNoise * 0.25f), 0.0f);
    perturbedNormal = normalize(perturbedNormal);

    return perturbedNormal;
}

float GetWaterThickness(float2 screenUV, uint sceneDepthFractionMaskTextureSlot)
{
    uint group = 0u;
    uint offset = 0u;
    GetTextureGroupAndOffset(sceneDepthFractionMaskTextureSlot, group, offset);

    float terrainDepthFraction = t_textures[textureIndexes[group][offset]].Sample(s_sampler, screenUV).r;
    return (1.0f - terrainDepthFraction);
}

float GetMirkyness(float waterThickness, float rawDepth)
{
    FogData usedFogData = c_fogData[FOG_INDEX_UNDERWATER];
    float linearFogDepth = LinearizeDepth(rawDepth, 0.1f, usedFogData.depth);
    float fogAmount = 1.0 - pow(linearFogDepth, usedFogData.transitionExponent);

    float mirkyExponent = (max(c_waterMirkyness, 0.001) * 20.0f);
    float mirkyness = pow(saturate(waterThickness), mirkyExponent);
    return mirkyness * fogAmount;
}

float GetRawDepth(float2 screenUV, uint depthTextureSlot)
{
    uint group = 0u;
    uint offset = 0u;
    GetTextureGroupAndOffset(depthTextureSlot, group, offset);

    float depth = t_textures[textureIndexes[group][offset]].Sample(s_wrapSampler, screenUV).r;
    return depth;
}

float3 SampleSceneColor(float2 screenUV, uint sceneColorTextureSlot)
{
    uint group = 0u;
    uint offset = 0u;
    GetTextureGroupAndOffset(sceneColorTextureSlot, group, offset);

    Texture2D<float4> sceneColorTexture = t_textures[textureIndexes[group][offset]];
    return sceneColorTexture.Sample(s_sampler, screenUV).rgb;
}

float2 ClampScreenUV(float2 screenUV)
{
    return clamp(screenUV, float2(0.001f, 0.001f), float2(0.999f, 0.999f));
}

float2 GetRefractedScreenUV(float2 screenUV, float3 perturbedNormal, float waterThickness)
{
    float refractionAmount = saturate(waterThickness);
    refractionAmount = lerp(0.0f, 0.2f, refractionAmount);
    refractionAmount *= c_refractionStrength;

    float2 normalOffset = (perturbedNormal.xy * refractionAmount);
    return ClampScreenUV(screenUV + normalOffset);
}

float3 GetShallowWaterColor(float2 screenUV, float2 refractedUV, float waterLinearDepth, float3 perturbedNormal, float waterThickness, float3 shallowTint, uint sceneColorTextureSlot)
{
    float farPlaneStartFade = (FAR_PLANE * 0.6f);
    float farPlaneEndFade = (FAR_PLANE * 0.9f);

    float refractionDistanceFade = 1.0f - saturate((waterLinearDepth - farPlaneStartFade) / (farPlaneEndFade - farPlaneStartFade));

    float2 refractedScreenUV = refractedUV;
    refractedScreenUV = lerp(screenUV, refractedScreenUV, refractionDistanceFade);
    refractedScreenUV = ClampScreenUV(refractedScreenUV);
    float3 refractedSceneColor = SampleSceneColor(refractedScreenUV, sceneColorTextureSlot);
    
    float tintStrength = saturate(waterThickness);
    float3 shallowWaterColor = lerp(refractedSceneColor, (refractedSceneColor * shallowTint), tintStrength);
    return shallowWaterColor;
}

float2 GetShorelineFoam(float2 screenUV, float waterLinearDepth, float2 worldPositionXY, uint terrainDepthTextureSlot)
{
    float rawDepth = GetRawDepth(screenUV, terrainDepthTextureSlot);
    float terrainDepth = LinearizeDepth(rawDepth, 0.1, FAR_PLANE);
    float2 shorelineFoam = float2(0.f, 0.f);
    
    if(terrainDepth < 0.9999f)
    {
        float depthDifference = max((terrainDepth - waterLinearDepth), 0.0f);

        float shorelineBandWidth = 0.02f;
        float shorelineFadeWidth = 0.08f;

        float intersectionBand = (1.0f - smoothstep(0.0f, shorelineBandWidth, depthDifference));
        float fullFoam = (1.0f - smoothstep(0.0f, shorelineFadeWidth, depthDifference));

        float shorelineFoamExponent = (1.0f - c_shorelineFoamStrength);

        intersectionBand = pow(intersectionBand, (shorelineFoamExponent * 50.0f));
        fullFoam = pow(fullFoam, (shorelineFoamExponent * 20.0f));

        intersectionBand = saturate(intersectionBand * (1.0f + c_shorelineFoamStrength));
        fullFoam = saturate(fullFoam * (1.0f + c_shorelineFoamStrength));
        shorelineFoam = float2(intersectionBand, fullFoam);
    }
    
    return shorelineFoam;
}

float GetWaveFoamFactor(float3 perturbedNormal, float2 worldPositionXY, float worldPositionZ, float shorelineFoam, float totalWaveHeight)
{
    float waveSlope = saturate(length(perturbedNormal.xy));
    float heightFactor = saturate(((worldPositionZ - c_seaLevel) / totalWaveHeight));

    float sensitiveSlope = pow(waveSlope, 0.35f);
    float crestFactor = pow(heightFactor, 0.6f);

    float baseFoamFactor = saturate(((sensitiveSlope * 1.35f) + (crestFactor * 0.85f)));
    baseFoamFactor = pow(baseFoamFactor, 0.75f);

    float largeScaleNoise = snoise(((worldPositionXY * 0.035f) + float2((c_time * 0.05f), (-c_time * 0.03f))));
    largeScaleNoise = ((largeScaleNoise * 0.5f) + 0.5f);
    largeScaleNoise = smoothstep(0.25f, 0.85f, largeScaleNoise);

    float mediumScaleNoise = snoise(((worldPositionXY * 0.18f) + float2((-c_time * 0.22f), (c_time * 0.17f))));
    mediumScaleNoise = ((mediumScaleNoise * 0.5f) + 0.5f);
    mediumScaleNoise = smoothstep(0.2f, 0.8f, mediumScaleNoise);

    float foamFactor = (baseFoamFactor * lerp(0.45f, 1.0f, largeScaleNoise));
    foamFactor += (mediumScaleNoise * baseFoamFactor * 0.35f);

    foamFactor *= (c_foaminess * 1.5f);
    foamFactor = smoothstep(0.2f, 0.75f, foamFactor);

    foamFactor += shorelineFoam;

    return saturate(foamFactor);
}

float3 ApplyFoamColor(v2p_t input, uint foamTextureSlot, float foamFactor, float shoreLineFoam, float3 waterColor)
{
    uint group = 0u;
    uint offset = 0u;
    GetTextureGroupAndOffset(foamTextureSlot, group, offset);
    Texture2D<float4> foamTexture = t_textures[textureIndexes[group][offset]];
   
    float2 baseWorldUV = (input.worldPosition.xy * (0.0008f * c_worldVoxelScale));

    float2 foamUV_A = baseWorldUV;
    float2 foamUV_B = ((baseWorldUV * 1.12f) + float2(17.3f, 41.7f));

    float3 foamTexelA = foamTexture.Sample(s_wrapSampler, foamUV_A).rgb;
    float3 foamTexelB = foamTexture.Sample(s_wrapSampler, foamUV_B).rgb;

    float blendFactor = ((sin(c_time * 0.5f) * 0.5f) + 0.5f);

    float3 blendedFoamTexel = lerp(foamTexelA, foamTexelB, blendFactor);

    float foamTextureStrength = saturate(length(blendedFoamTexel) + (shoreLineFoam * 0.5f));
    
    
    float3 foamColor = lerp(waterColor, float3(1.0f, 1.0f, 1.0f), foamTextureStrength);
    foamColor = lerp(waterColor, foamColor, foamFactor);
    
    return foamColor;
}

float3 GetDirectionalWaterLighting(float3 normal, float3 lightDirection)
{
    float nDotL = dot((lightDirection), normal);
    float directionalLightStrength = RangeMap(nDotL, (-c_ambientIntensity), 1.0f, 0.0f, 1.0f);
    return (c_sunIntensity * saturate(directionalLightStrength));
}

float3 ApplySkyReflectionToDeepColor(float3 deepColorRGB, float3 normal, float3 viewDirection)
{
    float3 reflectionDirection = reflect((-viewDirection), normal);
    float3 skyReflectionColor = SampleSkyboxSixTextures(reflectionDirection, s_sampler);

    float nDotV = saturate(dot(normal, viewDirection));
    float F0 = (0.5f * c_reflectionStrength);
    float fresnel = (F0 + ((1.0f - F0) * pow((1.0f - nDotV), 5.0f)));

    float reflectionAmount = saturate((fresnel * c_reflectionStrength));
    return lerp(deepColorRGB, skyReflectionColor, reflectionAmount);
}

float3 GetWaterSpecular(float3 normal, float3 viewDirection, float3 lightDirection, float waveSlope)
{
    float3 halfVector = normalize((viewDirection + lightDirection));

    float nDotH = saturate(dot(normal, halfVector));
    float specularPower = lerp(48.0f, 128.0f, c_specularPower);
    float specularStrength = pow(nDotH, specularPower);

    float nDotV = saturate(dot(normal, viewDirection));
    float fresnelSpec = pow((1.0f - nDotV), 3.0f);
    fresnelSpec = lerp(0.35f, 1.0f, fresnelSpec);

    float slopeBoost = lerp(1.0f, 2.0f, waveSlope);
    float specularIntensity = (c_specularIntensity * 5.0f);

    return (c_sunColorRGB * c_sunIntensity * specularStrength * fresnelSpec * specularIntensity * slopeBoost);
}

float2 GetSunBlobCenterXY()
{
    float3 worldToSunDirection = (-c_sunDirection.xyz);
    float sunRayZ = worldToSunDirection.z;

    float2 sunBlobCenterXY = c_cameraPosition.xy;
    if (abs(sunRayZ) > 0.0001f)
    {
        float distanceAlongSunRay = ((c_seaLevel - c_cameraPosition.z) / sunRayZ);
        float3 sunBlobCenterWorld = (c_cameraPosition.xyz + (worldToSunDirection * distanceAlongSunRay));
        sunBlobCenterXY = sunBlobCenterWorld.xy;
    }

    return sunBlobCenterXY;
}

float3 GetUndersideBaseWaterColor(v2p_t input, float2 screenUV, float waterLinearDepth, float3 perturbedNormal, float3 lightDirection, float3 deepWaterColor)
{
    float refractionAmount = c_refractionStrength;
    float2 normalOffset = (perturbedNormal.xy * refractionAmount);

    float radiusSunCatch = saturate(dot(perturbedNormal, lightDirection));
    float radiusSunMask = pow(radiusSunCatch, 2.0f);
    float2 sunWeightedOffset = (perturbedNormal.xy * radiusSunMask * 0.02f);

    float2 refractedScreenUV = (screenUV + normalOffset + sunWeightedOffset);
    refractedScreenUV = ClampScreenUV(refractedScreenUV);

    float refractedRawDepth = GetRawDepth(refractedScreenUV, TERRAIN_DEPTH_SLOT);
    float refractedWaterThickness = GetWaterThickness(refractedScreenUV, TERRAIN_SEA_DEPTH_SLOT);
    float refractedShallowBlend = GetMirkyness(refractedWaterThickness, refractedRawDepth);

    float3 refractedSceneColor = SampleSceneColor(refractedScreenUV, TERRAIN_COLOR_SLOT);
    float3 refractedShallowWaterColor = GetShallowWaterColor(screenUV, refractedScreenUV, waterLinearDepth, perturbedNormal, refractedWaterThickness, float3(1.0f, 1.0f, 1.0f), 2);
    float3 refractedUnderwaterColor = lerp(refractedSceneColor, refractedShallowWaterColor, refractedShallowBlend);

    float2 sunBlobCenterXY = GetSunBlobCenterXY();
    float2 sunBlobToSurfaceXY = (input.worldPosition.xy - sunBlobCenterXY);
    float2 refractionDeltaUV = (refractedScreenUV - screenUV);

    float radiusMaskRefractionScale = 800.0f;
    float2 refractedRadiusOffsetXY = (refractionDeltaUV * radiusMaskRefractionScale);
    float2 refractedSunBlobToSurfaceXY = (sunBlobToSurfaceXY + refractedRadiusOffsetXY);
    float refractionRadiusDistance = length(refractedSunBlobToSurfaceXY);

    float refractionRadiusInner = 100.0f;
    float refractionRadiusOuter = 350.0f;
    float refractionRadiusMask = (1.0f - smoothstep(refractionRadiusInner, refractionRadiusOuter, refractionRadiusDistance));
    refractionRadiusMask *= lerp(0.7f, 1.0f, radiusSunMask);
    refractionRadiusMask = saturate(refractionRadiusMask);

    return lerp(deepWaterColor, refractedUnderwaterColor, refractionRadiusMask);
}

float3 GetUndersideSunLighting(v2p_t input, float3 perturbedNormal, float3 lightDirection)
{
    float broadTransmission = saturate(dot(perturbedNormal, lightDirection));
    broadTransmission = pow(broadTransmission, 1.5f);
    float3 broadUnderwaterLight = (c_sunColorRGB * broadTransmission * 0.12f);

    float2 sunBlobCenterXY = GetSunBlobCenterXY();
    float sunBlobRadiusScale = 0.02f;

    float2 blobUV = ((input.worldPosition.xy - sunBlobCenterXY) * sunBlobRadiusScale);
    blobUV += (perturbedNormal.xy * 1.0f);

    float blobDistance = length(blobUV);
    float largeBlob = (1.0f - smoothstep(0.20f, 1.10f, blobDistance));
    float blobCore = (1.0f - smoothstep(0.0f, 0.22f, blobDistance));
    float blobMask = saturate((largeBlob * 0.55f) + (blobCore * 0.95f));

    float3 sunBlobLight = saturate(c_sunColorRGB * blobMask * 0.65f * c_sunIntensity);
    

    float shimmerNoiseA = snoise((input.worldPosition.xy * 0.06f) + float2((c_time * 0.08f), (c_time * 0.05f)));
    float shimmerNoiseB = snoise((input.worldPosition.xy * 0.12f) + float2((-c_time * 0.05f), (c_time * 0.07f)));
    float shimmerNoise = ((shimmerNoiseA * 0.6f) + (shimmerNoiseB * 0.4f));
    shimmerNoise = ((shimmerNoise * 0.5f) + 0.5f);
    shimmerNoise = smoothstep(0.35f, 0.75f, shimmerNoise);

    float blobShimmerMask = saturate((blobMask * 0.45f) + (blobMask * shimmerNoise * 0.55f));
    float3 blobShimmerLight = (c_sunColorRGB * blobShimmerMask * 0.22f);

    return (broadUnderwaterLight + sunBlobLight + blobShimmerLight);
}

//Vertex shader
//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
    // Wave accumulation
    float2 basePositionXY = input.modelPosition.xy;
    float4 modelPosition = float4(input.modelPosition, 1.0f);
    WaveAccumulation waveAccumulation = AccumulateWaterWaves(basePositionXY);
    modelPosition.xyz += waveAccumulation.displacement;

    // Surface basis
    float3 modelTangent = normalize(waveAccumulation.tangentDerivative);
    float3 modelBitangent = normalize(waveAccumulation.bitangentDerivative);
    float3 modelNormal = normalize(cross(modelTangent, modelBitangent));

    float3x3 worldRotation = (float3x3) c_modelToWorldTransform;
    float3 worldTangent = normalize(mul(worldRotation, modelTangent));
    float3 worldBitangent = normalize(mul(worldRotation, modelBitangent));
    float3 worldNormal = normalize(mul(worldRotation, modelNormal));

    // Position transforms
    float4 worldPosition = mul(c_modelToWorldTransform, modelPosition);
    float4 cameraPosition = mul(c_worldToCameraTransform, worldPosition);
    float4 renderPosition = mul(c_cameraToRenderTransform, cameraPosition);
    float4 clipPosition = mul(c_renderToClipTransform, renderPosition);

    // Output
    v2p_t output;
    output.position = clipPosition;
    output.clipPosition = clipPosition;
    output.color = input.color;
    output.uv = input.uv;
    output.worldTangent = worldTangent;
    output.worldBitangent = worldBitangent;
    output.worldNormal = worldNormal;
    output.worldPosition = worldPosition;
    output.linearDepth = (clipPosition.z / clipPosition.w);
    output.waveDisplacement = waveAccumulation.displacement;
    output.totalWaveHeight = waveAccumulation.totalWaveHeight;
    return output;
}
//Pixel Shader
//------------------------------------------------------------------------------------------------

float4 PixelMain(v2p_t input, bool isFrontFace : SV_IsFrontFace) : SV_Target0
{
    // Shared surface data
    float3 lightDirection = normalize((-c_sunDirection.xyz));
    float waterLinearDepth = LinearizeDepth(input.linearDepth, 0.1f, FAR_PLANE);
    float2 screenUV = GetScreenUVFromClipPosition(input.clipPosition);

    float3 perturbedNormal = GetWaterPerturbedNormal(input, WATER_NORMAL_MAP_SLOT);
    if (!isFrontFace)
    {
        perturbedNormal = (-perturbedNormal);
    }

    float3 deepWaterColor = float3(0.05f, 0.25f, 0.35f);
    float3 finalColorRgb = float3(0.0f, 0.0f, 0.0f);

    if (isFrontFace)
    {
        // Top surface color
        float3 viewDirection = normalize((c_cameraPosition.xyz - input.worldPosition.xyz));
        float waveSlope = saturate(length(perturbedNormal.xy));
        float waterThickness = GetWaterThickness(screenUV, TERRAIN_SEA_DEPTH_SLOT);
        float2 refractedUV = GetRefractedScreenUV(screenUV, perturbedNormal, waterThickness);
        waterThickness = GetWaterThickness(refractedUV, TERRAIN_SEA_DEPTH_SLOT);
        float refractedRawDepth = GetRawDepth(refractedUV, TERRAIN_DEPTH_SLOT);
        float shallowBlend = GetMirkyness(waterThickness, refractedRawDepth);

        float3 shallowWaterTint = float3(0.20f, 0.85f, 0.78f);
        float3 reflectedDeepWaterColor = ApplySkyReflectionToDeepColor(deepWaterColor, perturbedNormal, viewDirection);
        //return float4(reflectedDeepWaterColor, 1.0);
        float3 shallowWaterColor = GetShallowWaterColor(screenUV, refractedUV, waterLinearDepth, perturbedNormal, waterThickness, shallowWaterTint, SCENE_COLOR_SLOT);
        float3 baseWaterColor = (lerp(reflectedDeepWaterColor, shallowWaterColor, shallowBlend) * c_sunColorRGB);
        //float3 shallowBlendColor = lerp(deepWaterColor, shallowWaterTint, shallowBlend);
        //return float4(shallowBlendColor, 1.0);
        //return float4(baseWaterColor, 1.0);

        // Foam
        float2 shorelineFoam = GetShorelineFoam(screenUV, waterLinearDepth, input.worldPosition.xy, TERRAIN_DEPTH_SLOT);
        //return float4(shorelineFoam.yyy, 1.f);
        float totalWaveHeight = max(input.totalWaveHeight, 0.0001f);
        float foamFactor = GetWaveFoamFactor(perturbedNormal, input.worldPosition.xy, input.worldPosition.z, shorelineFoam.y, totalWaveHeight);
        float3 foamColoredWater = ApplyFoamColor(input, FOAM_MASK_SLOT, foamFactor, shorelineFoam.x, baseWaterColor);
        //return float4(foamColoredWater, 1.0);

        // Lighting
        float3 directionalLighting = GetDirectionalWaterLighting(perturbedNormal, lightDirection);
        float3 specularColor = GetWaterSpecular(perturbedNormal, viewDirection, lightDirection, waveSlope);
        finalColorRgb = ((foamColoredWater * directionalLighting) + specularColor);
    }
    else
    {
        // Underside refraction and color
        float3 baseUnderwaterColor = GetUndersideBaseWaterColor(input, screenUV, waterLinearDepth, perturbedNormal, lightDirection, deepWaterColor);
        //return float4(baseUnderwaterColor, 1.0);
        
        // Underside lighting
        float3 undersideLighting = GetUndersideSunLighting(input, perturbedNormal, lightDirection);
        //return float4(undersideLighting, 1.0);
        finalColorRgb = (baseUnderwaterColor + undersideLighting);
    }

    // Final output
    float4 finalColor = float4(finalColorRgb,1.0f);
    clip((finalColor.a - 0.01f));

    if (c_debugInt == c_normals)
    {
        finalColor.rgb = EncodeXYZToRGB(perturbedNormal);
    }
    
    if (c_debugInt == c_emission)
    {
        finalColor.rgb = 0.f.xxx;
    }

    return finalColor;
}