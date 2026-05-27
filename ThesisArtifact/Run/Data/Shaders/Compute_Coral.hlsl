#include "Data/Shaders/Utils/VegetationUtils.hlsli"
//#include "Data/Shaders/Utils/Common.hlsli"
struct VegetationVertex
{
    float3 position;
    float pad;
    float3 terrainNormal;
    uint pad2;
};

struct CoralInstance
{
    float4x4 worldTransform;
    uint seed;
};

cbuffer CoralCSConstants : register(b0)
{
    uint vertexCount;
    float3 pad;
};

StructuredBuffer<VegetationVertex> InVerts : register(t0);
RWStructuredBuffer<CoralInstance> OutInstances : register(u0);

void BuildOrthonormalBasis(float3 up, out float3 forward, out float3 left)
{
    float3 referenceAxis = float3(0.0f, 0.0f, 1.0f);

    if (abs(dot(up, referenceAxis)) > 0.999f)
    {
        referenceAxis = float3(1.0f, 0.0f, 0.0f);
    }

    forward = normalize(cross(referenceAxis, up));
    left = normalize(cross(up, forward));
}

float4x4 BuildWorldTransform(float3 position, float3 upNormal, uint seed)
{
    float3 forwardBase = float3(1.0, 0.0, 0.0);
    float3 leftBase = float3(0.0, 1.0, 0.0);
    BuildOrthonormalBasis(upNormal, forwardBase, leftBase);
    
    float yawRadians = GetRandomYawRadians(seed);
    float sineYaw = sin(yawRadians);
    float cosineYaw = cos(yawRadians);

    float3 forward =((forwardBase * cosineYaw) + (leftBase * sineYaw));
    float3 left = normalize(cross(upNormal, forward));
    forward = normalize(cross(left, upNormal));

    float uniformScale = 1.0f;

    float3 scaledForward = (forward * uniformScale);
    float3 scaledLeft = (left * uniformScale);
    float3 scaledUp = (upNormal * uniformScale);

    return float4x4(
    forward.x, left.x, upNormal.x, position.x,
    forward.y, left.y, upNormal.y, position.y,
    forward.z, left.z, upNormal.z, position.z,
    0.0f, 0.0f, 0.0f, 1.0f
);
}


[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    if (i >= vertexCount)
    {
        return;
    }

    VegetationVertex v = InVerts[i];
    uint seed = GetSeedFromId(id);
    float3 coralAdjustedPos = v.position + (-v.terrainNormal * 0.1f);
    
    CoralInstance instance;
    instance.worldTransform = BuildWorldTransform(coralAdjustedPos, v.terrainNormal, seed);
    instance.seed = seed;

    OutInstances[i] = instance;
}

