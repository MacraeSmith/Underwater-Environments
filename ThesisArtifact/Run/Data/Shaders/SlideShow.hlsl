cbuffer PerFrameConstants : register(b1)
{
    float c_time;
    int c_debugInt;
    float c_debugFloat;
    float PADDING;
}; // b1

cbuffer CameraConstants : register(b2)
{
    float4x4 WorldToCameraTransform;
    float4x4 CameraToRenderTransform;
    float4x4 RenderToClipTransform;
};

cbuffer ModelConstants : register(b3)
{
    float4x4 ModelToWorldTransform;
    float4 ModelColor;
};


#define NUM_TEXTURE_SLOTS 16
#define MAX_NUM_BINDLESS_TEXTURES 4096

cbuffer TextureConstants : register(b5)
{
    uint4 textureIndexes[NUM_TEXTURE_SLOTS / 4];
};

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

Texture2D textures[MAX_NUM_BINDLESS_TEXTURES] : register(t0, space0);
SamplerState diffuseSampler : register(s0);

void GetTextureGroupAndOffset(in uint textureIndex, out uint out_group, out uint out_offset)
{
    out_group = (textureIndex / 4);
    out_offset = (textureIndex % 4);
}

float4 SampleBindlessTexture(uint textureIndex, float2 uv)
{
    uint group;
    uint offset;
    GetTextureGroupAndOffset(textureIndex, group, offset);
    return textures[textureIndexes[group][offset]].Sample(diffuseSampler, uv);
}

v2p_t VertexMain(vs_input_t input)
{
    float4 modelSpacePosition = float4(input.modelSpacePosition, 1.0f);
    float4 worldSpacePosition = mul(ModelToWorldTransform, modelSpacePosition);
    float4 cameraSpacePosition = mul(WorldToCameraTransform, worldSpacePosition);
    float4 renderSpacePosition = mul(CameraToRenderTransform, cameraSpacePosition);
    float4 clipSpacePosition = mul(RenderToClipTransform, renderSpacePosition);

    v2p_t v2p;
    v2p.clipSpacePosition = clipSpacePosition;
    v2p.color = input.color;
    v2p.uv = input.uv;
    return v2p;
}

float4 PixelMain(v2p_t input) : SV_Target0
{
    float4 textureColor0 = SampleBindlessTexture(0, input.uv);
    float4 textureColor1 = SampleBindlessTexture(1, input.uv);

    float fadeT = ((sin(c_time * c_debugFloat) * 0.5f) + 0.5f);

    float4 blendedTextureColor = lerp(textureColor0, textureColor1, fadeT);
    float4 finalColor = (blendedTextureColor * ModelColor * input.color);

    clip(finalColor.a - 0.01f);
    return finalColor;
}