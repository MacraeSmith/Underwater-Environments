void GetTextureGroupAndOffset(in uint biomeIndex, out uint out_group, out uint out_offset)
{
    out_group = biomeIndex / 4;
    out_offset = biomeIndex % 4;
}

float LinearizeDepth(float depth, float nearZ, float farZ)
{
    float linearDepth = (farZ * nearZ) / (farZ -  ( depth * (farZ - nearZ)));
    return saturate((linearDepth - nearZ) / (farZ - nearZ));
}

float ConvertDepthToViewSpace(float depth, float nearPlane, float farPlane)
{
    return ((nearPlane * farPlane) / (farPlane - (depth * (farPlane - nearPlane))));
}

float3 TriplanarSampleColor(float3 worldPos, float3 normal, float scale, Texture2D<float4> texToSample, SamplerState samplerState)
{
    // Axis projections, matching X=forward, Y=left, Z=up
    float2 uvX = (worldPos.yz * scale); // project along forward (X), so use Y/Z
    float2 uvY = (worldPos.xz * scale); // project along left (Y), so use X/Z
    float2 uvZ = (worldPos.xy * scale); // project along up (Z), so use X/Y
    
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
    float2 uvX = (worldPos.yz * scale);
    float2 uvY = (worldPos.xz * scale);
    float2 uvZ = (worldPos.xy * scale);
	
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

float TriplanarSampleHeightLevel(float3 worldPos, float3 worldNormal, float textureScale, Texture2D<float4> heightTex, SamplerState samplerState, float lod)
{
    float3 absN = abs(worldNormal);
    float sumAbsN = max((absN.x + absN.y + absN.z), 0.000001f);
    float3 triW = (absN / sumAbsN);

    float2 uvX = (worldPos.yz * textureScale);
    float2 uvY = (worldPos.xz * textureScale);
    float2 uvZ = (worldPos.xy * textureScale);

    float hx = heightTex.SampleLevel(samplerState, uvX, lod).r;
    float hy = heightTex.SampleLevel(samplerState, uvY, lod).r;
    float hz = heightTex.SampleLevel(samplerState, uvZ, lod).r;

    return ((hx * triW.x) + (hy * triW.y) + (hz * triW.z));
}

float3 TriplanarSampleColorMipLevel(float3 worldPos, float3 normal, float scale, Texture2D<float4> texToSample, SamplerState samplerState, float mipLevel)
{
    float2 uvX = (worldPos.yz * scale);
    float2 uvY = (worldPos.xz * scale);
    float2 uvZ = (worldPos.xy * scale);

    float3 xTex = texToSample.SampleLevel(samplerState, uvX, mipLevel).rgb;
    float3 yTex = texToSample.SampleLevel(samplerState, uvY, mipLevel).rgb;
    float3 zTex = texToSample.SampleLevel(samplerState, uvZ, mipLevel).rgb;

    float3 weights = abs(normal);
    float sum = (weights.x + weights.y + weights.z + 1e-6f);
    weights /= sum;

    return ((xTex * weights.x) + (yTex * weights.y) + (zTex * weights.z));
}

float3 TriplanarSampleNormalMipLevel(float3 worldPos, float3 normal, float scale, Texture2D<float4> texToSample, SamplerState samplerState, float mipLevel)
{
    float2 uvX = (worldPos.yz * scale);
    float2 uvY = (worldPos.xz * scale);
    float2 uvZ = (worldPos.xy * scale);

    float3 xN = ((texToSample.SampleLevel(samplerState, uvX, mipLevel).rgb * 2.0f) - 1.0f);
    float3 yN = ((texToSample.SampleLevel(samplerState, uvY, mipLevel).rgb * 2.0f) - 1.0f);
    float3 zN = ((texToSample.SampleLevel(samplerState, uvZ, mipLevel).rgb * 2.0f) - 1.0f);

    float3 xWorld = float3(xN.z, xN.x, xN.y);
    xWorld.x *= sign(normal.x);

    float3 yWorld = float3(yN.x, yN.z, yN.y);
    yWorld.y *= sign(normal.y);

    float3 zWorld = float3(zN.x, zN.y, zN.z);
    zWorld.z *= sign(normal.z);

    float3 weights = abs(normal);
    float sum = (weights.x + weights.y + weights.z + 1e-6f);
    weights /= sum;

    return normalize(((xWorld * weights.x) + (yWorld * weights.y) + (zWorld * weights.z)));
}


float3 ReconstructWorldPosition(float2 uv, float deviceDepth, float4x4 clipToWorldTransform)
{
    float clipX = ((uv.x * 2.0f) - 1.0f);
    float clipY = (((1.0f - uv.y) * 2.0f) - 1.0f);
    float clipZ = ((deviceDepth * 2.0f) - 1.0f);
    //float clipZ = deviceDepth;

    float4 clipPosition = float4(clipX, clipY, clipZ, 1.0f);
    float4 reconstructed = mul(clipToWorldTransform, clipPosition);

    float w = reconstructed.w;
    if (abs(w) < 0.0001f)
    {
        w = 0.0001f;
    }

    return (reconstructed.xyz / w);
}

float3 BlurTexture(float2 uv, Texture2D<float4> tex, SamplerState sampleState, float blurRadius, float2 screenSize)
{
    float2 texelSize = float2(1.0f / screenSize.x, 1.0f / screenSize.y);

    float2 offset = texelSize * blurRadius;

    float3 color = 0;
    color += tex.Sample(sampleState, uv + offset * float2(-1, -1)).rgb * 0.0625f;
    color += tex.Sample(sampleState, uv + offset * float2(0, -1)).rgb * 0.125f;
    color += tex.Sample(sampleState, uv + offset * float2(1, -1)).rgb * 0.0625f;

    color += tex.Sample(sampleState, uv + offset * float2(-1, 0)).rgb * 0.125f;
    color += tex.Sample(sampleState, uv).rgb * 0.25f;
    color += tex.Sample(sampleState, uv + offset * float2(1, 0)).rgb * 0.125f;

    color += tex.Sample(sampleState, uv + offset * float2(-1, 1)).rgb * 0.0625f;
    color += tex.Sample(sampleState, uv + offset * float2(0, 1)).rgb * 0.125f;
    color += tex.Sample(sampleState, uv + offset * float2(1, 1)).rgb * 0.0625f;

    return color;
}

float3 GetBloomBlur(float2 uv, Texture2D<float4> sourceTexture, SamplerState sampleState, float2 screenSize, int sampleRadius, float radiusScale)
{
    float2 texelSize = float2((1.0f / screenSize.x), (1.0f / screenSize.y));

    float3 accumulatedColor = float3(0.0f, 0.0f, 0.0f);
    float accumulatedWeight = 0.0f;

    for (int sampleY = (-sampleRadius); sampleY <= sampleRadius; ++sampleY)
    {
        for (int sampleX = (-sampleRadius); sampleX <= sampleRadius; ++sampleX)
        {
            float2 sampleOffset = float2((float) sampleX, (float) sampleY);
            float normalizedDistance = (length(sampleOffset) / (float) sampleRadius);
            float sampleWeight = (1.0f - saturate(normalizedDistance));
            sampleWeight = (sampleWeight * sampleWeight);

            float2 sampleUV = (uv + (sampleOffset * texelSize * radiusScale));
            float3 sampleColor = sourceTexture.Sample(sampleState, sampleUV).rgb;

            accumulatedColor += (sampleColor * sampleWeight);
            accumulatedWeight += sampleWeight;
        }
    }

    if (accumulatedWeight > 0.0f)
    {
        accumulatedColor /= accumulatedWeight;
    }

    return accumulatedColor;
}


float2 GetScreenUVFromClipPosition(float4 clipPosition)
{
    float2 ndc = (clipPosition.xy / clipPosition.w);
    float2 screenUV = ((ndc * 0.5f) + 0.5f);
    screenUV.y = (1.0f - screenUV.y);
    return screenUV;
}
