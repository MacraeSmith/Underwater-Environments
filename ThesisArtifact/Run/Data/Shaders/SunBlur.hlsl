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
cbuffer CameraConstants : register(b2)
{
    float4x4 WorldToCameraTransform; // View transform
    float4x4 CameraToRenderTransform; // Non-standard transform from game to DirectX conventions
    float4x4 RenderToClipTransform; // Projection transform
    float3 c_cameraPosition;
    float CAMERA_PADDING;
};

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

#define MAX_WAVES 8
cbuffer WaterConstants : register(b8)
{
    float4 c_waveData[MAX_WAVES];
    float4 c_waveDataExtra[MAX_WAVES];
    int c_numWaves;
    float c_globalWaveSpeed;
    
    float c_waveChoppiness;
    float c_waveRegionScale;
    float c_waveRegionStrength;
    float c_waveDirectionVariance;
    float c_wavePhaseVariance;
    float c_waveHeightScale;
    
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




#define FAR_PLANE 500.0

//Texture and Sampler constants
//------------------------------------------------------------------------------------------------
#define MAX_NUM_BINDLESS_TEXTURES 4096
Texture2D<float4> t_textures[MAX_NUM_BINDLESS_TEXTURES] : register(t0, space0);
SamplerState s_sampler : register(s0);

//Helper functions
//------------------------------------------------------------------------------------------------

bool IsUnderwater()
{
    return c_cameraPosition.z < c_seaLevel;
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
    uint textureIndex = 0;
    uint group = 0;
    uint offset = 0;

    GetTextureGroupAndOffset(textureIndex, group, offset);
    Texture2D sceneTexture = t_textures[textureIndexes[group][offset]];
    float4 sceneColor = sceneTexture.Sample(s_sampler, input.uv);
    
    if(IsUnderwater())
    {
        //return sceneColor;
    }
    
    textureIndex = 1;
    GetTextureGroupAndOffset(textureIndex, group, offset);
    Texture2D<float4> sunTexture = t_textures[textureIndexes[group][offset]];
    float sunMask = GetBloomBlur(input.uv, sunTexture, s_sampler, c_screenSize, 16.f, 5.f).r;
    
    float4 finalColor = lerp(sceneColor, float4(c_sunColorRGB + c_sunIntensity, 1.0), sunMask);
    return finalColor;

}