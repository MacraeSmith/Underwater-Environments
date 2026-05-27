float Hash01(uint n)
{
    n = (n ^ 61u) ^ (n >> 16u);
    n *= 9u;
    n = n ^ (n >> 4u);
    n *= 0x27d4eb2du;
    n = n ^ (n >> 15u);
    // 24-bit mantissa-ish scaling
    return (float) (n & 0x00FFFFFFu) * (1.0f / 16777216.0f);
}

float GetRandomRotationAroundAxis(float3 axis, uint id)
{
    float3 helperAxis = (abs(axis.z) < 0.999f) ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
    float3 rightAxis = normalize(cross(helperAxis, axis));
    float3 forwardAxis = cross(axis, rightAxis);
    
    float yawRadians = (Hash01(id) * 6.28318530718f);

}


uint HashUint(uint value)
{
    // Simple integer hash with good avalanche for shader noise use.
    value ^= (value >> 16);
    value *= 0x7FEB352Du;
    value ^= (value >> 15);
    value *= 0x846CA68Bu;
    value ^= (value >> 16);
    return value;
}

uint Hash2DInt(int2 cell, uint seed)
{
    // Mix cell coords + seed into one hash.
    uint hx = HashUint((uint) cell.x ^ (seed * 0x9E3779B9u));
    uint hy = HashUint((uint) cell.y ^ (seed * 0x85EBCA6Bu));
    return HashUint(hx ^ (hy * 0xC2B2AE35u) ^ seed);
}

float FadeQuintic(float t)
{
    // 6t^5 - 15t^4 + 10t^3
    return (t * t * t) * ((t * ((t * 6.0) - 15.0)) + 10.0);
}

float2 FadeQuintic2(float2 t)
{
    return float2(FadeQuintic(t.x), FadeQuintic(t.y));
}

static const float2 kGradientDirections2D[8] =
{
    float2(1.0, 0.0),
    float2(-1.0, 0.0),
    float2(0.0, 1.0),
    float2(0.0, -1.0),
    normalize(float2(1.0, 1.0)),
    normalize(float2(-1.0, 1.0)),
    normalize(float2(1.0, -1.0)),
    normalize(float2(-1.0, -1.0))
};

float PerlinNoise2D(float2 position, uint seed)
{
    // Lattice cell and local position in cell
    int2 cell = int2(floor(position));
    float2 local = position - (float2) cell;

    // Fade for interpolation
    float2 fade = FadeQuintic2(local);

    // Pick gradients at 4 corners
    uint h00 = Hash2DInt(cell + int2(0, 0), seed);
    uint h10 = Hash2DInt(cell + int2(1, 0), seed);
    uint h01 = Hash2DInt(cell + int2(0, 1), seed);
    uint h11 = Hash2DInt(cell + int2(1, 1), seed);

    float2 g00 = kGradientDirections2D[(h00 & 7u)];
    float2 g10 = kGradientDirections2D[(h10 & 7u)];
    float2 g01 = kGradientDirections2D[(h01 & 7u)];
    float2 g11 = kGradientDirections2D[(h11 & 7u)];

    // Dot(gradient, offset-to-corner)
    float n00 = dot(g00, (local - float2(0.0, 0.0)));
    float n10 = dot(g10, (local - float2(1.0, 0.0)));
    float n01 = dot(g01, (local - float2(0.0, 1.0)));
    float n11 = dot(g11, (local - float2(1.0, 1.0)));

    // Bilinear interpolate with fade
    float nx0 = lerp(n00, n10, fade.x);
    float nx1 = lerp(n01, n11, fade.x);
    float nxy = lerp(nx0, nx1, fade.y);

    // This already lands roughly in [-1, 1].
    return nxy;
}

float GetHeightOffsetFrom2DPosition(float2 position, uint id)
{
    const float frequency = 0.05; // lower = bigger flowing patches

    // Sample noise
    float noiseValue = PerlinNoise2D(position * frequency, id);

    float normalizedNoise = ((noiseValue * 0.5) + 0.5);

    // Map [0,1] -> [0.5, 3.0]
    float heightMultiplier = lerp(0.25, 2.0, normalizedNoise);

    return heightMultiplier;
}

uint Scramble(uint n)
{
    n = n * 747796405u + 2891336453u;
    n = (n >> ((n >> 28u) + 4u)) ^ n;
    n = n * 277803737u;
    n = (n >> 22u) ^ n;
    return n;
}

uint GetSeedFromId(uint3 id)
{
    uint seed = ((id.x * 73856093u) ^ (id.y * 19349663u) ^ (id.z * 83492791u));
    return seed;
}

float GetRandomYawRadians(uint id)
{
    return (Hash01(Scramble(id)) * 6.28318530718f);
}

float GetRandomValueInRange(uint id, float min, float max)
{
    float rand01 = Hash01(Scramble(id));
    return lerp(min, max, rand01);

}

float3 GetArcSpineOffset(float3 N, float3 bendDirWS, float s, float theta)
{
    float thetaAbs = abs(theta);
    float thetaSafe = max(thetaAbs, 0.000001f);
    float invTheta = (1.0f / thetaSafe);

    float aGeneral = (sin(theta) * s * invTheta);
    float bGeneral = ((1.0f - cos(theta)) * s * invTheta);

    float3 offsetGeneral = ((N * aGeneral) + (bendDirWS * bGeneral));
    float3 offsetZero = (N * s);

    float useGeneral = step(0.000001f, thetaAbs);
    return lerp(offsetZero, offsetGeneral, useGeneral);
}

