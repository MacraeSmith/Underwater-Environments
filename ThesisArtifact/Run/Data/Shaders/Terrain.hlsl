
#include "Data/Shaders/Utils/CommonUtils.hlsli"
#include "Data/Shaders/Utils/TextureUtils.hlsli"
#include "Data/Shaders/Utils/UnderwaterLightingUtils.hlsli"

//Vertex Input
struct vs_input_t
{
    float3 modelPosition : POSITION;
    float3 modelNormal : NORMAL;

    uint4 splatBiomeIndices : SPLAT_INDICES; // R8G8B8A8_UINT
    float4 splatWeights : SPLAT_WEIGHTS; // R8G8B8A8_UNORM gives 0..1

    uint vertexID : SV_VertexID;
};

//Vertex to Pixel
struct v2p_t
{
    float4 clipPosition : SV_Position;
    float3 worldNormal : NORMAL;
    float3 worldPosition : POSITION;

    float4 biomeWeights0123 : TEXCOORD0;
    float4 biomeWeights4567 : TEXCOORD1;
    float biomeWeight8 : TEXCOORD2;

    float seaLevel : SEA_LEVEL;
};

struct terrain_ps_output_t
{
    float4 color : SV_Target0;
    float4 mask : SV_Target1;
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

#define MAX_LIGHTS 64

//Constant Buffers
//------------------------------------------------------------------------------------------------
cbuffer PerFrameConstants : register(b1)
{
	float c_time;
	int c_debugInt;
	float c_debugFloat;
	float PADDING;	
}; //b1

cbuffer CameraConstants : register(b2)
{
	float4x4 c_worldToCameraTransform;	// View transform
	float4x4 c_cameraToRenderTransform;	// Non-standard transform from game to DirectX conventions
	float4x4 c_renderToClipTransform;		// Projection transform
	float3 c_cameraPosition;
	float CAMERA_PADDING;
}; //b2

cbuffer ModelConstants : register(b3)
{
	float4x4 c_modelToWorldTransform;		// Model transform
	float4 c_modelColor;
}; //b3

cbuffer LightConstants : register(b4)
{
	float3 c_sunDirection;
	float c_sunIntensity;
	float3 c_sunColorRGB;
	float c_ambientIntensity;
    int c_numLights;
	float3 LIGHT_CONSTANTS_PADDING;
	
    LightData c_lights[ MAX_LIGHTS];
}; //b4

#define NUM_BIOMES 9
#define NUM_BIOME_TEXTURES NUM_BIOMES * 2
#define PAD_FOR_HLSL_VEC4(x) (((x) + 3) / 4)
cbuffer TerrainTextureConstants : register(b5)
{
	uint4 c_albedoTextureIndexes[PAD_FOR_HLSL_VEC4(NUM_BIOME_TEXTURES)];
	uint4 c_normalTextureIndexes[PAD_FOR_HLSL_VEC4(NUM_BIOME_TEXTURES)];
	uint4 c_aoTextureIndexes[PAD_FOR_HLSL_VEC4(NUM_BIOME_TEXTURES)];
	float4 c_textureScales[PAD_FOR_HLSL_VEC4(NUM_BIOME_TEXTURES)];
    float4 c_textureNormalStrengths[PAD_FOR_HLSL_VEC4(NUM_BIOME_TEXTURES)];
}; //b5

#define DEFAULT_TEXTURE_SCALE  0.01
#define TEXTURE_RESOLUTION 2048

cbuffer CausticsConstants : register(b7)
{
    float c_causticsIntensity;
    float c_causticsDepthFadeRate;
    float c_causticsWarpSpeed;
    float c_causticsWarpStrength;
    float c_causticsLineThickness;
    float c_causticsRippleSpeed;
    float c_causticsRippleStrength;
}; //b7

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

#define WALL_THRESHOLD 0.8

//Texture and Sampler constants
//------------------------------------------------------------------------------------------------
#define MAX_NUM_BINDLESS_TEXTURES 4096
Texture2D<float4> t_textures[MAX_NUM_BINDLESS_TEXTURES] : register(t0, space0);
SamplerState s_sampler : register(s0);
SamplerState s_bilinearWrapSampler : register(s1);



//Water Surface
//------------------------------------------------------------------------------------------------
float GetWaterHeight(float2 basePositionXY)
{
    float totalHeight = 0.0f;

    float directionRegionScale = lerp(0.0025f, 0.00005f, c_directionVarianceRegionScale);
    float2 directionRegionSamplePosition = (basePositionXY * directionRegionScale);

    float amplitudeRegionScale = lerp(0.0025f, 0.00005f, c_amplitudeVarianceRegionScale);
    float2 amplitudeRegionSamplePosition = (basePositionXY * amplitudeRegionScale);

    float amplitudeNoise = snoise((amplitudeRegionSamplePosition + float2(17.13f, 9.37f)));
    amplitudeNoise = ((amplitudeNoise * 0.5f) + 0.5f);
    amplitudeNoise = smoothstep(0.6f, 0.9f, amplitudeNoise);

    float amplitudeMultiplier = (1.0f + ((amplitudeNoise * c_waveAmplitudeVariance) * 50.0f));

    for (int waveIndex = 0; waveIndex < c_numWaves; ++waveIndex)
    {
        float4 waveData = c_waveData[waveIndex];
        float4 waveDataExtra = c_waveDataExtra[waveIndex];

        float2 baseWaveDirection = normalize(waveData.xy);
        float baseAmplitude = (waveData.z * 0.1f * c_globalAmplitudeScale);
        float waveLength = (waveData.w * 0.1f * c_globalWavelengthScale);

        float waveSpeed = waveDataExtra.x;
        float waveOffset = waveDataExtra.z;

        float2 perWaveSampleOffset = float2(((float) waveIndex * 12.13f), ((float) waveIndex * 9.37f));
        float regionNoiseDirection = snoise((directionRegionSamplePosition + perWaveSampleOffset));

        float directionOffsetRadians = (regionNoiseDirection * c_waveDirectionVariance);
        float2 waveDirection = normalize(RotateVector2D(baseWaveDirection, directionOffsetRadians));

        float localAmplitude = ((baseAmplitude * amplitudeMultiplier));

        float waveNumber = ((2.0f * PI) / waveLength);
        float angularFrequency = (0.2f * sqrt((9.81f * waveNumber)));
        float waveTime = ((c_time * c_globalSpeedScale) * waveSpeed);

        float phasePosition = dot(waveDirection, basePositionXY);
        float wavePhase = ((((phasePosition * waveNumber) - (angularFrequency * waveTime)) + waveOffset));

        float sinPhase = sin(wavePhase);

        totalHeight += (localAmplitude * sinPhase);
    }

    return totalHeight;
}

//Triplanar Texturing Helpers
//------------------------------------------------------------------------------------------------

void AddSparseBiomeWeight(inout float4 biomeWeights0123, inout float4 biomeWeights4567, inout float biomeWeight8, uint biomeIndex, float biomeWeight)
{
    if (biomeIndex == 0)
    {
        biomeWeights0123.x += biomeWeight;
    }
    else if (biomeIndex == 1)
    {
        biomeWeights0123.y += biomeWeight;
    }
    else if (biomeIndex == 2)
    {
        biomeWeights0123.z += biomeWeight;
    }
    else if (biomeIndex == 3)
    {
        biomeWeights0123.w += biomeWeight;
    }
    else if (biomeIndex == 4)
    {
        biomeWeights4567.x += biomeWeight;
    }
    else if (biomeIndex == 5)
    {
        biomeWeights4567.y += biomeWeight;
    }
    else if (biomeIndex == 6)
    {
        biomeWeights4567.z += biomeWeight;
    }
    else if (biomeIndex == 7)
    {
        biomeWeights4567.w += biomeWeight;
    }
    else if (biomeIndex == 8)
    {
        biomeWeight8 += biomeWeight;
    }
}

void ExpandSparseBiomeWeightsToDense(out float4 outBiomeWeights0123, out float4 outBiomeWeights4567, out float outBiomeWeight8, uint4 splatBiomeIndices, float4 splatWeights)
{
    outBiomeWeights0123 = float4(0.0f, 0.0f, 0.0f, 0.0f);
    outBiomeWeights4567 = float4(0.0f, 0.0f, 0.0f, 0.0f);
    outBiomeWeight8 = 0.0f;

    AddSparseBiomeWeight(outBiomeWeights0123, outBiomeWeights4567, outBiomeWeight8, splatBiomeIndices.x, splatWeights.x);
    AddSparseBiomeWeight(outBiomeWeights0123, outBiomeWeights4567, outBiomeWeight8, splatBiomeIndices.y, splatWeights.y);
    AddSparseBiomeWeight(outBiomeWeights0123, outBiomeWeights4567, outBiomeWeight8, splatBiomeIndices.z, splatWeights.z);
    AddSparseBiomeWeight(outBiomeWeights0123, outBiomeWeights4567, outBiomeWeight8, splatBiomeIndices.w, splatWeights.w);
}

void NormalizeDenseBiomeWeights(inout float4 biomeWeights0123, inout float4 biomeWeights4567, inout float biomeWeight8)
{
    biomeWeights0123 = max(biomeWeights0123, float4(0.0f, 0.0f, 0.0f, 0.0f));
    biomeWeights4567 = max(biomeWeights4567, float4(0.0f, 0.0f, 0.0f, 0.0f));
    biomeWeight8 = max(biomeWeight8, 0.0f);

    float weightSum = biomeWeights0123.x + biomeWeights0123.y + biomeWeights0123.z + biomeWeights0123.w;
    weightSum += biomeWeights4567.x + biomeWeights4567.y + biomeWeights4567.z + biomeWeights4567.w;
    weightSum += biomeWeight8;

    float inverseSum = (weightSum > 0.0f) ? (1.0f / weightSum) : 1.0f;
    biomeWeights0123 *= inverseSum;
    biomeWeights4567 *= inverseSum;
    biomeWeight8 *= inverseSum;
}

void SelectTop4DenseBiomeWeights(out uint4 outTopBiomeIndices, out float4 outTopBiomeWeights, float4 biomeWeights0123, float4 biomeWeights4567, float biomeWeight8)
{
    float denseBiomeWeights[NUM_BIOMES];
    denseBiomeWeights[0] = biomeWeights0123.x;
    denseBiomeWeights[1] = biomeWeights0123.y;
    denseBiomeWeights[2] = biomeWeights0123.z;
    denseBiomeWeights[3] = biomeWeights0123.w;
    denseBiomeWeights[4] = biomeWeights4567.x;
    denseBiomeWeights[5] = biomeWeights4567.y;
    denseBiomeWeights[6] = biomeWeights4567.z;
    denseBiomeWeights[7] = biomeWeights4567.w;
    denseBiomeWeights[8] = biomeWeight8;

    uint topBiomeIndices[4] = { 0, 0, 0, 0 };
    float topBiomeWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    for (uint biomeIndex = 0; biomeIndex < NUM_BIOMES; ++biomeIndex)
    {
        float biomeWeight = denseBiomeWeights[biomeIndex];

        if (biomeWeight > topBiomeWeights[0])
        {
            topBiomeWeights[3] = topBiomeWeights[2];
            topBiomeIndices[3] = topBiomeIndices[2];
            topBiomeWeights[2] = topBiomeWeights[1];
            topBiomeIndices[2] = topBiomeIndices[1];
            topBiomeWeights[1] = topBiomeWeights[0];
            topBiomeIndices[1] = topBiomeIndices[0];
            topBiomeWeights[0] = biomeWeight;
            topBiomeIndices[0] = biomeIndex;
        }
        else if (biomeWeight > topBiomeWeights[1])
        {
            topBiomeWeights[3] = topBiomeWeights[2];
            topBiomeIndices[3] = topBiomeIndices[2];
            topBiomeWeights[2] = topBiomeWeights[1];
            topBiomeIndices[2] = topBiomeIndices[1];
            topBiomeWeights[1] = biomeWeight;
            topBiomeIndices[1] = biomeIndex;
        }
        else if (biomeWeight > topBiomeWeights[2])
        {
            topBiomeWeights[3] = topBiomeWeights[2];
            topBiomeIndices[3] = topBiomeIndices[2];
            topBiomeWeights[2] = biomeWeight;
            topBiomeIndices[2] = biomeIndex;
        }
        else if (biomeWeight > topBiomeWeights[3])
        {
            topBiomeWeights[3] = biomeWeight;
            topBiomeIndices[3] = biomeIndex;
        }
    }

    float topWeightSum = topBiomeWeights[0] + topBiomeWeights[1] + topBiomeWeights[2] + topBiomeWeights[3];
    float inverseTopWeightSum = (topWeightSum > 0.0f) ? (1.0f / topWeightSum) : 1.0f;

    outTopBiomeIndices = uint4(topBiomeIndices[0], topBiomeIndices[1], topBiomeIndices[2], topBiomeIndices[3]);
    outTopBiomeWeights = float4(topBiomeWeights[0], topBiomeWeights[1], topBiomeWeights[2], topBiomeWeights[3]) * inverseTopWeightSum;
}

uint GetDominantBiomeIndexFromDenseWeights(float4 biomeWeights0123, float4 biomeWeights4567, float biomeWeight8)
{
    uint dominantBiomeIndex = 0;
    float dominantBiomeWeight = biomeWeights0123.x;

    if (biomeWeights0123.y > dominantBiomeWeight)
    {
        dominantBiomeWeight = biomeWeights0123.y;
        dominantBiomeIndex = 1;
    }
    if (biomeWeights0123.z > dominantBiomeWeight)
    {
        dominantBiomeWeight = biomeWeights0123.z;
        dominantBiomeIndex = 2;
    }
    if (biomeWeights0123.w > dominantBiomeWeight)
    {
        dominantBiomeWeight = biomeWeights0123.w;
        dominantBiomeIndex = 3;
    }
    if (biomeWeights4567.x > dominantBiomeWeight)
    {
        dominantBiomeWeight = biomeWeights4567.x;
        dominantBiomeIndex = 4;
    }
    if (biomeWeights4567.y > dominantBiomeWeight)
    {
        dominantBiomeWeight = biomeWeights4567.y;
        dominantBiomeIndex = 5;
    }
    if (biomeWeights4567.z > dominantBiomeWeight)
    {
        dominantBiomeWeight = biomeWeights4567.z;
        dominantBiomeIndex = 6;
    }
    if (biomeWeights4567.w > dominantBiomeWeight)
    {
        dominantBiomeWeight = biomeWeights4567.w;
        dominantBiomeIndex = 7;
    }
    if (biomeWeight8 > dominantBiomeWeight)
    {
        dominantBiomeIndex = 8;
    }

    return dominantBiomeIndex;
}

void GetAlbedo_Ao_Rough_Normal_PerturbedN(out float3 out_Albedo, out float3 out_Ao, out float3 out_Normal, out float3 out_PerturbedNormal, in float3 worldPos, in float3 worldNormal, in uint biomeIndex, in float dotUp, in float mipLevel)
{
    float normalStrength = 1.0f;

    uint biomeGroup = 0;
    uint biomeOffset = 0;
    GetTextureGroupAndOffset(biomeIndex, biomeGroup, biomeOffset);

    float textureScale = c_textureScales[biomeGroup][biomeOffset];
    uint albedoTextureIndex1 = c_albedoTextureIndexes[biomeGroup][biomeOffset];
    uint aoTextureIndex1 = c_aoTextureIndexes[biomeGroup][biomeOffset];
    uint normalTextureIndex1 = c_normalTextureIndexes[biomeGroup][biomeOffset];

    float3 albedo1 = TriplanarSampleColorMipLevel(worldPos, worldNormal, textureScale, t_textures[albedoTextureIndex1], s_bilinearWrapSampler, mipLevel);
    float3 ao1 = TriplanarSampleColorMipLevel(worldPos, worldNormal, textureScale, t_textures[aoTextureIndex1], s_bilinearWrapSampler, mipLevel);
    float3 normal1 = TriplanarSampleNormalMipLevel(worldPos, worldNormal, textureScale, t_textures[normalTextureIndex1], s_bilinearWrapSampler, mipLevel);
    float normalStrength1 = c_textureNormalStrengths[biomeGroup][biomeOffset];

    uint wallsBiomeIndex = (biomeIndex + NUM_BIOMES);
    uint wallsIndexGroup = 0;
    uint wallsIndexOffset = 0;
    GetTextureGroupAndOffset(wallsBiomeIndex, wallsIndexGroup, wallsIndexOffset);

    textureScale = c_textureScales[wallsIndexGroup][wallsIndexOffset];
    uint albedoTextureIndex2 = c_albedoTextureIndexes[wallsIndexGroup][wallsIndexOffset];
    uint aoTextureIndex2 = c_aoTextureIndexes[wallsIndexGroup][wallsIndexOffset];
    uint normalTextureIndex2 = c_normalTextureIndexes[wallsIndexGroup][wallsIndexOffset];

    float3 albedo2 = TriplanarSampleColorMipLevel(worldPos, worldNormal, textureScale, t_textures[albedoTextureIndex2], s_bilinearWrapSampler, mipLevel);
    float3 ao2 = TriplanarSampleColorMipLevel(worldPos, worldNormal, textureScale, t_textures[aoTextureIndex2], s_bilinearWrapSampler, mipLevel);
    float3 normal2 = TriplanarSampleNormalMipLevel(worldPos, worldNormal, textureScale, t_textures[normalTextureIndex2], s_bilinearWrapSampler, mipLevel);
    float normalStrength2 = c_textureNormalStrengths[wallsIndexGroup][wallsIndexOffset];

    float fraction = clamp(GetFractionWithinRange(dotUp, 1.0f, WALL_THRESHOLD), 0.0f, 1.0f);
    out_Albedo = lerp(albedo1, albedo2, fraction);
    out_Ao = lerp(ao1, ao2, fraction);
    out_Normal = lerp(normal1, normal2, fraction);

    normalStrength = lerp(normalStrength1, normalStrength2, fraction);
    float3 deviation = (out_Normal - worldNormal);
    out_PerturbedNormal = normalize(worldNormal + (deviation * normalStrength));
}

void SampleBiomeMaterial(out float3 out_Albedo, out float3 out_Ao, out float3 out_Normal, out float3 out_PerturbedNormal, float3 worldPos, float3 worldNormal, uint biomeIndex, float dotUp, float mipLevel)
{
    GetAlbedo_Ao_Rough_Normal_PerturbedN(out_Albedo, out_Ao, out_Normal, out_PerturbedNormal, worldPos, worldNormal, biomeIndex, dotUp, mipLevel);
}

float ComputeTriplanarMipLevel(float3 worldPos, float scale, float textureResolution)
{
    float2 uvXddx = (ddx(worldPos.yz) * scale);
    float2 uvXddy = (ddy(worldPos.yz) * scale);

    float2 uvYddx = (ddx(worldPos.xz) * scale);
    float2 uvYddy = (ddy(worldPos.xz) * scale);

    float2 uvZddx = (ddx(worldPos.xy) * scale);
    float2 uvZddy = (ddy(worldPos.xy) * scale);

    float rhoX = max(length(uvXddx), length(uvXddy)) * textureResolution;
    float rhoY = max(length(uvYddx), length(uvYddy)) * textureResolution;
    float rhoZ = max(length(uvZddx), length(uvZddy)) * textureResolution;

    float rho = max(rhoX, max(rhoY, rhoZ));
    rho = max(rho, 1.0f);

    return log2(rho);
}


//Caustics Helpers
//------------------------------------------------------------------------------------------------

float CausticPattern(float3 worldPos, float3 surfaceNormal, float baseScale, float seaLevel)
{
	const float STRETCH_FACTOR        = 0.02f;
	const float INNER_RIPPLE_FREQ     = 5.0f;

	//------------------------------------------------------------
	// Build a light-space projection basis from sun direction
	//------------------------------------------------------------
	float3 L = normalize(c_sunDirection);
	float3 up = float3(0, 0, 1);
	float3 T = normalize(cross(up, L));
	float3 B = normalize(cross(L, T));

	//------------------------------------------------------------
	// Project world position into light-space
	//------------------------------------------------------------
	float3 lightSpace;
	lightSpace.x = dot(worldPos, T);
	lightSpace.y = dot(worldPos, B);
	lightSpace.z = dot(worldPos, L);

	//------------------------------------------------------------
	// Compute depth along light direction
	//------------------------------------------------------------
	float depth = max(0.0f, seaLevel - worldPos.z);

	// Optional: small stretch with depth for subtle variation
	float zStretch = 1.0f + pow(depth * STRETCH_FACTOR, 0.8f);
	lightSpace.z /= zStretch;

	//------------------------------------------------------------
	// Use XY plane for stable UVs (projected from light direction)
	//------------------------------------------------------------
	float2 uv = lightSpace.xy * baseScale;

	//------------------------------------------------------------
	// Animate and distort
	//------------------------------------------------------------
	float t = c_time * c_causticsWarpSpeed;

	float2 warp1 = Noise2D(uv * 1.2f + float2(t * 0.9f, t * 0.7f));
	float2 warp2 = Noise2D(uv * 2.1f - float2(t * 0.6f, t * 0.8f));
	float2 warp3 = Noise2D(uv * 3.5f + float2(-t * 1.3f, t * 1.1f));
	float2 warp = (warp1 + warp2 * 0.6f + warp3 * 0.3f) * c_causticsWarpStrength;
	uv += warp;

	//------------------------------------------------------------
	// Caustic pattern core
	//------------------------------------------------------------
	float e = VoronoiEdgeSoft(uv * 0.25f, c_causticsLineThickness);
	float baseCaustic = pow(e, 1.4f);

	//------------------------------------------------------------
	// Local brightness modulation (patchy shimmer)
	//------------------------------------------------------------
	float2 modUV = (uv * 0.15f) + float2(t * 0.05f, -t * 0.03f);
	float2 modSample = Noise2D(modUV);
	float localFluct = 0.5f + 0.5f *
		sin(dot(modSample, float2(1.2f, 0.8f)) * 6.2831f + t * 0.7f);
	float localMod = lerp(0.85f, 1.25f, localFluct);

	//------------------------------------------------------------
	// Inner ripple shimmer
	//------------------------------------------------------------
	float rippleT = c_time * c_causticsRippleSpeed;
	float ripplePhase =
		sin((uv.x + uv.y) * INNER_RIPPLE_FREQ + rippleT) * 0.5f + 0.5f;
	float rippleMask = saturate(baseCaustic * 3.0f);
	float ripple = lerp(1.0f - c_causticsRippleStrength,
						1.0f + c_causticsRippleStrength, ripplePhase);
	float shimmeredCaustic =
		baseCaustic * (1.0f + (ripple - 1.0f) * rippleMask);

	float c = shimmeredCaustic * localMod;

	//------------------------------------------------------------
	// Depth fade, wall fade, and light angle weighting
	//------------------------------------------------------------
	float depthFade = exp(-depth * c_causticsDepthFadeRate * 0.01);
	float facing = saturate(dot(normalize(surfaceNormal), -L)); // 1 when facing light

	// Combine all factors
	c *= depthFade * facing;
	float result = c;


	float shallowStart = 5.f;
	float shallowEnd = 0.f;
	float shallowFade = saturate((depth - shallowEnd) / (shallowStart - shallowEnd));
	shallowFade = SmoothStep3(shallowFade);

    return result * shallowFade * c_causticsIntensity;
}


//Light and Color
//------------------------------------------------------------------------------------------------
float3 GetUnderwaterTintRGB(float3 worldPos)
{
	float depth = max(0.0, c_seaLevel - worldPos.z);
	float depthScale = c_underwaterTintDepthScale;
	float shallowDepthMax = 10.f * depthScale;
	float shallow = saturate(1.0 - depth / shallowDepthMax);
    float3 shallowColor = float3(0.0, 0.3, 0.5);
    float3 deepColor = float3(0.02, 0.1, 0.2);
    float3 scatterColor = lerp(deepColor, shallowColor, shallow);

    float3 absorption = float3(0.03, 0.01, 0.007); // how much color channels are absorbed
	
	//point / spot lights
	float3 distanceAttenuation = float3(0.0,0.0,0.0);
	for(int i = 0; i < c_numLights; ++i)
	{
		float3 lightPos = c_lights[i].c_position;
		float3 lightToPixel = worldPos - lightPos;
		float distance = length(lightToPixel);
		float3 lightAttenuation = exp(-absorption * distance * depthScale);
		distanceAttenuation = max(distanceAttenuation, lightAttenuation);
	}
	
	//for directional Light (only worry about depth below the surface)
	float depthBelowSurface =  c_seaLevel - worldPos.z;
	float3 depthAttenuation = exp(-absorption * depthBelowSurface * c_underwaterTintDepthScale);
	distanceAttenuation = max(distanceAttenuation, depthAttenuation);
	return min(scatterColor + distanceAttenuation, float3(1.0,1.0,1.0));
}


float3 GetLightRGB(float3 worldPos, float3 worldNormal, float seaLevel, float terrainDepthFraction)
{
	// --- Caustics ---
    float depth = seaLevel - worldPos.z;
    float wShallow = 1.0 - smoothstep(15.0, 45.0, depth);
    float wMid = smoothstep(15.0, 45.0, depth) * (1.0 - smoothstep(120.0, 180.0, depth));
    float wDeep = smoothstep(120.0, 180.0, depth);
    float sumW = wShallow + wMid + wDeep + 1e-6f;
    wShallow /= sumW;
    wMid /= sumW;
    wDeep /= sumW;
	
	float causticShallow = CausticPattern(worldPos, worldNormal, 0.075, seaLevel) * wShallow;
    float causticMid = CausticPattern(worldPos, worldNormal, 0.045, seaLevel) * wMid;
    float causticDeep = CausticPattern(worldPos, worldNormal, 0.015, seaLevel) * wDeep;
	
    float caustic = causticShallow + causticMid + causticDeep;

	//--- Sunlight ---
    float3 lightDir = normalize(c_sunDirection.xyz);
    float nDotL = dot(-lightDir, worldNormal);
    float directionalLightStrength = RangeMap(nDotL, -c_ambientIntensity, 1.0, 0.0, 1.0);
    float causticLightStrength =  c_sunIntensity * saturate(directionalLightStrength);
    float3 directionalColor = c_sunIntensity * saturate(directionalLightStrength) * saturate(1.0 - (terrainDepthFraction * c_underwaterDepthDarkenScale)); // reduce light strength by depth;
    float3 combinedLightColor = directionalColor;

	//--- Point Lights ---
	float pointLightIntensity = saturate(terrainDepthFraction * 3.0); // decrease light strength by shallow

    float totalPointLightStrength = 0.0f;
    for (int i = 0; i < c_numLights; ++i)
    {
        float3 pixelToLightDisp = c_lights[i].c_position - worldPos;
        float distance = length(pixelToLightDisp);
        float falloff = SmoothStep3(saturate(RangeMap(distance, c_lights[i].c_innerRadius, c_lights[i].c_outerRadius, 1.f, 0.f)));
        float3 pixelToLightDir = normalize(pixelToLightDisp);
        float lightDot = dot(worldNormal, pixelToLightDir);
        float penumbra = SmoothStep3(saturate(RangeMap(dot(c_lights[i].c_spotForward, -pixelToLightDir),c_lights[i].c_penumbraInnerDotThreshold,c_lights[i].c_penumbraOuterDotThreshold, 1.f, 0.f)));
        float lightStrength = penumbra * falloff * saturate(RangeMap(lightDot, -c_ambientIntensity, 1.f, 0.f, 1.f)) * c_lights[i].c_color.a;
        totalPointLightStrength += lightStrength;
        combinedLightColor += c_lights[i].c_color.rgb * lightStrength * pointLightIntensity;
    }

	// --- fade caustics near artificial lights ---
    float localLightStrength = totalPointLightStrength;
    float causticInfluence = saturate(causticLightStrength / (causticLightStrength + localLightStrength)); // * 3.0f));

	// apply caustic only where sun dominates
    combinedLightColor += directionalColor * ((1.0 + caustic * 1.5f) - 1.0) * causticInfluence;

    return combinedLightColor;
}

//===============================================================================================================================
//===============================================================================================================================

//Vertex shader
//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
    float4 modelPosition = float4(input.modelPosition, 1.0f);
    float4 worldPosition = mul(c_modelToWorldTransform, modelPosition);
    float seaLevel = c_seaLevel + GetWaterHeight(worldPosition.xy);

