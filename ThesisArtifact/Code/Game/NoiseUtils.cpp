#include "Game/NoiseUtils.hpp"
#include "Game/VegetationUtils.hpp"
#include "Engine/Math/MathUtils.hpp"


std::string GetNoiseTypeNameFromType(NoiseType type)
{
	std::string noiseTypeName;
	switch (type)
	{
	case NoiseType::PERLIN:
		noiseTypeName = "perlin";
		break;
	case NoiseType::FRACTAL:
		noiseTypeName = "fractal";
		break;
	case NoiseType::RAW_NEG_ONE_TO_ONE:
		noiseTypeName = "rawNegOneToOne";
		break;
	case NoiseType::RAW_ZERO_TO_ONE:
		noiseTypeName = "rawZeroToOne";
		break;
	case NoiseType::RIDGED:
		noiseTypeName = "ridged";
		break;
	default:
		noiseTypeName = "perlin";
		break;
	}

	return noiseTypeName;
}

NoiseType GetNoiseTypeFromName(std::string const& name)
{
	if (name == "rawZeroToOne")
		return NoiseType::RAW_ZERO_TO_ONE;
	if (name == "negOneToOne")
		return NoiseType::RAW_NEG_ONE_TO_ONE;
	if (name == "fractal")
		return NoiseType::FRACTAL;
	if (name == "perlin")
		return NoiseType::PERLIN;
	if (name == "ridged")
		return NoiseType::RIDGED;

	return NoiseType::PERLIN;
}

std::string GetWorldNoiseTypeNameFromType(WorldNoiseType type)
{
	switch (type)
	{
	case WorldNoiseType::TERRAIN: return "Terrain";
	case WorldNoiseType::VEGETATION: return "Vegetation";
	default: return UN_INITIALIZED_WORLD_DATA_NAME;
	}
}

WorldNoiseType GetWorldNoiseTypeFromName(std::string const& name)
{
	if (name == "Terrain")
		return WorldNoiseType::TERRAIN;
	if (name == "Vegetation")
		return WorldNoiseType::VEGETATION;

	return WorldNoiseType::UN_INITIALIZED;
}

std::string GetTerrainNoiseTypeNameFromType(TerrainNoiseType type)
{
	switch (type)
	{
	case TerrainNoiseType::BASE_TERRAIN: return "Base Terrain";
	case TerrainNoiseType::CONTINENTALNESS: return "Continentalness";
	case TerrainNoiseType::EROSION: return "Erosion";
	case TerrainNoiseType::PEAKS_AND_VALLEYS: return "Peaks and Valleys";
	case TerrainNoiseType::TEMPERATURE: return "Temperature";
	default: return UN_INITIALIZED_WORLD_DATA_NAME;
	}
}

TerrainNoiseType GetTerrainNoiseTypeFromName(std::string const& name)
{
	if (name == "Base Terrain")
		return TerrainNoiseType::BASE_TERRAIN;
	if (name == "Continentalness")
		return TerrainNoiseType::CONTINENTALNESS;
	if (name == "Erosion")
		return TerrainNoiseType::EROSION;
	if (name == "Peaks and Valleys")
		return TerrainNoiseType::PEAKS_AND_VALLEYS;
	if (name == "Temperature")
		return TerrainNoiseType::TEMPERATURE;

	return TerrainNoiseType::UN_INITIALIZED;
}

std::string GetVegetationNoiseTypeNameFromType(VegetationNoiseType type)
{
	switch (type)
	{
	case VegetationNoiseType::SEA_GRASS: return "Sea Grass";
	case VegetationNoiseType::KELP: return "Kelp";
	case VegetationNoiseType::SEA_ANEMONE: return "Sea Anemone";
	case VegetationNoiseType::GRASS: return "Grass";
	case VegetationNoiseType::CORAL: return "Coral";
	}

	return UN_INITIALIZED_WORLD_DATA_NAME;
}

VegetationNoiseType GetVegetationNoiseTypeFromName(std::string const& name)
{
	if (name == "Sea Grass")
		return VegetationNoiseType::SEA_GRASS;
	if (name == "Kelp")
		return VegetationNoiseType::KELP;
	if (name == "Sea Anemone")
		return VegetationNoiseType::SEA_ANEMONE;
	if (name == "Grass")
		return VegetationNoiseType::GRASS;
	if (name == "Coral")
		return VegetationNoiseType::CORAL;
	return VegetationNoiseType::UN_INITIALIZED;
}


VegetationNoiseType GetVegetationNoiseTypeFromVegetationType(VegetationType type)
{
	switch (type)
	{
	case VegetationType::UN_INITIALIZED: return VegetationNoiseType::UN_INITIALIZED;
	case VegetationType::SEA_GRASS: return VegetationNoiseType::SEA_GRASS;
	case VegetationType::KELP: return VegetationNoiseType::KELP;
	case VegetationType::SEA_ANEMONE: return VegetationNoiseType::SEA_ANEMONE;
	case VegetationType::GRASS: return VegetationNoiseType::GRASS;
	case VegetationType::COUNT: return VegetationNoiseType::UN_INITIALIZED;
	default: return VegetationNoiseType::CORAL;
	}
}

float GetVegetationClusterMask(float vegetationNoise, VegetationNoiseType vegNoiseType)
{
	float threshold = 0.8f;
	float band = 0.1f;

	switch (vegNoiseType)
	{
	case VegetationNoiseType::SEA_GRASS:
	{
		threshold = 0.58f;
		band = 0.22f;
	}
	break;

	case VegetationNoiseType::KELP:
	{
		threshold = 0.28f;
		band = 0.12f;
	}
	break;

	case VegetationNoiseType::SEA_ANEMONE:
	{
		threshold = 0.75f;
		band = 0.025f;
	}
	break;

	case VegetationNoiseType::GRASS:
	{
		threshold = -0.7f;
		band = 0.1f;
	}
	break;

	case VegetationNoiseType::CORAL:
	{
		threshold = 0.2f;
		band = 0.12f;
	}
	break;
	default:
		break;
	}

	float start = (threshold - band);
	float end = (threshold + band);
	float t01 = GetClampedFractionWithinRange(vegetationNoise, start, end);
	return SmoothStep3(t01);
}
