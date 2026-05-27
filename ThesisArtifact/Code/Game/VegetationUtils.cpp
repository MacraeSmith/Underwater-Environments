#include "Game/VegetationUtils.hpp"
#include "Game/NoiseUtils.hpp"
#include "Game/BiomeUtils.hpp"
#include "Engine/Core/Vertex_Terrain.hpp"
#include "Engine/Math/MathUtils.hpp"

VegetationType GetVegetationTypeFromName(std::string const& name)
{
	if (name == "Sea Grass") return VegetationType::SEA_GRASS;
	if (name == "Kelp") return VegetationType::KELP;
	if (name == "Grass") return VegetationType::GRASS;
	if (name == "Sea Anemone") return VegetationType::SEA_ANEMONE;

	if (name == "Coral Mushroom") return VegetationType::CORAL_MUSHROOM;

	if (name == "Coral Blue Mushroom 01") return VegetationType::CORAL_BLUE_MUSHROOM_01;
	if (name == "Coral Blue Mushroom 02") return VegetationType::CORAL_BLUE_MUSHROOM_02;

	if (name == "Coral Elk Horn") return VegetationType::CORAL_ELKHORN;
	if (name == "Coral Brain") return VegetationType::CORAL_BRAIN;

	if (name == "Coral Tube Sponge 01") return VegetationType::CORAL_TUBE_SPONGE_01;
	if (name == "Coral Tube Sponge 02") return VegetationType::CORAL_TUBE_SPONGE_02;
	if (name == "Coral Tube Sponge 03") return VegetationType::CORAL_TUBE_SPONGE_03;

	if (name == "Coral Lettuce 01") return VegetationType::CORAL_LETTUCE_01;
	if (name == "Coral Lettuce 02") return VegetationType::CORAL_LETTUCE_02;
	if (name == "Coral Lettuce 03") return VegetationType::CORAL_LETTUCE_03;

	if (name == "Coral Sea Fan 01") return VegetationType::CORAL_SEA_FAN_01;
	if (name == "Coral Sea Fan 02") return VegetationType::CORAL_SEA_FAN_02;

	if (name == "Coral Branching 01") return VegetationType::CORAL_BRANCHING_01;
	if (name == "Coral Branching 02") return VegetationType::CORAL_BRANCHING_02;

	if (name == "Coral Pine Cone 01") return VegetationType::CORAL_PINE_CONE_01;
	if (name == "Coral Pine Cone 02") return VegetationType::CORAL_PINE_CONE_02;

	if (name == "Coral Squash") return VegetationType::CORAL_SQUASH;

	if (name == "Coral Stub") return VegetationType::CORAL_STUB;

	return VegetationType::UN_INITIALIZED;
}

std::string GetVegetationTypeNameFromType(VegetationType type)
{
	switch (type)
	{
	case VegetationType::SEA_GRASS: return "Sea Grass";
	case VegetationType::KELP: return "Kelp";
	case VegetationType::GRASS: return "Grass";
	case  VegetationType::SEA_ANEMONE: return "Sea Anemone";
	case VegetationType::CORAL_MUSHROOM: return "Coral Mushroom";
	case VegetationType::CORAL_BLUE_MUSHROOM_01: return"Coral Blue Mushroom 01";
	case VegetationType::CORAL_BLUE_MUSHROOM_02: return"Coral Blue Mushroom 02";
	case VegetationType::CORAL_ELKHORN:  return "Coral Elk Horn";
	case VegetationType::CORAL_BRAIN:  return "Coral Brain";
	case VegetationType::CORAL_TUBE_SPONGE_01:  return "Coral Tube Sponge 01";
	case VegetationType::CORAL_TUBE_SPONGE_02:  return "Coral Tube Sponge 02";
	case VegetationType::CORAL_TUBE_SPONGE_03:  return "Coral Tube Sponge 03";
	case VegetationType::CORAL_LETTUCE_01:  return "Coral Lettuce 01";
	case VegetationType::CORAL_LETTUCE_02:  return "Coral Lettuce 02";
	case VegetationType::CORAL_LETTUCE_03:  return "Coral Lettuce 03";
	case VegetationType::CORAL_SEA_FAN_01:  return "Coral Sea Fan 01";
	case VegetationType::CORAL_SEA_FAN_02:  return "Coral Sea Fan 02";
	case VegetationType::CORAL_BRANCHING_01:  return "Coral Branching 01";
	case VegetationType::CORAL_BRANCHING_02:  return "Coral Branching 02";
	case VegetationType::CORAL_PINE_CONE_01: return "Coral Pine Cone 01";
	case VegetationType::CORAL_PINE_CONE_02: return "Coral Pine Cone 02";
	case VegetationType::CORAL_SQUASH: return "Coral Squash";
	case VegetationType::CORAL_STUB:  return "Coral Stub";
	default: return UN_INITIALIZED_WORLD_DATA_NAME;

	}
}


