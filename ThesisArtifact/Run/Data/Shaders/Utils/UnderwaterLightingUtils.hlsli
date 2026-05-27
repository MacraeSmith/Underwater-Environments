float3 Fade(float3 t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float Hash3D(int x, int y, int z)
{
    int n = x * 15731 + y * 789221 + z * 1376312589;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

float2 Hash22(float2 p)
{
    // integer-based hash, continuous across tile boundaries
    const float2 k = float2(0.3183099, 0.3678794);
    p = p - floor(p * k.yx) * 17.0;
    float h = frac(123.34 * sin(dot(p, k)));
    return frac(float2(h, h * 34.3 + 1.0));
}

float Grad(int hash, float3 p)
{
    int h = hash & 15;
    float3 grad = float3(
        (h & 1) == 0 ? p.x : -p.x,
        (h & 2) == 0 ? p.y : -p.y,
        (h & 4) == 0 ? p.z : -p.z
    );
    return grad.x + grad.y + grad.z;
}

float PerlinNoise3D(float3 p)
{
    float3 P0 = floor(p);
    float3 Pf = frac(p);

    float3 fade = Fade(Pf);

    int xi = (int) P0.x;
    int yi = (int) P0.y;
    int zi = (int) P0.z;

    float n000 = Grad(Hash3D(xi, yi, zi), Pf - float3(0, 0, 0));
    float n100 = Grad(Hash3D(xi + 1, yi, zi), Pf - float3(1, 0, 0));
    float n010 = Grad(Hash3D(xi, yi + 1, zi), Pf - float3(0, 1, 0));
    float n110 = Grad(Hash3D(xi + 1, yi + 1, zi), Pf - float3(1, 1, 0));
    float n001 = Grad(Hash3D(xi, yi, zi + 1), Pf - float3(0, 0, 1));
    float n101 = Grad(Hash3D(xi + 1, yi, zi + 1), Pf - float3(1, 0, 1));
    float n011 = Grad(Hash3D(xi, yi + 1, zi + 1), Pf - float3(0, 1, 1));
    float n111 = Grad(Hash3D(xi + 1, yi + 1, zi + 1), Pf - float3(1, 1, 1));

    // Trilinear interpolation
    float nx00 = lerp(n000, n100, fade.x);
    float nx10 = lerp(n010, n110, fade.x);
    float nx01 = lerp(n001, n101, fade.x);
    float nx11 = lerp(n011, n111, fade.x);

    float nxy0 = lerp(nx00, nx10, fade.y);
    float nxy1 = lerp(nx01, nx11, fade.y);

    float nxyz = lerp(nxy0, nxy1, fade.z);

    // Output normalized to roughly [-1, 1]
    return nxyz;
}

float2 Noise2D(float2 p)
{
    // Smooth pseudo-random 2D noise using sine interference
    float n = sin(dot(p, float2(0.8, 1.3))) + sin(dot(p, float2(-1.5, 2.2)));
    return float2(sin(n * 2.3), cos(n * 1.7));
}

float VoronoiEdgeSoft(float2 p, float lineThickness)
{
    p *= 8.0f;

    float2 i = floor(p);
    float2 f = frac(p);

    float m1 = 1e9, m2 = 1e9, m3 = 1e9;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 g = float2(x, y);
            float2 cell = i + g;
            float2 offset = Hash22(cell); //frac(sin(dot(cell, float2(27.619, 57.583))) * 43758.5453);
            float2 r = g + offset - f;
            float d = dot(r, r);

            if (d < m1)
            {
                m3 = m2;
                m2 = m1;
                m1 = d;
            }
            else if (d < m2)
            {
                m3 = m2;
                m2 = d;
            }
            else if (d < m3)
            {
                m3 = d;
            }
        }

    // Use distance difference and secondary falloff to broaden bright areas
    float edge = (m2 - m1) * 3.0f; // smaller = thicker lines
    float softness = saturate(exp(-edge / max(lineThickness, 0.0001f)));

    // Optional: also blend in some density from the third site for added volume
    float thirdBlend = saturate(exp(-m3 * 4.0f));
    return saturate(softness * 0.8f + thirdBlend * 0.2f);
}

float mod289(float x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}
float2 mod289(float2 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}
float3 mod289(float3 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}
float3 permute(float3 x)
{
    return mod289(((x * 34.0) + 1.0) * x);
}
float snoise(float2 v)
{
    const float4 C = float4(0.211324865405187, // (3.0 - sqrt(3.0)) / 6.0
	                        0.366025403784439, // 0.5 * (sqrt(3.0) - 1.0)
	                        -0.577350269189626, // -1.0 + 2.0 * C.x
	                        0.024390243902439); // 1.0 / 41.0

	// First corner
    float2 i = floor(v + dot(v, C.yy));
    float2 x0 = v - i + dot(i, C.xx);

	// Determine which simplex cell we’re in
    float2 i1 = (x0.x > x0.y) ? float2(1.0, 0.0) : float2(0.0, 1.0);

	// Offsets for remaining corners
    float2 x1 = x0 - i1 + C.xx;
    float2 x2 = x0 + C.zz;

	// Permutations
    i = mod289(i);
    float3 p = permute(permute(
		i.y + float3(0.0, i1.y, 1.0))
		+ i.x + float3(0.0, i1.x, 1.0));

	// Gradients
    float3 x_ = frac(p * C.w) * 2.0 - 1.0;
    float3 h = abs(x_) - 0.5;
    float3 ox = floor(x_ + 0.5);
    float3 a0 = x_ - ox;

	// Normalized gradients
    float2 g0 = float2(a0.x, h.x);
    float2 g1 = float2(a0.y, h.y);
    float2 g2 = float2(a0.z, h.z);

    float3 t = 0.5 - float3(dot(x0, x0), dot(x1, x1), dot(x2, x2));

    float3 n;
    n.x = (t.x < 0.0) ? 0.0 : pow(t.x, 4.0) * dot(g0, x0);
    n.y = (t.y < 0.0) ? 0.0 : pow(t.y, 4.0) * dot(g1, x1);
    n.z = (t.z < 0.0) ? 0.0 : pow(t.z, 4.0) * dot(g2, x2);

	// Final noise value
    return 70.0 * dot(n, float3(1.0, 1.0, 1.0));
}

float hash(float n)
{
    return frac(sin(n) * 43758.5453); // returns 0..1
}

