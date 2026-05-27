#pragma once
#include "Game/GameCommon.hpp"


template <typename T>
class PieceWiseCurve;
template <typename T>
class Curve;

enum class VegetationType : int;

enum class NoiseType : int
{
	RAW_ZERO_TO_ONE,
	RAW_NEG_ONE_TO_ONE,
	FRACTAL,
	PERLIN,
	RIDGED,
	COUNT
};

enum class WorldNoiseType : int
{
	UN_INITIALIZED = -1,
	TERRAIN,
	VEGETATION,
	COUNT,
};

//Terrain
//-------------------------------------------------------------------------------------------
enum class TerrainNoiseType : int
{
	UN_INITIALIZED = -1,
	TEMPERATURE,
	PEAKS_AND_VALLEYS,
	EROSION,
	CONTINENTALNESS,
	BASE_TERRAIN,
	COUNT,
};

const int NUM_2D_TERRAIN_NOISE_VALUES = (int)TerrainNoiseType::BASE_TERRAIN;

struct TerrainNoiseValues2D
{
	float m_values[NUM_2D_TERRAIN_NOISE_VALUES] = {};
};

enum class TerrainCurveType : int
{
	HEIGHT_OFFSET,
	SQUASH,
	COUNT
};

template <typename T>
struct CurveWithTStart;

struct TerrainNoiseInfo
{
	std::string m_name = UN_INITIALIZED_WORLD_DATA_NAME;
	WorldNoiseType m_worldNoiseType = WorldNoiseType::TERRAIN;
	NoiseType m_noiseType = NoiseType::PERLIN;
	bool m_isActive = true;
	int m_seed = 0;
	float m_scale = 100.f;
	float m_strength = 1.0;
	int m_numOctaves = 2;
	float m_warpStrength;
	PieceWiseCurve<float>* m_terrainHeightOffsetCurve = nullptr;
	PieceWiseCurve<float>* m_squashCurve = nullptr;
};

//Vegetation
//-------------------------------------------------------------------------------------------
enum class VegetationNoiseType : int
{
	UN_INITIALIZED = -1,
	SEA_GRASS,
	KELP,
	SEA_ANEMONE,
	GRASS,
	CORAL,
	COUNT
};

struct VegetationNoiseValues2D
{
	float m_values[(int)VegetationNoiseType::COUNT] = {};
};

struct VegetationNoiseInfo
{
	std::string m_name = UN_INITIALIZED_WORLD_DATA_NAME;
	WorldNoiseType m_worldNoiseType = WorldNoiseType::VEGETATION;
	NoiseType m_noiseType = NoiseType::PERLIN;
	int m_seed = 0;
	float m_scale = 100.f;
	float m_strength = 1.0;
	int m_numOctaves = 2;
	float m_warpStrength;
};


std::string			GetCurveTypeName(Curve<float>* curve, int& out_exponent);

std::string			GetNoiseTypeNameFromType(NoiseType type);
NoiseType			GetNoiseTypeFromName(std::string const& name);

std::string			GetWorldNoiseTypeNameFromType(WorldNoiseType type);
WorldNoiseType		GetWorldNoiseTypeFromName(std::string const& name);

std::string			GetTerrainNoiseTypeNameFromType(TerrainNoiseType type);
TerrainNoiseType	GetTerrainNoiseTypeFromName(std::string const& name);

std::string			GetVegetationNoiseTypeNameFromType(VegetationNoiseType type);
VegetationNoiseType	GetVegetationNoiseTypeFromName(std::string const& name);

VegetationNoiseType GetVegetationNoiseTypeFromVegetationType(VegetationType type);

float					GetVegetationClusterMask(float vegetationNoise, VegetationNoiseType vegNoiseType);