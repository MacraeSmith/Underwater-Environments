
#include "Data/Shaders/Utils/UnderwaterLightingUtils.hlsli"
#include "Data/Shaders/Utils/TextureUtils.hlsli"

RWTexture3D<float3> g_volumeTexture : register(u0);

//--------------------------------------------------------------
// Volume generation inputs
//--------------------------------------------------------------
cbuffer FogVolumeConstants : register(b0)
{
    float4x4 ClipToWorldTransform;
    float3 c_cameraPosition;
    float c_seaLevel;
    float c_fogDepth;
    float3 c_sunDirection;
    float c_time;
    float3 padding;
};

//--------------------------------------------------------------
// Light shaft tuning inputs
//--------------------------------------------------------------
cbuffer LightRayConstants : register(b1)
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

cbuffer UnderwaterParticleConstants : register(b2)
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

//--------------------------------------------------------------
// Reconstruct a world-space point from screen UV and device depth
//--------------------------------------------------------------

//--------------------------------------------------------------
// Build a stable basis aligned to the light direction
//--------------------------------------------------------------
void BuildLightSpaceBasis(float3 lightForward, out float3 lightRight, out float3 lightUp)
{
    float3 referenceUp = float3(0.0f, 0.0f, 1.0f);

    if (abs(dot(lightForward, referenceUp)) > 0.98f)
    {
        referenceUp = float3(1.0f, 0.0f, 0.0f);
    }

    lightRight = normalize(cross(referenceUp, lightForward));
    lightUp = normalize(cross(lightForward, lightRight));
}

//--------------------------------------------------------------
// Slow animated UV warp used to keep the pattern alive
//--------------------------------------------------------------
float2 SampleDirectionWarp(float2 uv)
{
    float animationTime = (c_time * c_shaftWarpSpeed);

    float2 warpA = Noise2D(((uv * 1.2f) + float2((animationTime * 0.9f), (animationTime * 0.7f))));
    float2 warpB = Noise2D(((uv * 2.1f) - float2((animationTime * 0.6f), (animationTime * 0.8f))));
    float2 warpC = Noise2D(((uv * 3.5f) + float2(((-animationTime) * 1.3f), (animationTime * 1.1f))));

    warpA = ((warpA - 0.5f) * 2.0f);
    warpB = ((warpB - 0.5f) * 2.0f);
    warpC = ((warpC - 0.5f) * 2.0f);

    float2 combinedWarp = (warpA + (warpB * 0.6f) + (warpC * 0.3f));
    return combinedWarp;
}

//--------------------------------------------------------------
// Build the shaft pattern in light-space
//--------------------------------------------------------------
float LightShaftPattern(float3 sampleWorldPosition, float seaLevel)
{
    float pattern = 0.f;
    if (c_activeLightRays == 1)
    {
        float3 lightForward = normalize(c_sunDirection);
        float3 lightRight;
        float3 lightUp;
        BuildLightSpaceBasis(lightForward, lightRight, lightUp);

        float depthBelowSurface = max(0.0f, (seaLevel - sampleWorldPosition.z));

        float3 surfaceAnchorPosition = sampleWorldPosition;
        float lightDirectionZ = lightForward.z;
        if (abs(lightDirectionZ) > 0.0001f)
        {
            float distanceToSurfaceAlongLight = ((seaLevel - sampleWorldPosition.z) / lightDirectionZ);
            surfaceAnchorPosition = (sampleWorldPosition + (lightForward * distanceToSurfaceAlongLight));
        }
        else
        {
            surfaceAnchorPosition.z = seaLevel;
        }

        float3 surfaceLightSpacePosition;
        surfaceLightSpacePosition.x = dot(surfaceAnchorPosition, lightRight);
        surfaceLightSpacePosition.y = dot(surfaceAnchorPosition, lightUp);
        surfaceLightSpacePosition.z = dot(surfaceAnchorPosition, lightForward);

        float safeShaftScale = max((c_shaftScale * 20.0f), 0.0001f);
        float2 uv = (surfaceLightSpacePosition.xy / safeShaftScale);

        float2 warpOffset = (SampleDirectionWarp(uv) * c_shaftWarpStrength);
        uv += warpOffset;

        float coarsePattern = VoronoiEdgeSoft((uv * 0.12f), c_shaftSpread);
        coarsePattern = pow(coarsePattern, 3.5f);

        float shaftMask = smoothstep(c_shaftThreshold, min((c_shaftThreshold + (c_shaftSpread * 2.0f)), 1.0f), coarsePattern);

        float shaftBody = exp(((-depthBelowSurface) * c_shaftDepthFade * 0.1f));
        float intensity = lerp(0.0f, 200.0f, c_shaftIntensity);
        pattern = saturate(shaftMask * shaftBody * intensity);
    }

    return pattern;
}