bool IsVegetationTypeCoral(VegetationType type)
{
	return (type >= VegetationType::CORAL_MUSHROOM && type <= VegetationType::CORAL_STUB);
}

bool WasVegetationTypeOBJLoaded(VegetationType type)
{
	if (type == VegetationType::KELP)
	{
		return true;
	}

	return (type >= VegetationType::CORAL_MUSHROOM && type <= VegetationType::CORAL_STUB);
}

bool DoesVegetationTypeHaveMaskedTexture(VegetationType type)
{
	switch (type)
	{

	case VegetationType::KELP: return true;
	case VegetationType::CORAL_BLUE_MUSHROOM_01: return true;
	case VegetationType::CORAL_BLUE_MUSHROOM_02: return true;
	case VegetationType::CORAL_SEA_FAN_01: return true;
	case VegetationType::CORAL_SEA_FAN_02: return true;
	}

	return false;
}

bool IsVegetationTypeTwoSided(VegetationType type)
{
	switch (type)
	{
	case VegetationType::SEA_GRASS: return true;
	case VegetationType::KELP: return true;
	case VegetationType::GRASS: return true;
	case VegetationType::CORAL_MUSHROOM:
		break;
	case VegetationType::CORAL_BLUE_MUSHROOM_01: return true;
	case VegetationType::CORAL_BLUE_MUSHROOM_02: return true;
	case VegetationType::CORAL_SEA_FAN_01: return true;
	case VegetationType::CORAL_SEA_FAN_02: return true;
	}

	return false;
}

float GetVegetationPotential(float temperature)
{
	float const minTemperature01 = 0.2f;   // below this, no vegetation
	float const fullTemperature01 = 1.f;  // above this, full vegetation allowed

	float t01 = GetFractionWithinRange(temperature, minTemperature01, fullTemperature01);

	return SmoothStep3(t01);
}


