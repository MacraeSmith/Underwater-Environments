#pragma once
#include "Game/BiomeUtils.hpp"
#include "Game/NoiseUtils.hpp"
#include "Game/VegetationUtils.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/EulerAngles.hpp"

#include <vector>

class ShaderDX12;
class TextureDX12;
class VertexBufferDX12;
class IndexBufferDX12;
class Image;

//Textures
//-------------------------------------------------------------------------------------------

constexpr int NUM_BIOME_TEXTURES = (int)Biome::NUM_BIOMES * 2;
//for every biome there is a floor and wall texture in each texture map (albedo, normal, ao, etc)
//indexing is 0 - num biomes = floor textures, numBiomes onward is wall textures
#define PAD_FOR_HLSL_VEC4(x) (((x) + 3) & ~3)

struct TerrainBiomeTextureConstants
{
	uint32_t m_albedoTextureIndexes[PAD_FOR_HLSL_VEC4(NUM_BIOME_TEXTURES)] = {};
	uint32_t m_normalTextureIndexes[PAD_FOR_HLSL_VEC4(NUM_BIOME_TEXTURES)] = {};
	uint32_t m_aoTextureIndexes[PAD_FOR_HLSL_VEC4(NUM_BIOME_TEXTURES)] = {};
	float m_textureScales[PAD_FOR_HLSL_VEC4(NUM_BIOME_TEXTURES)] = {};
	float m_normalStrengths[PAD_FOR_HLSL_VEC4(NUM_BIOME_TEXTURES)] = {};
};

constexpr int NUM_TERRAIN_TEXTURE_MAPS = 3;

struct TextureFolderPaths
{
	std::string m_parentFolder;
	std::string m_albedoFile;
	std::string m_normalFile;
	std::string m_aoFile;
};

struct BiomeTextureInfo
{
	Biome m_biome;
	TextureFolderPaths m_floorFiles;
	TextureFolderPaths m_wallFiles;
};


struct MeshImages
{
	Image* m_diffuseImage = nullptr;
	Image* m_normalImage = nullptr;
	Image* m_specularImage = nullptr;
	Image* m_aoImage = nullptr;
	Image* m_emissiveImage = nullptr;
};

struct MeshTextures
{
	TextureDX12* m_diffuseTex = nullptr;
	TextureDX12* m_normalTex = nullptr;
	TextureDX12* m_specularTex = nullptr;
	TextureDX12* m_aoTex = nullptr;
	TextureDX12* m_emissiveTex = nullptr;
	void LoadTexturesFromMeshImages(MeshImages const& meshImages, int mipLevels = 10);
};


//Vegetation
//-------------------------------------------------------------------------------------------
struct VegetationInfo
{
	VegetationType m_type = VegetationType::UN_INITIALIZED;
	std::string m_meshInfo = UN_INITIALIZED_WORLD_DATA_NAME;
	ShaderDX12* m_graphicsShader = nullptr;
	ShaderDX12* m_computeShader = nullptr;
	VertexBufferDX12* m_instanceVbo = nullptr;
	IndexBufferDX12* m_instanceIBO = nullptr;
	MeshTextures m_textures;

	std::string m_graphicsShaderName;
	std::string m_computeShaderName;
	std::string m_diffuseTexName = UN_INITIALIZED_WORLD_DATA_NAME;
	std::string m_normalTexName = UN_INITIALIZED_WORLD_DATA_NAME;
	bool m_isActive = false;
	float m_worldDotUpThreshold = 0.9f;
	float m_thickness = 0.9f;
	VegetationConstants m_vegConstants;
	float m_meshRadius = 0.f;
	float m_instanceRadius = 1.f;
};

//Water
//-------------------------------------------------------------------------------------------
constexpr int MAX_NUM_WAVES = 8;
struct WaveData
{
	Vec2 m_directionXY = Vec2(1.f, 0.f);
	float m_amplitude = 5.f;
	float m_waveLength = 130.f;
};

