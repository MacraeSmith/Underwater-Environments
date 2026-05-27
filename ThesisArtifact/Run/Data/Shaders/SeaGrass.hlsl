#include "Data/Shaders/Utils/CommonUtils.hlsli"
#include "Data/Shaders/Utils/VegetationUtils.hlsli"
#include "Data/Shaders/Utils/TextureUtils.hlsli"
#include "Data/Shaders/Utils/UnderwaterLightingUtils.hlsli"


// ------------------------------------------------------------
// Instance output from compute, consumed by graphics
// ------------------------------------------------------------
struct VegetationInstance
{
    float3 position;
    float yaw;
    float3 terrainNormal;
    uint seed;
    float heightMultiplier;
};

RWStructuredBuffer<VegetationInstance> OutInstances : register(u0);

// ------------------------------------------------------------
// Graphics shaders
// Render your grass blade triangle mesh instanced vertexCount times.
// Your blade mesh vertices are Vertex_PCU (POSITION/COLOR/TEXCOORD).
// ------------------------------------------------------------
struct vs_input_t
{
    float3 modelPosition : POSITION;
    float4 color         : COLOR;     // typical input layout expands UNORM8 to float4
    float2 uv            : TEXCOORD;
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
    float widthFraction : WIDTH_FRACTION;
    float3 curvedNormal : CURVED_NORMAL;
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
    float4x4 c_worldToCameraTransform;
    float4x4 c_cameraToRenderTransform;
    float4x4 c_renderToClipTransform;
    float3 c_cameraPosition;
    float pad1;
};


#define NUM_BUFFER_SLOTS 16
cbuffer BufferConstants : register(b6)
{
    uint4 bufferIndexes[NUM_BUFFER_SLOTS / 4];
}

#define MAX_NUM_BINDLESS_BUFFERS 4096

cbuffer LightConstants : register(b4)
{
    float3 c_sunDirection;
    float c_sunIntensity;
    float3 c_sunColorRGB;
    float c_ambientIntensity;
    int c_numLights;
    float3 LIGHT_CONSTANTS_PADDING;
	
    LightData c_lights[MAX_LIGHTS];
}; //b4

#define NUM_TEXTURE_SLOTS 16
#define MAX_NUM_BINDLESS_TEXTURES 4096

cbuffer TextureConstants : register(b5)
{
    uint4 textureIndexes[NUM_TEXTURE_SLOTS / 4];
};


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

cbuffer VegetationConstants : register(b11)
{
    float c_vegetationHeightMin;
    float c_vegetationHeightMax;
    float c_vegetationWidthMin;
    float c_vegetationWidthMax;
    float c_rigidity;
}

cbuffer WindConstants : register(b12)
{
    WindInfo c_windInfo;
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


Texture2D textures[MAX_NUM_BINDLESS_TEXTURES] : register(t0, space0);
StructuredBuffer<VegetationInstance> instanceBuffers[MAX_NUM_BINDLESS_BUFFERS] : register(t0, space1);

SamplerState s_pointClampSampler : register(s0);

//Caustics Helpers
//------------------------------------------------------------------------------------------------
float CausticPattern(float3 worldPos, float3 surfaceNormal, float baseScale, float seaLevel)
{
    const float STRETCH_FACTOR = 0.02f;
    const float INNER_RIPPLE_FREQ = 5.0f;

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
    float3 distanceAttenuation = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < c_numLights; ++i)
    {
        float3 lightPos = c_lights[i].c_position;
        float3 lightToPixel = worldPos - lightPos;
        float distance = length(lightToPixel);
        float3 lightAttenuation = exp(-absorption * distance * depthScale);
        distanceAttenuation = max(distanceAttenuation, lightAttenuation);
    }
    
    //for directional Light (only worry about depth below the surface)
    float depthBelowSurface = c_seaLevel - worldPos.z;
    float3 depthAttenuation = exp(-absorption * depthBelowSurface * c_underwaterTintDepthScale);
    distanceAttenuation = max(distanceAttenuation, depthAttenuation);
    return min(scatterColor + distanceAttenuation, float3(1.0, 1.0, 1.0));
}


