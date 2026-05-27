//Generic Math and Color Helpers
//------------------------------------------------------------------------------------------------

float3 EncodeXYZToRGB(float3 vec)
{
    return (vec + 1.0) * 0.5;
}

float3 DecodeRGBtoXYZ(float3 rgb)
{
    return (rgb * 2.0) - 1.0;
}

float3 DiminishingAddColor(float3 normalizedColorA, float3 normalizedColorB)
{
    float3 colorOne = float3(1.0, 1.0, 1.0);
    return (colorOne - ((colorOne - normalizedColorA) * (colorOne - normalizedColorB)));
}

float RangeMap(float inValue, float inStart, float inEnd, float outStart, float outEnd)
{
    float fraction = (inValue - inStart) / (inEnd - inStart);
    float outValue = outStart + fraction * (outEnd - outStart);
    return outValue;
}

float RangeMapClamped(float inValue, float inStart, float inEnd, float outStart, float outEnd)
{
    float fraction = saturate((inValue - inStart) / (inEnd - inStart));
    float outValue = outStart + fraction * (outEnd - outStart);
    return outValue;
}

float GetFractionWithinRange(float inValue, float inStart, float inEnd)
{
    return (inValue - inStart) / (inEnd - inStart);
}

float SmoothStep3(float t)
{
    return t * t * (3.f - (2.f * t));
}

float SmoothStep5(float t)
{
    return t * t * t * (t * ((6.f * t) - 15.f) + 10.f);
}

float SmoothStop2(float t)
{
    float s = 1.f - t;
    return 1.f - (s * s);
}


float SmoothStop3(float t)
{
    float s = 1.f - t;
    return 1.f - (s * s * s);
}

float3 CrossProduct(float3 a, float3 b)
{
    return float3((a.y * b.z) - (a.z * b.y), (a.z * b.x) - (a.x * b.z), (a.x * b.y) - (a.y * b.x));
};

float3x3 Orthonormalize_IFwrd_JLeft_KUp(float3 iForward, float3 jLeft, float3 kUp)
{
    float3 kBasis = CrossProduct(iForward, jLeft);
    iForward = normalize(iForward);
    jLeft = normalize(jLeft);
    kUp = normalize(kBasis);
	
    return float3x3(iForward, jLeft, kUp);
};


float2 RotateVector2D(float2 vectorToRotate, float rotationRadians)
{
    float cosineRotation = cos(rotationRadians);
    float sineRotation = sin(rotationRadians);

    return float2(
        ((vectorToRotate.x * cosineRotation) - (vectorToRotate.y * sineRotation)),
        ((vectorToRotate.x * sineRotation) + (vectorToRotate.y * cosineRotation))
    );
}

float3 RotateAroundAxis(float3 v, float3 axis, float angleRad)
{
    axis = normalize(axis);
    float s = sin(angleRad);
    float c = cos(angleRad);
    return ((v * c) + (cross(axis, v) * s) + (axis * dot(axis, v) * (1.0f - c)));
}

float3 SafeNormalize3(float3 value, float3 fallbackValue)
{
    float lengthSq = dot(value, value);
    if (lengthSq > 0.000001f)
    {
        return (value * rsqrt(lengthSq));
    }
    return fallbackValue;
}
