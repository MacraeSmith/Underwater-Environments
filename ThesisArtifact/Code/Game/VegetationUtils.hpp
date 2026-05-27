#pragma once
#include "Game/GameCommon.hpp"
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Mat44.hpp"


class ShaderDX12;
class TextureDX12;
class VertexBufferDX12;
class IndexBufferDX12;
enum class Biome : int;
struct BiomeSplat;

enum class VegetationType : int
{
	UN_INITIALIZED = -1,
	SEA_GRASS,
	KELP,
	SEA_ANEMONE,
	GRASS,

	CORAL_MUSHROOM,
	CORAL_BLUE_MUSHROOM_01,
	CORAL_BLUE_MUSHROOM_02,

	CORAL_ELKHORN,
	CORAL_BRAIN,

	CORAL_TUBE_SPONGE_01,
	CORAL_TUBE_SPONGE_02,
	CORAL_TUBE_SPONGE_03,

	CORAL_LETTUCE_01,
	CORAL_LETTUCE_02,
	CORAL_LETTUCE_03,

	CORAL_PINE_CONE_01,
	CORAL_PINE_CONE_02,

	CORAL_SQUASH,

	CORAL_SEA_FAN_01,
	CORAL_SEA_FAN_02,

	CORAL_BRANCHING_01,
	CORAL_BRANCHING_02,

	CORAL_STUB,
	COUNT,
};


constexpr int NUM_NON_CORAL_VEGETATION_TYPES = (int)VegetationType::CORAL_MUSHROOM;
constexpr int NUM_CORAL_TYPES = (int)VegetationType::COUNT - (int)VegetationType::CORAL_MUSHROOM;

struct VegetationConstants
{
	FloatRange m_heightRange;
	FloatRange m_widthRange;
	float m_rigidity = 0.f;
	float m_emissiveStrength = 1.f;
};

struct CoralBlendConstants
{
	float m_depthBlendStrength = 0.f;
	float m_depthBlendThreshold = 0.0025f;
	float m_blendRadius = 0.f;
};


struct VegetationWeight
{
	VegetationType m_type = VegetationType::UN_INITIALIZED;
	float m_weight = 0.f;
};


//Vegetation instances
struct VegetationVertex
{
	Vec3 m_position;
	float m_baseRadius; // unsused in all but anenomes
	Vec3 m_terrainNormal;
	unsigned int PAD = 0;
};


struct GrassInstance
{
	Vec3 m_position;
	float m_yaw = 0;
	Vec3 m_terrainNormal;
	unsigned int m_seed = 0;
	float m_heightMultiplier = 1.f;
};

struct CoralInstance
{
	Mat44 m_worldTransform;
	unsigned int m_seed = 0;
};

struct GrassConstants
{
	unsigned int  vertexCount = 0;
	float PAD[3] = {};
};

struct PackedVegetationDisc
{
	Vec2 m_center;
	float m_radius = 0.f;
	VegetationType m_type = VegetationType::UN_INITIALIZED;
};


std::string			GetVegetationTypeNameFromType(VegetationType type);
VegetationType		GetVegetationTypeFromName(std::string const& name);
void				AccumulateBiomeVegetationWeights(Biome biome, float biomeSplatWeight, float* vegetationWeights);
float				GetVegetationWeightsFromBiomeSplat(BiomeSplat const& biomeSplat, float* vegetationWeights);

bool				IsVegetationTypeCoral(VegetationType type);
float				GetCoralBiomeAllowance(Biome biome);
bool				WasVegetationTypeOBJLoaded(VegetationType type);
bool				DoesVegetationTypeHaveMaskedTexture(VegetationType type);
bool				IsVegetationTypeTwoSided(VegetationType type);