float3 GetArcSpineTangent(float3 N, float3 bendDirWS, float theta)
{
    float thetaAbs = abs(theta);
    float thetaSafe = max(thetaAbs, 0.000001f);
    float invTheta = (1.0f / thetaSafe);

    float aGeneral = (sin(theta) * invTheta);
    float bGeneral = ((1.0f - cos(theta)) * invTheta);

    float3 tangentGeneral = normalize((N * aGeneral) + (bendDirWS * bGeneral));
    float3 tangentZero = normalize(N);

    float useGeneral = step(0.000001f, thetaAbs);
    return lerp(tangentZero, tangentGeneral, useGeneral);
}


struct WindInfo
{
    float baseSpeed;

    float largeScale;
    float mediumScale;
    float smallScale;

    float largeSpeed;
    float mediumSpeed;
    float smallSpeed;
    
    float largeStrength;
    float mediumStrength;
    float smallStrength;

    float swirlStrength;
    float warbleStrength;
};

struct WindSample
{
    float3 worldDirection;
    float strength;
};

float NoiseSigned_Scalar(float2 p)
{
    float n = sin(dot(p, float2(0.8f, 1.3f))) + sin(dot(p, float2(-1.5f, 2.2f)));
    float n01 = saturate((n * 0.5f) + 0.5f);
    return (n01 * 2.0f) - 1.0f;
}

WindSample SampleUnderwaterCurrent(float3 worldPosWS, uint instanceId, float time, WindInfo windInfo)
{
    WindSample result;

    float timeSeconds = time * windInfo.baseSpeed;
    float2 worldXY = worldPosWS.xy;

    // ------------------------------------------------------------
    // Domain warp (creates "rushing" streaks)
    // ------------------------------------------------------------
    float warpX = NoiseSigned_Scalar((worldXY * 0.02f) + float2((timeSeconds * 0.20f), 0.0f));
    float warpY = NoiseSigned_Scalar((worldXY * 0.02f) + float2(0.0f, (timeSeconds * 0.20f)));
    float2 warpedXY = worldXY + (float2(warpX, warpY) * 15.0f);

    // ------------------------------------------------------------
    // Convert inverse scales (0..1, bigger means larger features) into frequency
    // Prevent frequency from hitting 0 and freezing the field.
    // ------------------------------------------------------------
    float largeFreq = max(0.001f, (1.0f - windInfo.largeScale));
    float mediumFreq = max(0.002f, (1.0f - windInfo.mediumScale));
    float smallFreq = max(0.004f, (1.0f - windInfo.smallScale));

    // ------------------------------------------------------------
    // Multi-scale angle noise (direction pertubations)
    // ------------------------------------------------------------
    float largeAngleNoise = NoiseSigned_Scalar((warpedXY * largeFreq) + (timeSeconds * windInfo.largeSpeed));
    float mediumAngleNoise = NoiseSigned_Scalar((warpedXY * mediumFreq) + (timeSeconds * windInfo.mediumSpeed));
    float smallAngleNoise = NoiseSigned_Scalar((warpedXY * smallFreq) + (timeSeconds * windInfo.smallSpeed));

    float combinedAngleNoise = (largeAngleNoise * windInfo.largeStrength) + (mediumAngleNoise * windInfo.mediumStrength) + (smallAngleNoise * windInfo.smallStrength);

    float combined01 = saturate((combinedAngleNoise * 0.5f) + 0.5f);
    float combinedSigned = (combined01 * 2.0f) - 1.0f;

    // ------------------------------------------------------------
    // Roaming base angle 
    // ------------------------------------------------------------
    float roamNoise = windInfo.largeStrength *  NoiseSigned_Scalar((warpedXY * (largeFreq * 0.25f)) + (timeSeconds * (windInfo.largeSpeed * 0.35f)));
    float roamAngleRadians = (roamNoise * 3.14159265f); // [-pi, +pi]

    // ------------------------------------------------------------
    // Warble allows deviation from base roam direction
    // ------------------------------------------------------------
    float warble01 = saturate(windInfo.warbleStrength);
    warble01 = (warble01 * warble01); // ease-in

    float maxDeviationRadians = lerp(0.35f, 2.2f, warble01); // ~20 deg .. ~126 deg
    float finalAngleRadians = roamAngleRadians + (combinedSigned * maxDeviationRadians);

    float2 windDir2D = float2(cos(finalAngleRadians), sin(finalAngleRadians));

    // ------------------------------------------------------------
    // Vertical swirl applies verticality to direction
    // ------------------------------------------------------------
    float verticalNoise = NoiseSigned_Scalar((warpedXY * 0.03f) + (timeSeconds * 0.10f));
    float vertical = (verticalNoise * windInfo.swirlStrength);

    float3 windDirWS = normalize(float3(windDir2D.x, windDir2D.y, vertical));

    // ------------------------------------------------------------
    // Strength (rushing: magnitude deviations) - allow values > 1 for surges
    // ------------------------------------------------------------
    float strengthLarge = NoiseSigned_Scalar((warpedXY * 0.01f) + (timeSeconds * 0.05f));
    float strengthSmall = NoiseSigned_Scalar((warpedXY * 0.06f) + (timeSeconds * 0.18f));
    float strengthNoiseCombined = (strengthLarge * 0.7f) + (strengthSmall * 0.3f);

    float strength01 = saturate((strengthNoiseCombined * 0.5f) + 0.5f);

    float instanceJitter = GetRandomValueInRange(instanceId, 0.85f, 1.15f);
    float finalStrength = (windInfo.baseSpeed * instanceJitter) * lerp(0.35f, 1.35f, strength01);
    finalStrength = clamp(finalStrength, 0.0f, 2.5f);

    result.worldDirection = windDirWS;
    result.strength = finalStrength;
    return result;
}