float Hash31(float3 value)
{
    value = frac(value * 0.1031f);
    value += dot(value, value.yzx + 33.33f);
    return frac((value.x + value.y) * value.z);
}

float ParticleField(float3 sampleWorldPosition)
{
    float particlePattern = 0.f;
    if (c_activeParticles == 1)
    {
        float particleCellSize = lerp(1.0f, 10.0f, c_particleCellSize);
        float safeParticleCellSize = max(particleCellSize, 0.0001f);
        float inverseSafeParticleCellSize = (1.0f / safeParticleCellSize);

        float minParticleRadius = (c_particleSizeMin * 0.1f);
        float maxParticleRadius = (c_particleSizeMax * 0.1f);
        float edgeSoftness = 0.05f;
        float localMotionAmount = (0.18f * safeParticleCellSize);

        float maxCenterOffsetWorld = localMotionAmount;
        float maxInfluenceWorld = (maxParticleRadius + edgeSoftness + maxCenterOffsetWorld);

        int neighborSearchRadius = (int)ceil(maxInfluenceWorld / safeParticleCellSize);
        neighborSearchRadius = clamp(neighborSearchRadius, 0, 1);

        float3 baseCellPosition = (sampleWorldPosition * inverseSafeParticleCellSize);
        float3 baseCellIndex = floor(baseCellPosition);

        float3 sampleToCamera = (sampleWorldPosition - c_cameraPosition);
        float distanceFromCamera = length(sampleToCamera);

        float farFade = saturate(exp(((-distanceFromCamera) * c_particleDepthFade * 0.1f)));

        float nearFadeStart = 0.0f;
        float nearFadeEnd = 10.0f;
        float nearFade = smoothstep(nearFadeStart, nearFadeEnd, distanceFromCamera);

        float depthFade = (farFade * nearFade);
        if (depthFade > 0.0001f)
        {
            bool shouldExitEarly = false;
            float strongestParticle = 0.0f;
            float occupancyThreshold = (1.0f - saturate((c_particleDensity * 0.5f)));

            for (int zOffset = (-neighborSearchRadius); zOffset <= neighborSearchRadius; ++zOffset)
            {
                if(shouldExitEarly)
                {
                    break;
                }
                
                for (int yOffset = (-neighborSearchRadius); yOffset <= neighborSearchRadius; ++yOffset)
                {
                    if (shouldExitEarly)
                    {
                        break;
                    }
                    
                    for (int xOffset = (-neighborSearchRadius); xOffset <= neighborSearchRadius; ++xOffset)
                    {
                        if (shouldExitEarly)
                        {
                            break;
                        }
                        
                        float3 neighborCellIndex = (baseCellIndex + float3((float) xOffset, (float) yOffset, (float) zOffset));

                        float occupancyRandom = Hash31(neighborCellIndex);
                        if (occupancyRandom < occupancyThreshold)
                        {
                            continue;
                        }

                        for (uint particleIndex = 0; particleIndex < c_particlesPerCell; ++particleIndex)
                        {
                            float particleIndexAsFloat = (float) particleIndex;
                            float3 particleSeedBase = (neighborCellIndex + (particleIndexAsFloat * float3(97.0f, 173.0f, 263.0f)));

                            float hashA = Hash31((particleSeedBase + float3(13.1f, 17.2f, 19.3f)));
                            float hashB = Hash31((particleSeedBase + float3(23.7f, 29.1f, 31.4f)));
                            float hashC = Hash31((particleSeedBase + float3(41.3f, 43.7f, 47.9f)));

                            float3 particleCenterInCell = float3(hashA,frac(((hashB * 13.37f) + (hashA * 7.11f))),frac(((hashC * 17.91f) + (hashB * 5.73f))));

                            float sizeRandom = frac(((hashA * 11.13f) + (hashB * 17.17f) + (hashC * 23.23f)));
                            float particleRadius = lerp(minParticleRadius, maxParticleRadius, (sizeRandom * sizeRandom));


                            float particleOuterRadius = (particleRadius + edgeSoftness);

                            float3 randomMotionDirection = float3(((frac(((hashA * 19.73f) + (hashC * 3.17f))) * 2.0f) - 1.0f),((frac(((hashB * 23.51f) + (hashA * 5.29f))) * 2.0f) - 1.0f),
                                ((frac(((hashC * 29.19f) + (hashB * 7.41f))) * 2.0f) - 1.0f));

                            float speedRandom = frac(((hashA * 31.31f) + (hashB * 7.97f) + (hashC * 13.53f)));
                            float particleSpeed = ((c_particleDriftSpeed * 2.0f) * lerp(0.9f, 1.1f, speedRandom));

                            float phaseRandom = frac(((hashA * 5.27f) + (hashB * 37.61f) + (hashC * 19.19f)));
                            float animatedTime = ((c_time * particleSpeed) + (phaseRandom * 6.2831853f));

                            float motionWave = sin(animatedTime);
                            float3 localMotionOffset = (randomMotionDirection * motionWave * localMotionAmount);

                            float3 particleCenterWorldPosition = ((neighborCellIndex + particleCenterInCell) * safeParticleCellSize);
                            particleCenterWorldPosition += localMotionOffset;

                            float3 particleDeltaInCellSpace = ((sampleWorldPosition - particleCenterWorldPosition) * inverseSafeParticleCellSize);

                            float maxAxisDistance = max(abs(particleDeltaInCellSpace.x), max(abs(particleDeltaInCellSpace.y), abs(particleDeltaInCellSpace.z)));
                            if (maxAxisDistance > particleOuterRadius)
                            {
                                continue;
                            }

                            float distanceToCenterSquared = dot(particleDeltaInCellSpace, particleDeltaInCellSpace);
                            float particleRadiusSquared = (particleRadius * particleRadius);
                            float particleOuterRadiusSquared = (particleOuterRadius * particleOuterRadius);

                            if (distanceToCenterSquared > particleOuterRadiusSquared)
                            {
                                continue;
                            }

                            float normalizedDistance = saturate((distanceToCenterSquared - particleRadiusSquared) / max((particleOuterRadiusSquared - particleRadiusSquared), 0.0001f));
                            float softParticle = (1.0f - normalizedDistance);
                            softParticle = smoothstep(0.0f, 1.0f, softParticle);

                            float intensityRandom = frac(((hashA * 41.41f) + (hashB * 11.11f) + (hashC * 3.03f)));
                            intensityRandom *= intensityRandom * intensityRandom;
                            intensityRandom = floor(intensityRandom * 4.f) * 0.25f;
                    
                            float intensityVariation = lerp(0.1f, 1.5f, intensityRandom);

                            float particleContribution = (softParticle * depthFade * intensityVariation);
                            strongestParticle = max(strongestParticle, particleContribution);

                            if (strongestParticle >= 0.99f)
                            {
                                particlePattern = saturate(strongestParticle);
                                shouldExitEarly = true;
                            }
                        }
                    }
                }
            }
            
            if(!shouldExitEarly)
            {
                particlePattern = saturate(strongestParticle);
            }
        }
    }
   

    return particlePattern;
} 