float3 GetLightRGB(float3 worldPos, float3 worldNormal, float3 roughnessRGB, float seaLevel, float terrainDepthFraction)
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
    
    float3 sunDirection = normalize(c_sunDirection);
    
    float causticShallow = CausticPattern(worldPos, worldNormal, 0.075, seaLevel) * wShallow;
    float causticMid = CausticPattern(worldPos, worldNormal, 0.045, seaLevel) * wMid;
    float causticDeep = CausticPattern(worldPos, worldNormal, 0.015, seaLevel) * wDeep;
    
    float caustic = causticShallow + causticMid + causticDeep;

    //--- Sunlight ---
    float ambientIntensity = c_ambientIntensity * 2.5;
    float3 lightDir = sunDirection.xyz;
    float nDotL = dot(-lightDir, worldNormal);
    float directionalLightStrength = RangeMap(nDotL, -ambientIntensity, 1.0, 0.0, 1.0);
    float causticLightStrength = c_sunIntensity * saturate(directionalLightStrength);
    float3 directionalColor = c_sunIntensity * saturate(directionalLightStrength) * saturate(1.0 - (terrainDepthFraction * c_underwaterDepthDarkenScale)); // reduce light strength by depth;
    float3 combinedLightColor = directionalColor;

    //--- Point Lights ---
    float pointLightIntensity = saturate(terrainDepthFraction * 3.0); // decrease light strength by shallow

    float totalPointLightStrength = 0.0f;
    for (int i = 0; i < c_numLights; ++i)
    {
        float3 pixelToLightDisp = (c_lights[i].c_position - worldPos);
        float distance = length(pixelToLightDisp);
        float falloff = SmoothStep3(saturate(RangeMap(distance, c_lights[i].c_innerRadius, c_lights[i].c_outerRadius, 1.f, 0.f)));
        float3 pixelToLightDir = normalize(pixelToLightDisp);
        float lightDot = dot(worldNormal, pixelToLightDir);
        float penumbra = SmoothStep3(saturate(RangeMap(dot(c_lights[i].c_spotForward, -pixelToLightDir),c_lights[i].c_penumbraInnerDotThreshold,c_lights[i].c_penumbraOuterDotThreshold, 1.f, 0.f)));
        float lightStrength = penumbra * falloff * saturate(RangeMap(lightDot, -ambientIntensity, 1.f, 0.f, 1.f)) * c_lights[i].c_color.a;
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


uint GetGrassBufferIndex(in uint bufferIndex)
{	
	uint group = bufferIndex / 4;
	uint offset = bufferIndex % 4;
    uint index = bufferIndexes[group][offset];
    return index;
}

//Water Currents
//------------------------------------------------------------------------------------------------

float3 GetBendDirectionWithCurrents(float3 naturalBendDir, float3 bladeUpAxis, float heightFraction, WindSample windSample, float rigidityIn, out float rigidityOut)
{
    //Get Bend direction based on alignment with wave current
    // ---------------------------------------------------------------------
    float3 currentDirWS = windSample.worldDirection;
    float currentStrength = windSample.strength;
    
    float dotUp = dot(currentDirWS, bladeUpAxis);
    float upAlign = saturate(dotUp);
    
    float rigidityMax = max(rigidityIn, 0.75);
    rigidityOut = rigidityIn;
    rigidityOut = lerp(rigidityIn, rigidityMax, (upAlign * currentStrength));
    
    // Project both to XY plane
    float3 naturalXY = float3(naturalBendDir.x, naturalBendDir.y, 0.0f);
    float naturalXYLenSq = dot(naturalXY, naturalXY);
    if (naturalXYLenSq > 0.0001f)
    {
        naturalXY = naturalXY * rsqrt(naturalXYLenSq);
    }
    else
    {
        naturalXY = float3(1.0f, 0.0f, 0.0f);
    }

    float3 currentXY = float3(currentDirWS.x, currentDirWS.y, 0.0f);
    float currentXYLenSq = dot(currentXY, currentXY);
    if (currentXYLenSq > 0.0001f)
    {
        currentXY = currentXY * rsqrt(currentXYLenSq);
    }
    else
    {
    // no horizontal current
        currentXY = naturalXY;
    }

    // Alignment in XY: -1 opposite, +1 same
    float alignmentXY = dot(naturalXY, currentXY);

    // You want strong influence when |alignment| is near 1, weak when near 0
    float absAlign = abs(alignmentXY);

    // Shape this so it is very small near 0 and ramps hard near 1
    float followWholeBlade = SmoothStep3(absAlign); // 0 at perpendicular, 1 at parallel/opposed

    // Height factor for "tip only"
    float tipFactor = (heightFraction * heightFraction);

// Blend behavior:
// - if followWholeBlade is 1: weight is 1 everywhere (whole blade follows)
// - if followWholeBlade is 0: weight is tipFactor (tip follows, base stays)
    float currentInfluenceByHeight = lerp(tipFactor, 1.0f, followWholeBlade);

// Upward current straightens: reduce bend when current has large +z
    float upward01 = saturate(currentDirWS.z); // 0 flat/horizontal, 1 straight up
    float straightenFactor = lerp(1.0f, 0.0f, upward01); // 1 no change, 0 fully straight

// Final influence also respects rigidity
    float finalInfluence = currentInfluenceByHeight * (1.0f - rigidityOut) * straightenFactor;

// Use current direction within the blade plane (so it stays plausible)
    float3 currentInPlaneWS = currentDirWS - (bladeUpAxis * dot(currentDirWS, bladeUpAxis));
    float currentInPlaneLenSq = dot(currentInPlaneWS, currentInPlaneWS);
    if (currentInPlaneLenSq > 0.0001f)
    {
        currentInPlaneWS = currentInPlaneWS * rsqrt(currentInPlaneLenSq);
    }
    else
    {
        currentInPlaneWS = naturalBendDir;
    }

    return normalize(lerp(naturalBendDir, currentInPlaneWS, finalInfluence));
}

float GetTerrainDepthFraction(float worldHeight)
{
    float terrainDepthFraction = 1.0;
    float depthBelowSurface = c_seaLevel - worldHeight;
    terrainDepthFraction = RangeMapClamped(depthBelowSurface, 0.f, c_maxTerrainDepth, 0.1f, 1.f);
    return terrainDepthFraction;
}

v2p_t VertexMain(vs_input_t input, uint instanceId : SV_InstanceID)
{
    float3 modelTangent = input.modelTangent;
    float3 modelBitangent = input.modelBitangent;
    float3 modelNormal = input.modelNormal;
    
    //Get grass instance
    // ---------------------------------------------------------------------
    uint grassBufferIndex = GetGrassBufferIndex(0);
    VegetationInstance grassInstance = instanceBuffers[grassBufferIndex][instanceId];

    //Get Height and width
    // ---------------------------------------------------------------------
    float randomizedHeight = GetRandomValueInRange(grassInstance.seed, c_vegetationHeightMin, c_vegetationHeightMax) * grassInstance.heightMultiplier;
    float randomizedWidth = GetRandomValueInRange(grassInstance.seed, c_vegetationWidthMin, c_vegetationWidthMax);

    //Get Instance orientation 
    // ---------------------------------------------------------------------
    float3 bladeUpAxisWS = normalize(grassInstance.terrainNormal);
    float3 referenceForwardWS = float3(1.0f, 0.0f, 0.0f);
    if (abs(dot(referenceForwardWS, bladeUpAxisWS)) > 0.99f)
    {
        referenceForwardWS = float3(0.0f, 1.0f, 0.0f); // world left
    }

    float3 bladePlaneForwardWS = referenceForwardWS - (bladeUpAxisWS * dot(referenceForwardWS, bladeUpAxisWS));
    
     bladePlaneForwardWS = normalize(bladePlaneForwardWS);
    float3 bladePlaneRightWS = normalize(cross(bladePlaneForwardWS, bladeUpAxisWS));

    float yawRadians = grassInstance.yaw;
    float yawSin = sin(yawRadians);
    float yawCos = cos(yawRadians);

    float3 yawedPlaneAxisX_WS = (bladePlaneRightWS * yawCos) + (bladePlaneForwardWS * yawSin);
    float3 yawedPlaneAxisY_WS = (bladePlaneForwardWS * yawCos) - (bladePlaneRightWS * yawSin);
    

    // Local blade vertex position and scaling
    // local.x = halfWidth, local.y = thickness, local.z = height (0..1)
    // ---------------------------------------------------------------------
    float3 bladeLocalPosition = input.modelPosition;
    float halfWidth = (randomizedWidth * 0.5f);
    float3 bladeLocalPositionScaled = float3((bladeLocalPosition.x * halfWidth),(bladeLocalPosition.y * halfWidth),(bladeLocalPosition.z * randomizedHeight));

    //Get Bend Angle
    // ---------------------------------------------------------------------
    float rigidity = c_rigidity;
    float heightFraction = saturate(bladeLocalPosition.z);
    float3 naturalBendDir = normalize(yawedPlaneAxisY_WS); 
    
    WindSample currentSample = SampleUnderwaterCurrent(grassInstance.position, instanceId, c_time, c_windInfo);
    
    float3 bendDirectionWS = GetBendDirectionWithCurrents(naturalBendDir, bladeUpAxisWS, heightFraction, currentSample, c_rigidity, rigidity);

    //Bend grass blade along angle
     // ---------------------------------------------------------------------
    float maxBendAngleRadians = lerp(1.5f, 0.0f, rigidity);
    float bendProfile = pow(heightFraction, 0.5f);
    float bendAngleRadians = max(0.01, (maxBendAngleRadians * bendProfile));
    
    float3 bendAxisWS = normalize(cross(bendDirectionWS, bladeUpAxisWS));
    float bendAxisLenSq = dot(bendAxisWS, bendAxisWS);
    if (bendAxisLenSq > 0.0001f)
    {
        bendAxisWS *= rsqrt(bendAxisLenSq);
    }
    else
    {
        bendAxisWS = normalize(cross(naturalBendDir, bladeUpAxisWS));
    }

    float3 bentPlaneAxisX_WS = RotateAroundAxis(yawedPlaneAxisX_WS, bendAxisWS, bendAngleRadians);
    float3 bentPlaneAxisY_WS = RotateAroundAxis(yawedPlaneAxisY_WS, bendAxisWS, bendAngleRadians);

    float3 spineOffsetWS = GetArcSpineOffset(bladeUpAxisWS, bendDirectionWS, bladeLocalPositionScaled.z, bendAngleRadians);

    //Get world Pos, and TBN
    // ---------------------------------------------------------------------
    float3 crossSectionOffsetWS = (bentPlaneAxisX_WS * bladeLocalPositionScaled.x) + (bentPlaneAxisY_WS * bladeLocalPositionScaled.y);
    float3 worldPositionWS = grassInstance.position + spineOffsetWS + crossSectionOffsetWS;
    float4 worldPosition = float4(worldPositionWS, 1.0f);


    float3 ribbonWidthDirectionWS = normalize(bentPlaneAxisX_WS);
    float3 ribbonSpineDirectionWS = GetArcSpineTangent(bladeUpAxisWS, bendDirectionWS, bendAngleRadians);

    float3 worldNormal = normalize(cross(ribbonWidthDirectionWS, ribbonSpineDirectionWS));
    float3 worldTangent = ribbonSpineDirectionWS;
    float3 worldBitangent = normalize(cross(worldNormal, worldTangent));
    
    //Get Curved normal (smoother lighting)
    // ---------------------------------------------------------------------
    float widthParam = saturate((bladeLocalPosition.x * 0.5f) + 0.5f); // maps [-1,1] to [0,1]
    float signedWidth = (widthParam * 2.0f) - 1.0f; // maps back to [-1,1]
    float curveStrength = 1.0f;
    
    float widthFraction = ((bladeLocalPosition.x * 0.5f) + 0.5f);
    float3 curvedNormal = normalize( (worldNormal * (1.0f - abs(signedWidth))) +(worldTangent * signedWidth * curveStrength));
    
    //Get grass shade (darker towards base)
    // ---------------------------------------------------------------------
    float3 grassShade = lerp(float3(0.7, 0.5, 0.7), float3(1.0, 0.7, 1.0), heightFraction * heightFraction);
    
    //Get clip position
    // ---------------------------------------------------------------------
    float4 cameraPosition = mul(c_worldToCameraTransform, worldPosition);
    float4 renderPosition = mul(c_cameraToRenderTransform, cameraPosition);
    float4 clipPosition = mul(c_renderToClipTransform, renderPosition);

    
    // Output
    // ---------------------------------------------------------------------
    v2p_t o = (v2p_t) 0;
    o.clipPosition = clipPosition;

    o.worldTangent = worldTangent;
    o.worldBitangent = worldBitangent;
    o.worldNormal = worldNormal;
    o.worldPosition = worldPositionWS;

    o.modelTangent = modelTangent;
    o.modelBitangent = modelBitangent;
    o.modelNormal = modelNormal;

    o.color = float4(grassShade, 1.0) * input.color;
    o.uv = input.uv;
    //o.worldBitangent = widthFraction;
    o.curvedNormal = curvedNormal;
    return o;
}

float4 PixelMain(v2p_t input, bool isFrontFace : SV_IsFrontFace) : SV_Target0
{
    float2 uvCoords = input.uv;
    uint diffuseTexIndex = 0;
    uint group;
    uint offset;
    GetTextureGroupAndOffset(diffuseTexIndex, group, offset);
    diffuseTexIndex = textureIndexes[group][offset];
    Texture2D diffuseTexture = textures[diffuseTexIndex];
    float4 diffuseTexel = diffuseTexture.Sample(s_pointClampSampler, uvCoords);
    float3 diffuseColor = input.color.rgb * diffuseTexel.rgb;
    
    float3 underwaterTintColor = GetUnderwaterTintRGB(input.worldPosition.xyz);
    
    float3 worldNormal = input.worldNormal;
    float3 worldTangent = input.worldTangent;
    float3 worldBitangent = input.worldBitangent;
    float3 curvedNormal = normalize(input.curvedNormal);

    if (!isFrontFace)
    {
        worldNormal *= -1.f;
        curvedNormal *= -1.0 * (1.0 - input.widthFraction);
    }   

    float terrainDepthFraction = GetTerrainDepthFraction(input.worldPosition.z);

    float3 specularity = float3(1.0, 1.0, 1.0);
    float3 lightColor = GetLightRGB(input.worldPosition, curvedNormal, specularity, c_seaLevel, terrainDepthFraction);
    
    float3 color = diffuseColor * lightColor * underwaterTintColor;
    float4 finalColor = float4(color, terrainDepthFraction);
    
    if (c_debugInt == c_normals)
    {
        finalColor.rgb = EncodeXYZToRGB(worldNormal);
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
    
    if(c_debugInt == c_emission)
    {
        finalColor.rgb = 0.f.xxx;
    }
    
    
    clip(finalColor.a - 0.01f);
    return finalColor;
}