struct WaveDataExtra
{
	float m_waveSpeed = 1.f;
	float m_waveSteepness = 0.5f;
	float m_waveOffset = 0.f;
	float pad = 0.f;
};

struct WaterConstants
{
	WaveData m_waveData[MAX_NUM_WAVES] = {};
	WaveDataExtra m_waveDataExtra[MAX_NUM_WAVES] = {};
	int m_numWaves = 1;
	float m_globalWaveSpeedScale = 0.5f;
	float m_globalCrestSteepnessScale = 0.f;
	float m_globalAmplitudeScale = 0.f;
	float m_globalWavelengthScale=0.f;

	float m_directionVarianceRegionScale = 0.f;
	float m_directionVariance = 0.f;
	float m_amplitudeVarianceRegionScale = 0.f;
	float m_amplitudeVariance = 0.f;

	float m_seaLevel = 0.f;
	float m_underwaterTintDepthScale = 0.1f;
	float m_underwaterDepthDarkenScale = 1.f;
	float m_foaminess = 0.5f;
	float m_reflectionStrength = 0.5f;
	float m_refractionStrength=0.5f;
	float m_maxTerrainDepth = 100.f;
	float m_mirkyness = 0.5f;
	float m_shorelineFoamStrength = 0.5f;
	float m_worldVoxelScale = 1.f;
	float m_specularIntensity = 1.f;
	float m_specularPower=0.25f;
};

enum class WindType : int 
{
	UN_INITIALIZED = -1,
	WIND,
	UNDER_WATER_CURRENT,
	COUNT
};

struct WindConstants
{
	float m_baseSpeed = 0.35f;                    // gentle steady push
	float m_largeScale = 0.04f;   // very large features (hundreds of meters)
	float m_mediumScale = 0.015f;   // mid sized flow variation
	float m_smallScale = 0.05f;    // small turbulence

	float m_largeSpeed = 0.05f;    // slow drift
	float m_mediumSpeed = 0.12f;    // noticeable movement
	float m_smallSpeed = 0.25f;    // subtle shimmer

	float m_largeStrength = 0.05f;   
	float m_mediumStrength = 0.12f;   
	float m_smallStrength = 0.35f;

	float m_swirlStrength = 0.15f;    // small vertical motion
	float m_warbleStrength = 0.2f;     // directional wobble;
};

// Lighting
//-------------------------------------------------------------------------------------------

struct SunConstants
{
	EulerAngles m_orientation;
	float m_intensity = 1.f;
	float m_ambientIntensity = 0.5f;
	float m_color[4] = {1.f, 1.f, 1.f, 1.f};
};

struct CausticsConstants
{
	float m_causticsIntensity = 1.f;
	float m_causticsDepthFadeRate = 0.00015f;
	float m_causticsWarpSpeed = 0.6f;
	float m_causticsWarpStrength = 0.05f;
	float m_causticsLineThickness = 0.3f;
	float m_causticsRippleSpeed = 1.f;
	float m_causticsRippleStrength = 0.15f;
};


//Fog
//-------------------------------------------------------------------------------------------
constexpr int UNDERWATER_FOG_INDEX = 0;
constexpr int ABOVE_WATER_FOG_INDEX = 1;
struct FogData
{
	float m_color[4] = {};
	float m_depth = 0.2f;
	float m_opacity = 1.f;
	int m_transition = 1;
	float m_padding;
};
struct FogConstants
{
	FogData m_fogData[2] = {};
	float m_underwaterTintDepthScale = 0.1f;
	float m_fogVolumeHeight = 20.f;
	float m_padding[3] ={};
};

struct FogVolumeConstants
{
	Mat44 m_clipToWorldTransform;
	Vec3 m_cameraPosition;
	float m_seaLevel;
	float m_fogDepth;
	Vec3 m_sunDirection;
	float m_time;
	Vec3 m_padding;
};