//--------------------------------------------------------------
[numthreads(8, 8, 4)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    //----------------------------------------------------------
    // Read target volume size
    //----------------------------------------------------------
    uint volumeWidth = 0;
    uint volumeHeight = 0;
    uint volumeDepth = 0;
    g_volumeTexture.GetDimensions(volumeWidth, volumeHeight, volumeDepth);

    //----------------------------------------------------------
    // Guard against threads outside the valid volume bounds
    //----------------------------------------------------------
    if ((dispatchThreadID.x >= volumeWidth) ||
        (dispatchThreadID.y >= volumeHeight) ||
        (dispatchThreadID.z >= volumeDepth))
    {
        return;
    }

    //----------------------------------------------------------
    // Convert voxel coordinates into normalized volume space
    //----------------------------------------------------------
    float u = (((((float) dispatchThreadID.x) + 0.5f) / ((float) volumeWidth)));
    float v = (((((float) dispatchThreadID.y) + 0.5f) / ((float) volumeHeight)));
    float w = (((((float) dispatchThreadID.z) + 0.5f) / ((float) volumeDepth)));

    float2 screenUV = float2(u, v);

    //----------------------------------------------------------
    // Build the world-space ray that corresponds to this screen location
    //----------------------------------------------------------
    float3 farWorldPosition = ReconstructWorldPosition(screenUV, 1.0f, ClipToWorldTransform);
    float3 viewRayDirection = normalize((farWorldPosition - c_cameraPosition));

    //----------------------------------------------------------
    // March outward from the camera through the shaft volume
    //----------------------------------------------------------
    float distanceAlongRay = (w * c_fogDepth);
    float3 sampleWorldPosition = (c_cameraPosition + (viewRayDirection * distanceAlongRay));

    //----------------------------------------------------------
    // Default to no density
    //----------------------------------------------------------
    float density = 0.0f;
    float particleDensity = 0.0f;

    //----------------------------------------------------------
    // Only generate shafts below the water surface
    //----------------------------------------------------------
    if (sampleWorldPosition.z < c_seaLevel)
    {
        density = LightShaftPattern(sampleWorldPosition, c_seaLevel);
        particleDensity = ParticleField(sampleWorldPosition);
    }

    //----------------------------------------------------------
    // Clamp and store the result
    //----------------------------------------------------------
    density = saturate(density);
    float3 value = float3(density, particleDensity, 0.f);
    g_volumeTexture[dispatchThreadID] = value;
}

