#include "Game/BiomeUtils.hpp"
#include "Game/WorldUtils.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/Vertex_Terrain.hpp"

std::string GetBiomeNameFromType(Biome biome)
{
	switch (biome)
	{
	case Biome::SANDY_PLAINS: return "Sandy Plains";
	case Biome::ROCKY_SAND: return "Rocky Sand";
	case Biome::SMOOTH_ROCK: return "Smooth Rock";
	case Biome::MOSSY_ROCK: return "Mossy Rock";
	case Biome::BEACH: return "Beach";
	case Biome::GRASSY_BEACH: return "Grassy Beach";
	case Biome::GRASSY_SAND: return "Grassy Sand";
	case Biome::KELP_FOREST: return "Kelp Forest";
	case Biome::CORAL_GARDEN: return "Coral Garden";
	default: return UN_INITIALIZED_WORLD_DATA_NAME;
	}
}

Biome GetBiomeTypeFromName(std::string const& name)
{
	if (name == "Sandy Plains")
		return Biome::SANDY_PLAINS;
	if (name == "Rocky Sand")
		return Biome::ROCKY_SAND;
	if (name == "Smooth Rock")
		return Biome::SMOOTH_ROCK;
	if (name == "Mossy Rock")
		return Biome::MOSSY_ROCK;
	if (name == "Beach")
		return Biome::BEACH;
	if (name == "Grassy Beach")
		return Biome::GRASSY_BEACH;
	if (name == "Grassy Sand")
		return Biome::GRASSY_SAND;
	if (name == "Kelp Forest")
		return Biome::KELP_FOREST;
	if (name == "Coral Garden")
		return Biome::CORAL_GARDEN;


	ERROR_AND_DIE(Stringf("Could not recognize biome name: %s", name.c_str()));

	//return Biome::UN_INITIALIZED;
}

BiomeSplat BuildBiomeSplatFromScores(float const biomeScores[(int)Biome::NUM_BIOMES])
{
	BiomeSplat biomeSplat;

	Biome topBiomes[4] = { Biome::SANDY_PLAINS, Biome::SANDY_PLAINS, Biome::SANDY_PLAINS, Biome::SANDY_PLAINS };
	float topScores[4] = { 0.f, 0.f, 0.f, 0.f };

	for (int biomeIndex = 0; biomeIndex < (int)Biome::NUM_BIOMES; ++biomeIndex)
	{
		float score = biomeScores[biomeIndex];
		if (score <= 0.f)
		{
			continue;
		}

		if (score > topScores[0])
		{
			topScores[3] = topScores[2];
			topBiomes[3] = topBiomes[2];
			topScores[2] = topScores[1];
			topBiomes[2] = topBiomes[1];
			topScores[1] = topScores[0];
			topBiomes[1] = topBiomes[0];
			topScores[0] = score;
			topBiomes[0] = (Biome)biomeIndex;
		}
		else if (score > topScores[1])
		{
			topScores[3] = topScores[2];
			topBiomes[3] = topBiomes[2];
			topScores[2] = topScores[1];
			topBiomes[2] = topBiomes[1];
			topScores[1] = score;
			topBiomes[1] = (Biome)biomeIndex;
		}
		else if (score > topScores[2])
		{
			topScores[3] = topScores[2];
			topBiomes[3] = topBiomes[2];
			topScores[2] = score;
			topBiomes[2] = (Biome)biomeIndex;
		}
		else if (score > topScores[3])
		{
			topScores[3] = score;
			topBiomes[3] = (Biome)biomeIndex;
		}
	}

	float weightSum = (topScores[0] + topScores[1] + topScores[2] + topScores[3]);
	if (weightSum <= 0.f)
	{
		biomeSplat.m_biomes[0] = Biome::SANDY_PLAINS;
		biomeSplat.m_weights[0] = 1.f;
		return biomeSplat;
	}

	float inverseWeightSum = (1.f / weightSum);
	for (int i = 0; i < 4; ++i)
	{
		biomeSplat.m_biomes[i] = topBiomes[i];
		biomeSplat.m_weights[i] = (topScores[i] * inverseWeightSum);
	}

	return biomeSplat;
}

