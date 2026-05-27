#include "Data/Shaders/Utils/CommonUtils.hlsli"
#include "Data/Shaders/Utils/TextureUtils.hlsli"
#include "Data/Shaders/Utils/UnderwaterLightingUtils.hlsli"

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
};

cbuffer LightConstants : register(b4)
{
    float3 c_sunDirection;
    float c_sunIntensity;
    float3 c_sunColorRGB;
    float c_ambientIntensity;
    int c_numLights;
    float3 LIGHT_CONSTANTS_PADDING;	
}; //b4

#define NUM_TEXTURE_SLOTS 16
cbuffer TextureConstants : register(b5)
{
	uint4 textureIndexes[NUM_TEXTURE_SLOTS / 4];
};

#define FOG_INDEX_UNDERWATER 0
#define FOG_INDEX_ABOVE_WATER 1
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
    float c_globalSpeedScale;
    float c_globalCrestSteepnessScale;
    float c_globalAmplitudeScale;
    float c_globalWavelengthScale;
    
    float c_directionVarianceRegionScale;
    float c_waveDirectionVariance;
    float c_amplitudeVarianceRegionScale;
    float c_waveAmplitudeVariance;
    
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


cbuffer InverseCameraConstants : register(b13)
{
    float4x4 ClipToWorldTransform;
}

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

cbuffer DebugRenderConstants : register(b15)
{
    int c_default;
    int c_albedo;
    int c_normals;
    int c_ambientOcclusion;
    int c_lightStrength;
    int c_noNormals;
    int c_noAmbientOcclusion;
    int c_specularAttenuation;
    int c_noSpecularAttenuation;
    int c_sceneDepth;
    int c_emission;
    int c_biomeWeight1;
    int c_biomeWeight2;
    int c_biomeWeight3;
    int c_biomeWeight4;
    int c_strongestBiome;
    int c_fogVolumeMasks;
};

//Texture and Sampler constants
//------------------------------------------------------------------------------------------------
#define MAX_NUM_BINDLESS_TEXTURES 4096
Texture2D<float4> t_textures[MAX_NUM_BINDLESS_TEXTURES] : register(t0, space0);
SamplerState s_sampler : register(s0);

//Helper functions
//------------------------------------------------------------------------------------------------

float3 GetUnderwaterTint(float fogDepth)
{
    float depthScale = c_underWaterTintDepthScale;
    float shallowDepthMax = 10.f * depthScale;
    float shallow = saturate(1.0 - fogDepth / shallowDepthMax);
    float3 shallowColor = float3(0.0, 0.3, 0.5);
    float3 deepColor = float3(0.02, 0.1, 0.2);
    float3 scatterColor = lerp(deepColor, shallowColor, shallow);

    float3 absorption = float3(0.03, 0.01, 0.007); // how much color channels are absorbed
    float3 depthAttenuation = exp(-absorption * fogDepth * depthScale);
    return min(scatterColor + depthAttenuation, float3(1.0, 1.0, 1.0));
    //return (scatterColor + depthAttenuation);

}


bool IsUnderwater()
{
    return c_cameraPosition.z < c_seaLevel;
}

float Hash01(float3 input)
{
    uint x = asuint(input.x);
    uint y = asuint(input.y);
    uint z = asuint(input.z);

    uint seed = (x * 73856093u) ^ (y * 19349663u) ^ (z * 83492791u);

    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);

    return (float) (seed & 0x00FFFFFFu) * (1.0f / 16777216.0f);
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


