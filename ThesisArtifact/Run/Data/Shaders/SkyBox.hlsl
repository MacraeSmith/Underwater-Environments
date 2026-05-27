#include "Data/Shaders/Utils/CommonUtils.hlsli"
#include "Data/Shaders/Utils/TextureUtils.hlsli"

//------------------------------------------------------------------------------------------------
struct vs_input_t
{
	float3 modelPosition : POSITION;
	float4 color : COLOR;
	float2 uvTexCoords : TEXCOORD;
};

//------------------------------------------------------------------------------------------------
struct v2p_t
{
	float4 clipPosition : SV_Position;
	float4 color : COLOR;
	float2 uvTexCoords : TEXCOORD;
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

#define NUM_TEXTURE_SLOTS 16
#define MAX_NUM_BINDLESS_TEXTURES 4096
cbuffer TextureConstants : register(b5)
{
	uint4 textureIndexes[NUM_TEXTURE_SLOTS / 4];
};

//Texture and Sampler constants
//------------------------------------------------------------------------------------------------
Texture2D textures[MAX_NUM_BINDLESS_TEXTURES] : register(t0, space0);
SamplerState s_pointClampSampler : register(s0);

uint GetTextureIndexFromFaceColor(float3 faceColor)
{
    float3 faceDirection = DecodeRGBtoXYZ(faceColor);
    float3 absoluteDirection = abs(faceDirection);
    
    uint faceIndex = 5;
    if ((absoluteDirection.x >= absoluteDirection.y) && (absoluteDirection.x >= absoluteDirection.z))
    {
        faceIndex = (faceDirection.x >= 0.0f) ? 0u : 3u; // +X : -X
    }

    if ((absoluteDirection.y >= absoluteDirection.x) && (absoluteDirection.y >= absoluteDirection.z))
    {
        faceIndex = (faceDirection.y >= 0.0f) ? 1u : 4u; // +Y : -Y
    }

    if ((absoluteDirection.z >= absoluteDirection.x) && (absoluteDirection.z >= absoluteDirection.y))
    {
		faceIndex = (faceDirection.z >= 0.0f) ? 2u : 5u; // +Z : -Z
    }

    return faceIndex;
}
//Vertex shader
//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
	float4 modelPosition = float4(input.modelPosition, 1.0);
	float4 worldPosition = mul(c_modelToWorldTransform, modelPosition);
	float4 cameraPosition = mul(c_worldToCameraTransform, worldPosition);
	float4 renderPosition = mul(c_cameraToRenderTransform, cameraPosition);
	float4 clipPosition = mul(c_renderToClipTransform, renderPosition);
	

	v2p_t v2p;
	v2p.clipPosition = clipPosition;
	v2p.color = input.color;
	v2p.uvTexCoords = input.uvTexCoords;
	return v2p;
}

//Pixel Shader
//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    uint textureIndex = GetTextureIndexFromFaceColor(input.color.rgb);
    uint group;
    uint offset;
    GetTextureGroupAndOffset(textureIndex, group, offset);
    Texture2D albedoTex = textures[textureIndexes[group][offset]];
    float3 albedoColor = albedoTex.Sample(s_pointClampSampler, input.uvTexCoords).rgb;
	
    return float4(albedoColor, 1.0);
}
