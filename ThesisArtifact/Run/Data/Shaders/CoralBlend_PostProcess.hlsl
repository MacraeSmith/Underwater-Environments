#include "Data/Shaders/Utils/CommonUtils.hlsli"
#include "Data/Shaders/Utils/TextureUtils.hlsli"

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

cbuffer PerFrameConstants : register(b1)
{
    float c_time;
    int c_debugInt;
    float c_debugFloat;
    float PADDING;
}; //b1

cbuffer CameraConstants : register(b2)
{
    float4x4 WorldToCameraTransform; // View transform
    float4x4 CameraToRenderTransform; // Non-standard transform from game to DirectX conventions
    float4x4 RenderToClipTransform; // Projection transform
    float3 c_cameraPosition;
    float pad1;
};



#define NUM_TEXTURE_SLOTS 16
#define MAX_NUM_BINDLESS_TEXTURES 4096

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


cbuffer CoralBlendConstants : register(b12)
{
    float c_blendStrength;
    float c_blendThreshold;
    float c_blendRadius;
}

#define FAR_PLANE 500

Texture2D textures[MAX_NUM_BINDLESS_TEXTURES] : register(t0, space0);

SamplerState s_pointClampSampler : register(s0);
SamplerState s_bilinearWrapSampler : register(s1);

float GetDepthWeight(float sampleDepth, float centerDepth, float blendThreshold)
{
    return (1.0f - saturate(abs(sampleDepth - centerDepth) / blendThreshold));
}

float2 ClampUV(float2 uvCoords)
{
    return clamp(uvCoords, float2(0.0f, 0.0f), float2(1.0f, 1.0f));
}

float GetWorldSpacePixelRadius(float deviceDepth)
{
    float viewSpaceDepth = ConvertDepthToViewSpace(deviceDepth, 0.1f, FAR_PLANE);
    float projectionScaleY = max(abs(RenderToClipTransform[1][1]), 0.0001f);
    float screenHeight = max(c_screenSize.y, 1.0f);
    float worldUnitsPerPixel = max(((2.0f * viewSpaceDepth) / (screenHeight * projectionScaleY)), 0.0001f);

    float linearDepth = saturate((viewSpaceDepth - 0.1f) / (FAR_PLANE - 0.1f));
    float blendRadius = lerp(c_blendRadius, (5.0f * c_blendRadius), linearDepth);

    float pixelRadius = (blendRadius / worldUnitsPerPixel);
    return clamp(pixelRadius, 1.0f, 32.0f);
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
    float2 uvCoords = input.uv;

    uint group;
    uint offset;

    uint sceneColorTextureIndex = 0;
    GetTextureGroupAndOffset(sceneColorTextureIndex, group, offset);
    sceneColorTextureIndex = textureIndexes[group][offset];
    Texture2D sceneColorTexture = textures[sceneColorTextureIndex];

    uint sceneDepthTextureIndex = 1;
    GetTextureGroupAndOffset(sceneDepthTextureIndex, group, offset);
    sceneDepthTextureIndex = textureIndexes[group][offset];
    Texture2D sceneDepthTexture = textures[sceneDepthTextureIndex];

    uint coralMaskTextureIndex = 2;
    GetTextureGroupAndOffset(coralMaskTextureIndex, group, offset);
    coralMaskTextureIndex = textureIndexes[group][offset];
    Texture2D coralMaskTexture = textures[coralMaskTextureIndex];

    float4 centerColor = sceneColorTexture.Sample(s_pointClampSampler, uvCoords);
    float centerDepth = sceneDepthTexture.Sample(s_pointClampSampler, uvCoords).r;

    float pixelRadius = GetWorldSpacePixelRadius(centerDepth);
    float2 texelOffset = (pixelRadius / c_screenSize);

    float2 sampleOffsets[12];
    sampleOffsets[0] = (texelOffset * float2(1.0f, 0.0f));
    sampleOffsets[1] = (texelOffset * float2(-1.0f, 0.0f));
    sampleOffsets[2] = (texelOffset * float2(0.0f, 1.0f));
    sampleOffsets[3] = (texelOffset * float2(0.0f, -1.0f));
    sampleOffsets[4] = (texelOffset * float2(0.70710678f, 0.70710678f));
    sampleOffsets[5] = (texelOffset * float2(-0.70710678f, 0.70710678f));
    sampleOffsets[6] = (texelOffset * float2(0.70710678f, -0.70710678f));
    sampleOffsets[7] = (texelOffset * float2(-0.70710678f, -0.70710678f));
    sampleOffsets[8] = ((texelOffset * 0.5f) * float2(1.0f, 0.0f));
    sampleOffsets[9] = ((texelOffset * 0.5f) * float2(-1.0f, 0.0f));
    sampleOffsets[10] = ((texelOffset * 0.5f) * float2(0.0f, 1.0f));
    sampleOffsets[11] = ((texelOffset * 0.5f) * float2(0.0f, -1.0f));

    // Blur the scene color locally with depth-aware weights.
    float4 blurredColor = (centerColor * 4.0f);
    float totalColorWeight = 4.0f;

    // Gather nearby contact so both sides of an intersection can receive the blend.
    float contactField = 0.0f;

    [unroll]
    for (int sampleIndex = 0; sampleIndex < 12; ++sampleIndex)
    {
        float2 sampleUV = ClampUV(uvCoords + sampleOffsets[sampleIndex]);
        float sampleDepth = sceneDepthTexture.Sample(s_pointClampSampler, sampleUV).r;
        float depthWeight = GetDepthWeight(sampleDepth, centerDepth, c_blendThreshold);

        float kernelWeight = 1.0f;
        if (sampleIndex < 4)
        {
            kernelWeight = 1.5f;
        }
        else if (sampleIndex >= 8)
        {
            kernelWeight = 2.0f;
        }

        float sampleWeight = (kernelWeight * depthWeight);

        float4 sampleColor = sceneColorTexture.Sample(s_bilinearWrapSampler, sampleUV);
        float sampleContactMask = coralMaskTexture.Sample(s_pointClampSampler, sampleUV).g;

        blurredColor += (sampleColor * sampleWeight);
        totalColorWeight += sampleWeight;

        contactField = max(contactField, (sampleContactMask * sampleWeight));
    }

    blurredColor /= max(totalColorWeight, 0.0001f);

    float blendMask = saturate(contactField * c_blendStrength);
    blendMask = smoothstep(0.05f, 0.5f, blendMask);

    /*
    if (c_debugInt == 1)
    {
        return float4(coralMaskTexture.Sample(s_pointClampSampler, uvCoords).g.xxx, 1.0f);
    }

    if (c_debugInt == 2)
    {
        return float4(contactField.xxx, 1.0f);
    }

    if (c_debugInt == 3)
    {
        return float4(blendMask.xxx, 1.0f);
    }
    */

    float4 blendedColor = lerp(centerColor, blurredColor, blendMask);

    return blendedColor;
}