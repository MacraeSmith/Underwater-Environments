void GetTextureGroupAndOffset(in uint biomeIndex, out uint out_group, out uint out_offset)
{
    out_group = biomeIndex / 4;
    out_offset = biomeIndex % 4;
}

float LinearizeDepth(float depth, float nearZ, float farZ)
{
    float linearDepth = (farZ * nearZ) / (farZ - depth * (farZ - nearZ));
    return saturate((linearDepth - nearZ) / (farZ - nearZ));
}

/*
float3 TriplanarSampleColor(float3 worldPos, float3 normal, float scale, Texture2D<float4> texToSample, SamplerState samplerState)
{
    // Axis projections, matching X=forward, Y=left, Z=up
    float2 uvX = frac(worldPos.yz * scale); // project along forward (X), so use Y/Z
    float2 uvY = frac(worldPos.xz * scale); // project along left (Y), so use X/Z
    float2 uvZ = frac(worldPos.xy * scale); // project along up (Z), so use X/Y
    
    float3 xTex = texToSample.Sample(samplerState, uvX).rgb;
    float3 yTex = texToSample.Sample(samplerState, uvY).rgb;
    float3 zTex = texToSample.Sample(samplerState, uvZ).rgb;
    
    float3 weights = abs(normal);
    float sum = weights.x + weights.y + weights.z + 1e-6;
    weights /= sum;

    return xTex * weights.x + yTex * weights.y + zTex * weights.z;
}

float3 TriplanarSampleNormal(float3 worldPos, float3 normal, float scale, Texture2D<float4> texToSample, SamplerState samplerState)
{
    float2 uvX = frac(worldPos.yz * scale);
    float2 uvY = frac(worldPos.xz * scale);
    float2 uvZ = frac(worldPos.xy * scale);
	
    float3 xN = texToSample.Sample(samplerState, uvX).rgb * 2.0 - 1.0;
    float3 yN = texToSample.Sample(samplerState, uvY).rgb * 2.0 - 1.0;
    float3 zN = texToSample.Sample(samplerState, uvZ).rgb * 2.0 - 1.0;

    float3 xWorld = float3(xN.z, xN.x, xN.y);
    xWorld.x *= sign(normal.x);
    float3 yWorld = float3(yN.x, yN.z, yN.y);
    yWorld.y *= sign(normal.y);
    float3 zWorld = float3(zN.x, zN.y, zN.z);
    zWorld.z *= sign(normal.z);

    float3 weights = abs(normal);
    float sum = weights.x + weights.y + weights.z + 1e-6;
    weights /= sum;
    float3 blendedNormal = normalize(xWorld * weights.x + yWorld * weights.y + zWorld * weights.z);

    return blendedNormal;
}
*/