#include "Data/Shaders/Utils/CommonUtils.hlsli"
#include "Data/Shaders/Utils/TextureUtils.hlsli"

//------------------------------------------------------------------------------------------------
struct vs_input_t
{
    float3 modelSpacePosition : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};
	
struct v2p_t
{
    float4 clipSpacePosition : SV_Position;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};


//Constant Buffers
//------------------------------------------------------------------------------------------------
cbuffer PerFrameConstants : register(b1)
{
	float c_time;
	int c_debugInt;
	float c_debugFloat;
	float PADDING;	
};

cbuffer CameraConstants : register(b2)
{
	float4x4 WorldToCameraTransform;	// View transform
	float4x4 CameraToRenderTransform;	// Non-standard transform from game to DirectX conventions
	float4x4 RenderToClipTransform;		// Projection transform
    float3 c_cameraPosition;
    float CAMERA_PADDING;
};

cbuffer InverseCameraConstants : register(b13)
{
    float4x4 ClipToWorldTransform;
}


cbuffer LightConstants : register(b4)
{
    float3 c_sunDirection;
    float c_sunIntensity;
    float3 c_sunColorRGB;
    float c_ambientIntensity;
    int c_numLights;
    float3 LIGHT_CONSTANTS_PADDING;
	
   // LightData c_lights[MAX_LIGHTS];
}; //b4

#define NUM_TEXTURE_SLOTS 16
cbuffer TextureConstants : register(b5)
{
	uint4 textureIndexes[NUM_TEXTURE_SLOTS / 4];
};

struct FogData
{
    float4 color;
    float depth;
    float opacity;
    uint transitionExponent;
    float pad;
};
#define FAR_PLANE 500.0
cbuffer FogConstants : register(b6)
{
    FogData c_fogData[2];
    float c_underWaterTintDepthScale;
    float c_fogVolumeHeight;
    float3 FOG_PADDING;
}; //b6

cbuffer LightRayConstants : register(b7)
{
    float2 c_screenSize;
    float c_shaftIntensity;
    float c_shaftScale;
    
    float c_shaftSpread;
    float c_shaftThreshold;
    float c_shaftWarpSpeed;
    float c_shaftWarpStrength;
    
    float4 c_shaftColor;
    
    float c_shaftContrast;
    float c_shaftDepthFade;
    uint c_shaftMaxNumSteps;
    uint c_activeLightRays;
};


#define PI 3.14159265
#define MAX_WAVES 8
cbuffer WaterConstants : register(b8)
{
    float4 c_waveData[MAX_WAVES];
    float4 c_waveDataExtra[MAX_WAVES];
    int c_numWaves;
    float c_globalWaveSpeed;
    float c_seaLevel;
    float c_underwaterTintDepthScale;
    float c_underwaterDepthDarkenScale;
    float c_foaminess;
    float c_reflectionStrength;
    float c_maxTerrainDepth;
    float c_waterMirkyness;
    float c_shorelineFoamStrength;
    float c_worldVoxelScale;
};

cbuffer UnderwaterParticleConstants : register(b14)
{
    float c_particleDensity;
    float c_particleSizeMin;
    float c_particleSizeMax;
    float c_particleCellSize;
    
    uint c_particlesPerCell;
    float c_particleIntensity;
    float c_particleDepthFade;
    float c_particleDriftSpeed;
    
    uint c_activeParticles;
}


#define FAR_PLANE 500.0

//Texture and Sampler constants
//------------------------------------------------------------------------------------------------
#define MAX_NUM_BINDLESS_TEXTURES 4096
Texture2D<float4> t_textures[MAX_NUM_BINDLESS_TEXTURES] : register(t0, space0);
Texture3D<float3> t_3Dtextures[MAX_NUM_BINDLESS_TEXTURES] : register(t0, space2);
SamplerState s_sampler : register(s0);

//Helper functions
//------------------------------------------------------------------------------------------------

float4 ProjectWorldToClip(float3 worldPosition)
{
    float4 worldPosition4 = float4(worldPosition, 1.0f);
    float4 cameraPosition = mul(WorldToCameraTransform, worldPosition4);
    float4 renderPosition = mul(CameraToRenderTransform, cameraPosition);
    float4 clipPosition = mul(RenderToClipTransform, renderPosition);
    return clipPosition;
}


//Vertex shader
//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
    float4 clipSpacePosition = float4(input.modelSpacePosition, 1);
	
    v2p_t v2p;
    v2p.clipSpacePosition = clipSpacePosition;
    v2p.color = input.color;
    v2p.uv = input.uv;
    return v2p;
}

float4 PixelMain(v2p_t input) : SV_Target0
{
    if(c_activeLightRays != 1 && c_activeParticles != 1)
    {
        return float4(0.f.xxx, 1.f);
    }
    
    uint textureIndex = 0;
    uint group = 0;
    uint offset = 0;

    textureIndex = 1;
    GetTextureGroupAndOffset(textureIndex, group, offset);
    Texture2D<float4> depthTexture = t_textures[textureIndexes[group][offset]];
    float deviceDepth = depthTexture.Sample(s_sampler, input.uv).r;

    float fogDepth = c_fogData[0].depth;
    float linearDepth = LinearizeDepth(deviceDepth, 0.1f, fogDepth);
    float depthForMarch = saturate(linearDepth);

    textureIndex = 2;
    GetTextureGroupAndOffset(textureIndex, group, offset);
    Texture3D<float3> fogTexture = t_3Dtextures[textureIndexes[group][offset]];

    float3 farWorldPosition = ReconstructWorldPosition(input.uv, 1.0f, ClipToWorldTransform);
    float3 viewDirection = normalize(farWorldPosition - c_cameraPosition);
    float3 lightTravelDirection = normalize(c_sunDirection);

    float scattering = saturate(dot(viewDirection, (-lightTravelDirection)));
    scattering = lerp(0.35f, 1.0f, scattering);

    int stepCount = (int) (depthForMarch * c_shaftMaxNumSteps);
    if (stepCount < 8)
    {
        stepCount = 8;
    }

    float shaftAccumulation = 0.0f;
    float particleAccumulation = 0.0f;
        
    for (int stepIndex = 0; stepIndex < stepCount; ++stepIndex)
    {
        float z =(((((float) stepIndex) + 0.5f) / ((float) stepCount)) * depthForMarch);
        float3 fogSample = fogTexture.SampleLevel(s_sampler, float3(input.uv.x, input.uv.y, z), 0.0f).rgb;

        shaftAccumulation += (fogSample.r / ((float) stepCount)); 
        particleAccumulation = max(particleAccumulation, fogSample.g);
    }
    
    
    shaftAccumulation *= scattering;
    shaftAccumulation *= 2.5f;
    shaftAccumulation = saturate(shaftAccumulation);
    shaftAccumulation = pow(shaftAccumulation, 1.5f);
    shaftAccumulation = clamp(shaftAccumulation, 0.f, 0.75f);
    
    particleAccumulation = saturate(particleAccumulation);
    particleAccumulation = pow(particleAccumulation, 1.2f);

    return float4(shaftAccumulation, particleAccumulation, 0.f, 1.0f);
}