#include "Data/Shaders/Utils/CommonUtils.hlsli"
#include "Data/Shaders/Utils/TextureUtils.hlsli"
#include "Data/Shaders/Utils/UnderwaterLightingUtils.hlsli"


struct vs_input_t
{
    float3 modelPosition : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
    float3 modelTangent : TANGENT;
    float3 modelBitangent : BITANGENT;
    float3 modelNormal : NORMAL;
};

struct v2p_t
{
    float4 clipPosition : SV_Position;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
    float3 worldTangent : TANGENT;
    float3 worldBitangent : BITANGENT;
    float3 worldNormal : NORMAL;
    float3 worldPosition : POSITION;
    float3 modelTangent : MODEL_TANGENT;
    float3 modelBitangent : MODEL_BITANGENT;
    float3 modelNormal : MODEL_NORMAL;
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

#define NUM_TEXTURE_SLOTS 16
#define MAX_NUM_BINDLESS_TEXTURES 4096

cbuffer TextureConstants : register(b5)
{
    uint4 textureIndexes[NUM_TEXTURE_SLOTS / 4];
};


#define NUM_BUFFER_SLOTS 16
#define MAX_NUM_BINDLESS_BUFFERS 4096
cbuffer BufferConstants : register(b6)
{
    uint4 bufferIndexes[NUM_BUFFER_SLOTS / 4];
}


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


cbuffer FishTypeSwimConstants : register(b9)
{
    float c_swimTailStart;
    float c_modelLength;
}

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



struct FishInstanceData
{
    float3 position;
    uint fishID;
    float3 forwardNormal;
    float swimAmplitude;
    float swimFrequency;
    float swimWavelength;
    float swimTailPower;
};




//------------------------------------------------------------------------------------------------
Texture2D textures[MAX_NUM_BINDLESS_TEXTURES] : register(t0, space0);
StructuredBuffer<FishInstanceData> instanceBuffers[MAX_NUM_BINDLESS_BUFFERS] : register(t0, space1);

//------------------------------------------------------------------------------------------------
SamplerState s_pointClampSampler : register(s0);
SamplerState s_bilinearWrapSampler : register(s1);


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
        float penumbra = SmoothStep3(saturate(RangeMap(dot(c_lights[i].c_spotForward, -pixelToLightDir),
                                                       c_lights[i].c_penumbraInnerDotThreshold,
                                                       c_lights[i].c_penumbraOuterDotThreshold, 1.f, 0.f)));
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

float GetTerrainDepthFraction(float worldHeight)
{
    float terrainDepthFraction = 1.0;
    float depthBelowSurface = c_seaLevel - worldHeight;
    terrainDepthFraction = RangeMapClamped(depthBelowSurface, 0.f, c_maxTerrainDepth, 0.f, 1.f);
    return terrainDepthFraction;
}

void BuildFishBasis(float3 forwardNormal, out float3 leftNormal, out float3 upNormal)
{
    float3 referenceUpNormal = float3(0.0f, 0.0f, 1.0f);

    if (abs(dot(forwardNormal, referenceUpNormal)) > 0.999f)
    {
        referenceUpNormal = float3(0.0f, 1.0f, 0.0f);
    }

    leftNormal = normalize(cross(referenceUpNormal, forwardNormal));
    upNormal = normalize(cross(forwardNormal, leftNormal));
}

float4x4 BuildModelToWorldTransform(FishInstanceData fishInstance)
{
    float3 forwardNormal = normalize(fishInstance.forwardNormal);

    float3 leftNormal;
    float3 upNormal;
    BuildFishBasis(forwardNormal, leftNormal, upNormal);

    return float4x4(
        forwardNormal.x, leftNormal.x, upNormal.x, fishInstance.position.x,
        forwardNormal.y, leftNormal.y, upNormal.y, fishInstance.position.y,
        forwardNormal.z, leftNormal.z, upNormal.z, fishInstance.position.z,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

uint GetFishBufferIndex(in uint bufferIndex)
{
    uint group = bufferIndex / 4;
    uint offset = bufferIndex % 4;
    uint index = bufferIndexes[group][offset];
    return index;
}

//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input, uint instanceId : SV_InstanceID)
{
    uint fishBufferIndex = GetFishBufferIndex(0);
    FishInstanceData fishInstance = instanceBuffers[fishBufferIndex][instanceId];
    
    
    float3 modelPosition = input.modelPosition;

    float modelLengthSafe = max(c_modelLength, 0.0001f);

    float normalizedHeadToTail01 = saturate(0.5f - (modelPosition.x / modelLengthSafe));

    // Tail influence: 0 before tailStart, then ramps to 1 toward the tail.
    float normalizedTailRegion01 = 0.0f;
    if (normalizedHeadToTail01 > c_swimTailStart)
    {
        float tailRegionDenominator = max((1.0f - c_swimTailStart), 0.0001f);
        normalizedTailRegion01 = ((normalizedHeadToTail01 - c_swimTailStart) / tailRegionDenominator);
    }

    float tailInfluence01 = pow(SmoothStep3(saturate(normalizedTailRegion01)), fishInstance.swimTailPower);

    // Traveling wave along the body.
    float wavelengthSafe = max(fishInstance.swimWavelength, 0.0001f);
    float waveNumber = (6.28318530718f / wavelengthSafe);
    float angularFrequency = (6.28318530718f * fishInstance.swimFrequency);

    // Use modelPosition.x for world-space-ish continuity along the mesh.
    // If you prefer phase to be anchored exactly head-to-tail regardless of scaling, you can use (normalizedHeadToTail01 * modelLengthSafe) instead.
    float time = c_time + (fishInstance.fishID * 0.3f);
    float wavePhaseRadians = (modelPosition.x * waveNumber) - (time * angularFrequency);

    float lateralDisplacement = (sin(wavePhaseRadians) * (fishInstance.swimAmplitude * tailInfluence01));

    // Assuming model Y is lateral (side-to-side).
    modelPosition.y = (modelPosition.y + lateralDisplacement);

    // Optional twist
    float twistAngleRadians = (sin(wavePhaseRadians + 1.57079632679f) * (0.35f * fishInstance.swimAmplitude) * tailInfluence01);
    float3 modelUpAxis = float3(0.0f, 0.0f, 1.0f);

    float3 modelNormal = RotateAroundAxis(input.modelNormal, modelUpAxis, twistAngleRadians);
    float3 modelTangent = RotateAroundAxis(input.modelTangent, modelUpAxis, twistAngleRadians);
    float3 modelBitangent = RotateAroundAxis(input.modelBitangent, modelUpAxis, twistAngleRadians);
    
    float4x4 modelToWorldTransform = BuildModelToWorldTransform(fishInstance);

    float4 worldPosition = mul(modelToWorldTransform, float4(modelPosition, 1.0f));
    float4 cameraPosition = mul(c_worldToCameraTransform, worldPosition);
    float4 renderPosition = mul(c_cameraToRenderTransform, cameraPosition);
    float4 clipPosition = mul(c_renderToClipTransform, renderPosition);

    float3 worldTangent = mul(modelToWorldTransform, float4(modelTangent, 0.0f)).xyz;
    float3 worldBitangent = mul(modelToWorldTransform, float4(modelBitangent, 0.0f)).xyz;
    float3 worldNormal = mul(modelToWorldTransform, float4(modelNormal, 0.0f)).xyz;

    v2p_t v2p;
    v2p.clipPosition = clipPosition;
    v2p.color = input.color;
    v2p.uv = input.uv;

    v2p.worldTangent = normalize(worldTangent);
    v2p.worldBitangent = normalize(worldBitangent);
    v2p.worldNormal = normalize(worldNormal);
    v2p.worldPosition = worldPosition.xyz;

    v2p.modelTangent = normalize(modelTangent);
    v2p.modelBitangent = normalize(modelBitangent);
    v2p.modelNormal = normalize(modelNormal);

    return v2p;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    float2 uvCoords = input.uv;

    uint diffuseTexIndex = 0;
    uint group;
    uint offset;
    GetTextureGroupAndOffset(diffuseTexIndex, group, offset);
    diffuseTexIndex = textureIndexes[group][offset];
    Texture2D diffuseTexture = textures[diffuseTexIndex];

    uint normalTexIndex = 1;
    GetTextureGroupAndOffset(normalTexIndex, group, offset);
    normalTexIndex = textureIndexes[group][offset];
    Texture2D normalTexture = textures[normalTexIndex];
    
    uint specTexIndex = 2;
    GetTextureGroupAndOffset(specTexIndex, group, offset);
    specTexIndex = textureIndexes[group][offset];
    Texture2D specTexture = textures[specTexIndex];
    
    uint aoTexIndex = 3;
    GetTextureGroupAndOffset(aoTexIndex, group, offset);
    aoTexIndex = textureIndexes[group][offset];
    Texture2D aoTexture = textures[aoTexIndex];

    float4 diffuseTexel = diffuseTexture.Sample(s_pointClampSampler, uvCoords);
    float4 normalTexel = normalTexture.Sample(s_bilinearWrapSampler, uvCoords);

    float4 ambientColor =  float4(c_ambientIntensity * float3(1.0f, 1.0f, 1.0f), 1.0f);
    float3 underwaterTintColor = GetUnderwaterTintRGB(input.worldPosition.xyz);
    float3 lightDirection = normalize(c_sunDirection);

    //decode normalTexel rgb into xyz abd re-normalize
    float3 pixelNormalTBNSpace = normalize(DecodeRGBtoXYZ(normalTexel.rgb));

    // Get TBN basis vectors
    float3 surfaceTangentWorldSpace		= input.worldTangent;
    float3 surfaceBitangentWorldSpace	= input.worldBitangent;
    float3 surfaceNormalWorldSpace		= input.worldNormal;

    float3 surfaceTangentModelSpace		= normalize( input.modelTangent);
    float3 surfaceBitangentModelSpace	= normalize( input.modelBitangent);
    float3 surfaceNormalModelSpace		= normalize( input.modelNormal);

    float3x3 tbnToWorld = Orthonormalize_IFwrd_JLeft_KUp( surfaceTangentWorldSpace, surfaceBitangentWorldSpace, surfaceNormalWorldSpace );
    float3 pixelNormalWorldSpace = mul( pixelNormalTBNSpace, tbnToWorld );

    float terrainDepthFraction = GetTerrainDepthFraction(input.worldPosition.z);
    float3 lightColor = GetLightRGB(input.worldPosition, pixelNormalWorldSpace, c_seaLevel, terrainDepthFraction);
    
    float3 color = diffuseTexel.rgb * lightColor * underwaterTintColor;
    float4 finalColor = float4(color, 1.0);
    clip(finalColor.a - 0.01f);
    
    if(c_debugInt == c_normals)
    {
        finalColor.rgb = EncodeXYZToRGB(pixelNormalWorldSpace);
    }
    

    if (c_debugInt == c_noAmbientOcclusion) // no ambient occlusion
    {
        finalColor.rgb = diffuseTexel.rgb * lightColor * underwaterTintColor;
    }

    if (c_debugInt == c_lightStrength) // light strength
    {
        finalColor.rgb = lightColor;
    }

    if (c_debugInt == c_albedo) //albedo
    {
        finalColor.rgb = diffuseTexel.rgb;
    }

    if (c_debugInt == c_specularAttenuation) // underwater Tint
    {
        finalColor.rgb = underwaterTintColor;
    }

    if (c_debugInt == c_noSpecularAttenuation) // no underwater Tint
    {
        finalColor.rgb = diffuseTexel.rgb * lightColor;
    }
    
    if (c_debugInt == c_emission)
    {
        finalColor.rgb = 0.f.xxx;
    }
    
    return finalColor;
    
}