Vertex_Terrain GetTerrainVertFromBiomeSplat(Vec3 const& pos, Vec3 const& normal, BiomeSplat const& splat)
{
	Vertex_Terrain newVert;
	uint8_t packedWeight0 = newVert.QuantizeWeightToByte(splat.m_weights[0]);
	uint8_t packedWeight1 = newVert.QuantizeWeightToByte(splat.m_weights[1]);
	uint8_t packedWeight2 = newVert.QuantizeWeightToByte(splat.m_weights[2]);
	uint8_t packedWeight3 = newVert.QuantizeWeightToByte(splat.m_weights[3]);
	int packedSum = (int)packedWeight0 + (int)packedWeight1 + (int)packedWeight2 + (int)packedWeight3;
	int correction = (255 - packedSum);

	if (correction != 0)
	{
		int largestIndex = 0;
		int largestValue = (int)packedWeight0;

		if ((int)packedWeight1 > largestValue) { largestIndex = 1; largestValue = (int)packedWeight1; }
		if ((int)packedWeight2 > largestValue) { largestIndex = 2; largestValue = (int)packedWeight2; }
		if ((int)packedWeight3 > largestValue) { largestIndex = 3; largestValue = (int)packedWeight3; }

		if (largestIndex == 0) { packedWeight0 = (uint8_t)GetClampedInt((int)packedWeight0 + correction, 0, 255); }
		if (largestIndex == 1) { packedWeight1 = (uint8_t)GetClampedInt((int)packedWeight1 + correction, 0, 255); }
		if (largestIndex == 2) { packedWeight2 = (uint8_t)GetClampedInt((int)packedWeight2 + correction, 0, 255); }
		if (largestIndex == 3) { packedWeight3 = (uint8_t)GetClampedInt((int)packedWeight3 + correction, 0, 255); }
	}

	newVert.m_position = pos;
	newVert.m_normal = normal;

	newVert.m_splatIndex0 = (uint8_t)splat.m_biomes[0];
	newVert.m_splatIndex1 = (uint8_t)splat.m_biomes[1];
	newVert.m_splatIndex2 = (uint8_t)splat.m_biomes[2];
	newVert.m_splatIndex3 = (uint8_t)splat.m_biomes[3];

	newVert.m_splatWeight0 = packedWeight0;
	newVert.m_splatWeight1 = packedWeight1;
	newVert.m_splatWeight2 = packedWeight2;
	newVert.m_splatWeight3 = packedWeight3;

	return newVert;
}

BiomeSplat MakeTwoBiomeSplat(Biome a, Biome b, float t)
{
	BiomeSplat result;
	result.m_biomes[0] = a;
	result.m_weights[0] = (1.f - t);
	result.m_biomes[1] = b;
	result.m_weights[1] = t;
	return result;
}

float GetCoralRegionMask(float coralNoise, float oceanDepthMeters)
{
	float coralNoiseMask = SmoothStep3(GetClampedFractionWithinRange(coralNoise, 0.60f, 0.80f));
	float coralDepthMask = SmoothStep3(GetClampedFractionWithinRange(oceanDepthMeters, 4.0f, 10.0f));

	return (coralNoiseMask * coralDepthMask);
}

float GetKelpRegionMask(float kelpNoise, float temperature, float oceanDepthMeters)
{
	float kelpNoiseMask = SmoothStep3(GetClampedFractionWithinRange(kelpNoise, 0.60f, 0.80f));
	float kelpTemperatureMask = SmoothStep3(GetClampedFractionWithinRange(temperature, 0.55f, 0.85f));
	float kelpDepthMask = SmoothStep3(GetClampedFractionWithinRange(oceanDepthMeters, 3.0f, 12.0f));

	return (kelpNoiseMask * kelpTemperatureMask * kelpDepthMask);
}
