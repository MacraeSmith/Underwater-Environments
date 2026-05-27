#include "Data/Shaders/Utils/VegetationUtils.hlsli"
struct VegetationVertex
{
    float3 position;
    float pad;
    float3 terrainNormal;  
    uint pad2;
};


struct VegetationInstance
{
    float3 position;
    float yaw;
    float3 terrainNormal;
    uint   seed;
    float heightMultiplier;
};

cbuffer GrassCSConstants : register(b0)
{
    uint vertexCount;
    float3 pad;
};


StructuredBuffer<VegetationVertex> InVerts : register(t0);
RWStructuredBuffer<VegetationInstance> OutInstances : register(u0);

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    if (i >= vertexCount) { return; }

    VegetationVertex v = InVerts[i];
    
    

    VegetationInstance inst;
    float3 position = v.position;
    uint seed = GetSeedFromId(id);
    inst.position = position;
    inst.seed = seed;
    inst.yaw = GetRandomYawRadians(seed);
    inst.terrainNormal = v.terrainNormal;
    inst.heightMultiplier = GetHeightOffsetFrom2DPosition(position.xy, seed);
   

    OutInstances[i] = inst;
}



