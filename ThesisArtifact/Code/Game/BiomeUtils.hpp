#pragma once
#include <string>

struct Vertex_Terrain;
struct Vec3;

//Biomes
//-------------------------------------------------------------------------------------------
enum class Biome : int
{
	UN_INITIALIZED = -1,
	SANDY_PLAINS,
	ROCKY_SAND,
	CORAL_GARDEN,
	KELP_FOREST,
	SMOOTH_ROCK,
	MOSSY_ROCK,
	BEACH,
	GRASSY_BEACH,
	GRASSY_SAND,
	NUM_BIOMES,
};

struct BiomeSplat
{
	Biome m_biomes[4] = { Biome::SANDY_PLAINS, Biome::SANDY_PLAINS, Biome::SANDY_PLAINS, Biome::SANDY_PLAINS };
	float m_weights[4] = { 1.f, 0.f, 0.f, 0.f };
};



std::string			GetBiomeNameFromType(Biome biome);
Biome				GetBiomeTypeFromName(std::string const& name);

BiomeSplat				BuildBiomeSplatFromScores(float const biomeScores[(int)Biome::NUM_BIOMES]);
Vertex_Terrain			GetTerrainVertFromBiomeSplat(Vec3 const& pos, Vec3 const& normal, BiomeSplat const& splat);
BiomeSplat				 MakeTwoBiomeSplat(Biome a, Biome b, float t);



float					GetCoralRegionMask(float coralNoise, float oceanDepthMeters);
float					GetKelpRegionMask(float kelpNoise, float temperature, float oceanDepthMeters);
float					GetVegetationPotential(float temperature);