    float4 modelNormal = float4(input.modelNormal, 0.0f);
    float4 worldNormal = mul(c_modelToWorldTransform, modelNormal);

    float4 cameraPosition = mul(c_worldToCameraTransform, worldPosition);
    float4 renderPosition = mul(c_cameraToRenderTransform, cameraPosition);
    float4 clipPosition = mul(c_renderToClipTransform, renderPosition);

    v2p_t output;
    output.clipPosition = clipPosition;
    output.worldPosition = worldPosition.xyz;
    output.worldNormal = worldNormal.xyz;
    ExpandSparseBiomeWeightsToDense(output.biomeWeights0123, output.biomeWeights4567, output.biomeWeight8, input.splatBiomeIndices, input.splatWeights);
    output.seaLevel = seaLevel;

    return output;
}

//Pixel Shader
//------------------------------------------------------------------------------------------------
terrain_ps_output_t PixelMain(v2p_t input)
{
    terrain_ps_output_t output;
    output.mask = float4(0, 0, 0, 1);

    float4 denseBiomeWeights0123 = input.biomeWeights0123;
    float4 denseBiomeWeights4567 = input.biomeWeights4567;
    float denseBiomeWeight8 = input.biomeWeight8;
    NormalizeDenseBiomeWeights(denseBiomeWeights0123, denseBiomeWeights4567, denseBiomeWeight8);

    uint4 splatIndices;
    float4 splatWeights;
    SelectTop4DenseBiomeWeights(splatIndices, splatWeights, denseBiomeWeights0123, denseBiomeWeights4567, denseBiomeWeight8);

    if (c_debugInt == 19)
    {
        float weightDifference = abs((splatWeights.x - splatWeights.y));
        output.color = float4(weightDifference.xxx, 1.0f);
        return output;
    }

    float dotUp = dot(input.worldNormal, float3(0.0f, 0.0f, 1.0f));
    float3 sampleWorldPos = input.worldPosition;

    float pixelMipLevel = ComputeTriplanarMipLevel(input.worldPosition, DEFAULT_TEXTURE_SCALE, TEXTURE_RESOLUTION);

    float3 albedoAccum = float3(0.0f, 0.0f, 0.0f);
    float3 aoAccum = float3(0.0f, 0.0f, 0.0f);
    float3 normalAccum = float3(0.0f, 0.0f, 0.0f);
    float3 perturbedNormalAccum = float3(0.0f, 0.0f, 0.0f);

    if (c_debugInt == c_strongestBiome)
    {
        uint strongestBiome = GetDominantBiomeIndexFromDenseWeights(denseBiomeWeights0123, denseBiomeWeights4567, denseBiomeWeight8);
        float3 albedoStrongest;
        float3 aoStrongest;
        float3 normalStrongest;
        float3 perturbedStrongest;
        SampleBiomeMaterial(albedoStrongest, aoStrongest, normalStrongest, perturbedStrongest, sampleWorldPos, input.worldNormal, strongestBiome, dotUp, pixelMipLevel);
        output.color = float4(albedoStrongest, 1.0f);
        return output;
    }

    if (splatWeights.x > 0.0f)
    {
        float3 albedo0;
        float3 ao0;
        float3 normal0;
        float3 perturbed0;
        SampleBiomeMaterial(albedo0, ao0, normal0, perturbed0, sampleWorldPos, input.worldNormal, splatIndices.x, dotUp, pixelMipLevel);

        if (c_debugInt == c_biomeWeight1)
        {
            output.color = float4(albedo0, 1.0f);
            return output;
        }

        albedoAccum += (albedo0 * splatWeights.x);
        aoAccum += (ao0 * splatWeights.x);
        normalAccum += (normal0 * splatWeights.x);
        perturbedNormalAccum += (perturbed0 * splatWeights.x);
    }

    if (splatWeights.y > 0.0f)
    {
        float3 albedo1;
        float3 ao1;
        float3 normal1;
        float3 perturbed1;
        SampleBiomeMaterial(albedo1, ao1, normal1, perturbed1, sampleWorldPos, input.worldNormal, splatIndices.y, dotUp, pixelMipLevel);

        if (c_debugInt == c_biomeWeight2)
        {
            output.color = float4(albedo1, 1.0f);
            return output;
        }

        albedoAccum += (albedo1 * splatWeights.y);
        aoAccum += (ao1 * splatWeights.y);
        normalAccum += (normal1 * splatWeights.y);
        perturbedNormalAccum += (perturbed1 * splatWeights.y);
    }

    if (splatWeights.z > 0.0f)
    {
        float3 albedo2;
        float3 ao2;
        float3 normal2;
        float3 perturbed2;
        SampleBiomeMaterial(albedo2, ao2, normal2, perturbed2, sampleWorldPos, input.worldNormal, splatIndices.z, dotUp, pixelMipLevel);

        if (c_debugInt == c_biomeWeight3)
        {
            output.color = float4(albedo2, 1.0f);
            return output;
        }

        albedoAccum += (albedo2 * splatWeights.z);
        aoAccum += (ao2 * splatWeights.z);
        normalAccum += (normal2 * splatWeights.z);
        perturbedNormalAccum += (perturbed2 * splatWeights.z);
    }

    if (splatWeights.w > 0.0f)
    {
        float3 albedo3;
        float3 ao3;
        float3 normal3;
        float3 perturbed3;
        SampleBiomeMaterial(albedo3, ao3, normal3, perturbed3, sampleWorldPos, input.worldNormal, splatIndices.w, dotUp, pixelMipLevel);

        if (c_debugInt == c_biomeWeight4)
        {
            output.color = float4(albedo3, 1.0f);
            return output;
        }

        albedoAccum += (albedo3 * splatWeights.w);
        aoAccum += (ao3 * splatWeights.w);
        normalAccum += (normal3 * splatWeights.w);
        perturbedNormalAccum += (perturbed3 * splatWeights.w);
    }

    float3 albedoRGB = albedoAccum;
    float3 aoRGB = aoAccum;
    float3 normalMapNormal = normalize(normalAccum);
    float3 perturbedNormal = normalize(perturbedNormalAccum);

    float3 normal = perturbedNormal;
    if (c_debugInt == c_noNormals)
    {
        normal = input.worldNormal;
    }

    float3 underwaterTint = GetUnderwaterTintRGB(input.worldPosition);

    float terrainDepthFraction = 1.0f;
    float depthBelowSurface = (input.seaLevel - input.worldPosition.z);
    terrainDepthFraction = RangeMapClamped(depthBelowSurface, 0.0f, c_maxTerrainDepth, 0.0f, 1.0f);

    output.mask.r = terrainDepthFraction;

    float3 lightColor = GetLightRGB(input.worldPosition, normal, input.seaLevel, terrainDepthFraction);
    float3 color = (albedoRGB * lightColor * underwaterTint * aoRGB);
    float4 finalColor = float4(color, 1.0f);

    if (c_debugInt == c_normals)
    {
        finalColor.rgb = EncodeXYZToRGB(perturbedNormal);
    }

    if (c_debugInt == c_ambientOcclusion)
    {
        finalColor.rgb = aoRGB;
    }

    if (c_debugInt == c_noAmbientOcclusion)
    {
        finalColor.rgb = (albedoRGB * lightColor * underwaterTint);
    }

    if (c_debugInt == c_lightStrength)
    {
        finalColor.rgb = lightColor;
    }

    if (c_debugInt == c_albedo)
    {
        finalColor.rgb = albedoRGB;
    }

    if (c_debugInt == c_specularAttenuation)
    {
        finalColor.rgb = underwaterTint;
    }

    if (c_debugInt == c_noSpecularAttenuation)
    {
        finalColor.rgb = (albedoRGB * lightColor * aoRGB);
    }

    if (c_debugInt == c_biomeWeight1 || c_debugInt == c_biomeWeight2 || c_debugInt == c_biomeWeight3 || c_debugInt == c_biomeWeight4)
    {
        finalColor.rgb = float3(1.0f, 1.0f, 1.0f);
    }

    if (c_debugInt == c_emission)
    {
        finalColor.rgb = 0.0f.xxx;
    }

    clip((finalColor.a - 0.01f));
    output.color = finalColor;

    return output;
}