struct LightRayConstants
{
	Vec2 m_screenSize;
	float m_shaftIntensity = 0.f;
	float m_shaftScale = 0.f;

	float m_shaftSpread = 1.f;
	float m_shaftThreshold = 0.f;
	float m_shaftWarpSpeed = 0.08f;
	float m_shaftWarpStrength = 1.6f;

	float m_shaftColor[4] = {};

	float m_shaftContrast = 0.f;
	float m_shaftDepthFade = 0.f;
	int m_maxNumSteps = 64;
	int m_renderLightRays = 1;
};

struct ParticleConstants
{
	float m_density = 0.f;
	FloatRange m_sizeRange = FloatRange::ZERO_TO_ONE;
	float m_cellSize = 0.08f;

	int m_particlesPerCell = 1;
	float m_intensity = 0.f;
	float m_depthFade=0.15f;
	float m_driftSpeed= 0.f;

	int	  m_activeParticles = 0;

};


//Fish
//-------------------------------------------------------------------------------------------

enum class FishType : int
{
	UN_INITIALIZED = -1,
	ANGEL_FISH,
	CHROMIS,
	SHARK,
	COUNT,
};


struct FishTypeSwimConstants
{
	float m_tailStart = -0.5f;  // Portion of body that stays mostly rigid
	float m_modelLength = 10.0f;   // Length of fish along the swim axis
};


struct FishInfo
{
	FishType m_type = FishType::UN_INITIALIZED;
	bool m_isActive = true;
	std::string m_meshInfo;
	int m_maxNumber = 300;
	int m_spawnThickness = 5;
	float m_flockingRadiusSqrd = 20.f;
	float m_separationRadiusSqrd = 10.f;
	float m_playerFlockingRadiusSqrd = 500.f;
	float m_playerSeparationRadiusSqrd = 20.f;
	float m_probeDistance = 3.f;
	float m_terrainAvoidance = 3.f;
	float m_cohesion = 3.f;
	float m_alignment = 3.f;
	float m_separation = 3.f;
	float m_minOceanDepth = 15.f;
	float m_maxSpeed = 50.f;
	float m_maxTurnSpeed = 10.f;
	float m_playerCohesion = 10.f;
	float m_playerSeparation = 10.f;
	VertexBufferDX12* m_vbo = nullptr;
	IndexBufferDX12* m_ibo = nullptr;
	ShaderDX12* m_shader = nullptr;
	ShaderDX12* m_computeShader = nullptr;
	FishTypeSwimConstants m_swimConstants;
	MeshTextures m_textures;
	std::string GetFolderName() const;
};

struct FishInstanceData
{

	Vec3 m_position;
	unsigned int m_fishID;
	Vec3 m_forwardNormal;
	float m_amplitude = 0.2f;  // Side-to-side displacement in model units
	float m_frequency = 1.f;   // Tail beats per second
	float m_waveLength = 3.9f;   // Distance (in model units) per sine cycle
	float m_tailPower = 1.0f;   // How aggressively motion concentrates in tail

	void UpdateInfo(Vec3 position, Vec3 forwardNormal, float speed, FishInfo const& fishInfo);
};


void				LoadChildrenTexturePathsFromParentFolder(TextureFolderPaths& filePaths, std::string const& parentFolderPath);


std::string			GetWindTypeNameFromType(WindType type);
WindType			GetWindTypeFromName(std::string const& name);

std::string			GetFishTypeNameFromType(FishType type);
FishType			GetFishTypeFromName(std::string const& name);

WindType			GetWindTypeFromVegetationType(VegetationType vegType);

//Biome Helpers
Biome					GetAboveWaterBiome(float temperature);
Biome					GetShallowBiome(float rockiness, float temperature, float kelpMask);
Biome					GetMidBiome(float rockiness, float temperature, float coralMask, float kelpMask);
Biome					GetDeepBiome(float temperature, float coralMask);


