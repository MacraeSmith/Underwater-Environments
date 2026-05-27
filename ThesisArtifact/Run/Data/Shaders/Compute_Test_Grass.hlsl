
struct VegetationVertex
{
    float3 position;
    uint pad;
    float3 normal;  
    uint pad2;
};

// ------------------------------------------------------------
// Instance output from compute, consumed by graphics
// 32-byte aligned for comfort.
// ------------------------------------------------------------
struct GrassInstance
{
    float3 position;
    uint pad1;
    float3 normal;
    uint   pad2;
};

StructuredBuffer<VegetationVertex> InVerts : register(t0);
RWStructuredBuffer<GrassInstance> OutInstances : register(u0);

cbuffer GrassCSConstants : register(b0)
{
    uint  vertexCount;
    float bladeHeight;   // optional: can be used in VS instead
    float bladeWidth;    // optional: can be used in VS instead
    uint  seedBase;
};

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    if (i >= vertexCount) { return; }

    VegetationVertex v = InVerts[i];

    GrassInstance inst;
    float3 position = v.position;
    //position.z = 610.f;
    inst.position = position;
    //inst.position.x = 400.f;
    inst.pad1 = 0u;
    inst.normal   = normalize(v.normal);
    inst.pad2 = 0u;
   

    OutInstances[i] = inst;
}

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
};

struct v2p_t
{
    float4 position : SV_Position;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD;
};

#define MAX_NUM_BINDLESS_BUFFERS 4096
#define NUM_BUFFER_SLOTS 16
cbuffer BufferConstants : register(b6)
{
    uint4 bufferIndexes[NUM_BUFFER_SLOTS / 4];
}
StructuredBuffer<GrassInstance> instanceBuffers[MAX_NUM_BINDLESS_BUFFERS] : register(t0, space1);

cbuffer CameraConstants : register(b2)
{
    float4x4 c_worldToCameraTransform;
    float4x4 c_cameraToRenderTransform;
    float4x4 c_renderToClipTransform;
    float3   c_cameraPosition;
    float    pad1;
};

cbuffer ModelConstants : register(b3)
{
	float4x4 c_modelToWorldTransform;		// Model transform
	float4 c_modelColor;
};

uint GetGrassBufferIndex(in uint bufferIndex)
{	
	uint group = bufferIndex / 4;
	uint offset = bufferIndex % 4;
    uint index = bufferIndexes[group][offset];
    return index;
}

v2p_t VertexMain(vs_input_t input, uint instanceId : SV_InstanceID)
{
    
    float bladeHeightScale = 1.0;
    float bladeWidthScale = 1.0;

    
    uint index = GetGrassBufferIndex(0);
    GrassInstance inst = instanceBuffers[index][instanceId];


    // Make them bigger so you can see them.


    float3 N = normalize(inst.normal);
    //float3 N = float3(0.0,0.0,1.0);
    float3 helper = (abs(N.z) < 0.999f) ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
    float3 T = normalize(cross(helper, N));
    float3 B = normalize(cross(N, T));

    // Blade local vertex position (your grass triangle)
    float3 local = input.modelPosition;

    // Rotate local into world using (T,B,N) basis
    float3 worldOffset = (T * local.x) + (B * local.y) + (N * local.z);

    // Place at instance position
    float3 worldPos3 = (inst.position + worldOffset);
    //float3 worldPos3 = basePos + worldOffset;
    //float3 worldPos3 = finalPos + worldOffset;

    float4 worldPos = float4(worldPos3, 1.0f);
 
    float4 cameraPos = mul(c_worldToCameraTransform, worldPos);
    float4 renderPos = mul(c_cameraToRenderTransform, cameraPos);
    float4 clipPos   = mul(c_renderToClipTransform, renderPos);

    v2p_t o;
    o.position = clipPos;
    o.color = input.color * c_modelColor;
    o.uv = input.uv;
    return o;

}

float4 PixelMain(v2p_t input) : SV_Target0
{
    return input.color;
}