void AccumulateBiomeVegetationWeights(Biome biome, float biomeSplatWeight, float* vegetationWeights)
{
	if(biomeSplatWeight <= 0.2f)
		return;

	switch (biome)
	{
	case Biome::BEACH:
		break;

	case Biome::GRASSY_BEACH:
		vegetationWeights[(int)VegetationType::GRASS] += (1.00f * biomeSplatWeight);
		break;

	case Biome::SANDY_PLAINS:
		vegetationWeights[(int)VegetationType::SEA_GRASS] += (0.5f * biomeSplatWeight);
		vegetationWeights[(int)VegetationType::SEA_ANEMONE] += (0.1f * biomeSplatWeight);
		break;

	case Biome::GRASSY_SAND:
		vegetationWeights[(int)VegetationType::SEA_GRASS] += (3.00f * biomeSplatWeight);
		break;

	case Biome::ROCKY_SAND:
		vegetationWeights[(int)VegetationType::SEA_ANEMONE] += (0.2f * biomeSplatWeight);
		vegetationWeights[(int)VegetationType::KELP] += (0.20f * biomeSplatWeight);
		break;

	case Biome::KELP_FOREST:
		vegetationWeights[(int)VegetationType::KELP] += (3.00f * biomeSplatWeight);
		vegetationWeights[(int)VegetationType::SEA_GRASS] += (0.45f * biomeSplatWeight);
		break;

	case Biome::CORAL_GARDEN:
	{
		float const coralFamilyWeight = (1.00f * biomeSplatWeight);
		constexpr float mushroomWeight = 0.4f;
		vegetationWeights[(int)VegetationType::CORAL_MUSHROOM] += (mushroomWeight * coralFamilyWeight);
		vegetationWeights[(int)VegetationType::CORAL_BLUE_MUSHROOM_01] += (mushroomWeight * coralFamilyWeight);
		vegetationWeights[(int)VegetationType::CORAL_BLUE_MUSHROOM_02] += (mushroomWeight * coralFamilyWeight);

		vegetationWeights[(int)VegetationType::CORAL_ELKHORN] += coralFamilyWeight;
		vegetationWeights[(int)VegetationType::CORAL_BRAIN] += coralFamilyWeight;

		constexpr float tubeSpongeWeight = 0.33f;
		vegetationWeights[(int)VegetationType::CORAL_TUBE_SPONGE_01] += (tubeSpongeWeight * coralFamilyWeight);
		vegetationWeights[(int)VegetationType::CORAL_TUBE_SPONGE_02] += (tubeSpongeWeight * coralFamilyWeight);
		vegetationWeights[(int)VegetationType::CORAL_TUBE_SPONGE_03] += (tubeSpongeWeight * coralFamilyWeight);

		constexpr float lettuceWeight = 0.33f;
		vegetationWeights[(int)VegetationType::CORAL_LETTUCE_01] += (lettuceWeight * coralFamilyWeight);
		vegetationWeights[(int)VegetationType::CORAL_LETTUCE_02] += (lettuceWeight * coralFamilyWeight);
		vegetationWeights[(int)VegetationType::CORAL_LETTUCE_03] += (lettuceWeight * coralFamilyWeight);

		constexpr float pineConeWeight = 0.25f;
		vegetationWeights[(int)VegetationType::CORAL_PINE_CONE_01] += (pineConeWeight * coralFamilyWeight);
		vegetationWeights[(int)VegetationType::CORAL_PINE_CONE_02] += (pineConeWeight * coralFamilyWeight);

		constexpr float squashWeight = 0.33f;
		vegetationWeights[(int)VegetationType::CORAL_SQUASH] += (squashWeight * coralFamilyWeight);

		constexpr float seaFanWeight = 0.75f;
		vegetationWeights[(int)VegetationType::CORAL_SEA_FAN_01] += (seaFanWeight * coralFamilyWeight);
		vegetationWeights[(int)VegetationType::CORAL_SEA_FAN_02] += (seaFanWeight * coralFamilyWeight);

		constexpr float branchingWeight = 0.33f;
		vegetationWeights[(int)VegetationType::CORAL_BRANCHING_01] += (branchingWeight * coralFamilyWeight);
		vegetationWeights[(int)VegetationType::CORAL_BRANCHING_02] += (branchingWeight * coralFamilyWeight);

		constexpr float stubWeight = 1.f;
		vegetationWeights[(int)VegetationType::CORAL_STUB] += (stubWeight * coralFamilyWeight);
		break;
	}

	case Biome::MOSSY_ROCK:
		vegetationWeights[(int)VegetationType::KELP] += (0.58f * biomeSplatWeight);
		break;

	case Biome::SMOOTH_ROCK:
		vegetationWeights[(int)VegetationType::SEA_ANEMONE] += (0.20f * biomeSplatWeight);
		break;

	default:
		break;
	}
}

float GetVegetationWeightsFromBiomeSplat(BiomeSplat const& biomeSplat, float* vegetationWeights)
{
	for (int vegetationTypeIndex = 0; vegetationTypeIndex < (int)VegetationType::COUNT; ++vegetationTypeIndex)
	{
		vegetationWeights[vegetationTypeIndex] = 0.0f;
	}

	for (int biomeIndex = 0; biomeIndex < 4; ++biomeIndex)
	{
		float biomeSplatWeight = biomeSplat.m_weights[biomeIndex];
		if (biomeSplatWeight <= 0.0f)
		{
			continue;
		}

		Biome biome = biomeSplat.m_biomes[biomeIndex];
		AccumulateBiomeVegetationWeights(biome, biomeSplatWeight, vegetationWeights);
	}

	float totalWeight = 0.0f;
	for (int vegetationTypeIndex = 0; vegetationTypeIndex < (int)VegetationType::COUNT; ++vegetationTypeIndex)
	{
		totalWeight += vegetationWeights[vegetationTypeIndex];
	}

	return totalWeight;
}

float GetCoralBiomeAllowance(Biome biome)
{
	switch (biome)
	{
	case Biome::CORAL_GARDEN: return 1.0f;
	case Biome::ROCKY_SAND: return 0.45f;
	case Biome::MOSSY_ROCK: return 0.35f;
	case Biome::SMOOTH_ROCK: return 0.15f;
	default: return 0.0f;
	}
}