//Pixel Shader
//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    uint group = 0;
    uint offset = 0;
    uint textureIndex = 0;

    GetTextureGroupAndOffset(textureIndex, group, offset);
    Texture2D<float4> sceneTex = t_textures[textureIndexes[group][offset]];
    float4 sceneColor = sceneTex.Sample(s_sampler, input.uv);
    if(c_debugInt != c_sceneDepth && c_debugInt != c_fogVolumeMasks && c_debugInt != c_default)
    {
        return sceneColor;
    }

    textureIndex = 1;
    GetTextureGroupAndOffset(textureIndex, group, offset);
    Texture2D<float4> depthTex = t_textures[textureIndexes[group][offset]];
    float rawDepth = depthTex.Sample(s_sampler, input.uv).r;

    if (c_debugInt == c_sceneDepth)
    {
        float debugDepth = LinearizeDepth(rawDepth, 0.1f, FAR_PLANE);
        return float4(debugDepth, debugDepth, debugDepth, 1.0f);
    }
    
    bool isUnderwater = IsUnderwater();
    
    FogData usedFogData = c_fogData[0];
    if(!isUnderwater)
    {
        usedFogData = c_fogData[1];

    }

    float depth = LinearizeDepth(rawDepth, 0.1f, usedFogData.depth);
    float worldDepth = (depth * usedFogData.depth);

    float mediumAmount = pow(depth, usedFogData.transitionExponent);
    mediumAmount = saturate(mediumAmount * usedFogData.opacity);

    float3 fogColor = usedFogData.color.rgb;

    float3 worldPosition = ReconstructWorldPosition(input.uv, rawDepth, ClipToWorldTransform);
    if (isUnderwater == 1)
    {
        float depthBelowSurface = max(0.0f, (c_seaLevel - worldPosition.z));
        float depth01 = saturate(depthBelowSurface / c_maxTerrainDepth);
        depth01 = smoothstep(0.0f, 1.0f, depth01);
        
        float3 tint = GetUnderwaterTint(worldDepth);
        float darkenRandom = RangeMap(Hash01(worldPosition), 0.f, 1.f, 0.95f, 1.f);
        float depthDarkening = lerp(1.0, 0.1f, depth01 * c_underwaterDepthDarkenScale);
        fogColor *= (tint * depthDarkening * darkenRandom);
        
        //base fog before light rays
        float transmittance = (1.0f - mediumAmount);
        float3 mediumScattering = (fogColor * mediumAmount);
        float3 finalRGB = ((sceneColor.rgb * transmittance) + mediumScattering);
        finalRGB = saturate(finalRGB);

        if (c_activeLightRays == 1 || c_activeParticles == 1)
        {
            textureIndex = 2;
            GetTextureGroupAndOffset(textureIndex, group, offset);
            Texture2D<float4> maskTexture = t_textures[textureIndexes[group][offset]];
            //float3 maskColor = BlurTexture(input.uv, maskTexture,s_sampler, 1.0, c_screenSize);
            float3 maskColor = GetBloomBlur(input.uv, maskTexture, s_sampler, c_screenSize, 6.f, 1.5f).rgb;            
            
            if (c_debugInt == c_fogVolumeMasks)
            {
                return float4(maskColor.rgb, 1.0f);
            }
            
            float3 shaftExtraScattering = float3(0.f.xxx);
            float3 shaftSurfaceGlow = 0.0f.xxx;
            

            if(c_activeLightRays == 1)
            {
                float lightRayAmount = maskColor.r;
                lightRayAmount = saturate(lightRayAmount);
                lightRayAmount = pow(lightRayAmount, 1.5f);
                lightRayAmount *= c_sunIntensity;

                float shaftInfluence = saturate(lightRayAmount); // * 1.15f);

                // Add a small shaft-specific scattering term.
                float shaftTintAmount = 0.5f;
                float3 shaftScatteringColor = lerp(fogColor, c_shaftColor.rgb, shaftTintAmount);
                float shaftScatteringStrength = 0.55f;
                shaftExtraScattering = (shaftScatteringColor * mediumAmount * shaftInfluence * shaftScatteringStrength);
                
                float3 viewDirection = normalize(worldPosition - c_cameraPosition);
                float viewDirectionZ = viewDirection.z;
                if (abs(viewDirectionZ) > 0.0001f)
                {
                    float surfaceIntersectionDistance = ((c_seaLevel - c_cameraPosition.z) / viewDirectionZ);

                    if (surfaceIntersectionDistance > 0.0f)
                    {
                        float surfaceGlowDistance = 500.0f;
                        float surfaceIntersectionFactor = (1.0f - saturate(surfaceIntersectionDistance / surfaceGlowDistance));
                        surfaceIntersectionFactor = pow(surfaceIntersectionFactor, 2.0f);

                        float surfaceGlowStrength = 0.5f;
                        shaftSurfaceGlow = (c_shaftColor.rgb * shaftInfluence * surfaceIntersectionFactor * surfaceGlowStrength);
                        float viewDotSunDirection = dot(viewDirection, c_sunDirection);
                        //viewDotSunDirection = smoothstep()

                        shaftSurfaceGlow *= 1.0 - saturate(viewDotSunDirection);
                        //return float4(shaftSurfaceGlow.xxx, 1.f);
                    }
                }
            }
            
            float3 particleScattering = 0.f.xxx;
            if(c_activeParticles == 1)
            {   
                float particleAmount = maskColor.g;
                particleAmount = pow(particleAmount, 1.35f);
                float particleInfluence = saturate(particleAmount);
                
                float3 particleColor = (fogColor * 1.15f);
                float particleStrengthMultiplier = lerp(1.f, 0.0f, depth01);
                float particleStrength = particleStrengthMultiplier * c_particleIntensity;
                particleScattering = (particleColor * particleInfluence * particleStrength);
            }
            
             
            mediumScattering += (shaftExtraScattering + shaftSurfaceGlow + particleScattering);
            transmittance = (1.0f - mediumAmount);
            finalRGB = ((sceneColor.rgb * transmittance) + mediumScattering);
            finalRGB = saturate(finalRGB);
        }
        

        float4 finalColor = float4(finalRGB, sceneColor.a);
        clip(finalColor.a - 0.01f);
        return finalColor;
    }
    
    else
    {   
        float pixelWorldHeightDiff = clamp(worldPosition.z - c_cameraPosition.z, 0.f, c_fogVolumeHeight);
        float fogVolumeMask = 1.0 - saturate(GetFractionWithinRange(pixelWorldHeightDiff, 0.f, c_fogVolumeHeight));
        mediumAmount *= fogVolumeMask;

        if (c_debugInt == c_fogVolumeMasks)
        {
            return float4(fogVolumeMask, fogVolumeMask, fogVolumeMask, 1.0f);
        }

        float transmittance = (1.0f - mediumAmount);
        float3 mediumScattering = (fogColor * mediumAmount);

        float3 finalRGB = ((sceneColor.rgb * transmittance) + mediumScattering);
        finalRGB = saturate(finalRGB);

        float4 finalColor = float4(finalRGB, sceneColor.a);
        clip(finalColor.a - 0.01f);
        return finalColor;
    }
}