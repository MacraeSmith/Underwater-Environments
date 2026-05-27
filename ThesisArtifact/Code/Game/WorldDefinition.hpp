#pragma once
#include "Game/WorldUtils.hpp"
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include <string>
#include <vector>
#include <mutex>

class Skybox;
class ShaderDX12;
class TextureDX12;
struct IntVec3;
class Image;
template <typename T>
class PieceWiseCurve;
template <typename T>
class Curve;
class VertexBufferDX12;
class IndexBufferDX12;

struct WorldDefinition
{
public:
	//Base Settings
	//---------------------------------------------------------------------------------------
	std::string m_worldName = "UNAMED_WORLD";
	float m_worldHeight = 500.f;
	float m_baseTerrainDepthBelowSeaLevel = 0.f;
	int m_voxelScale = 1;
	float m_invVoxelScale = 1.f;
	bool m_xmlGenerated = false;
	float m_biomeBlend = 1.f;

	//Noise values
	//---------------------------------------------------------------------------------------
	int m_gameSeed = 0;
	float m_defaultOctavePersistance = 0.5f;
	float m_defaultOctaveScale = 2.f;
	Vec2 m_warpNoiseScaleXY = Vec2(800.f, 900.f);
	int m_warpNoiseNumOctaves = 5;
	float m_baseTerrainBiasFactor = 0.f;
	float m_finalTerrainBiasFactor = 100.f;
	TerrainNoiseInfo m_terrainNoiseInfos[(int)TerrainNoiseType::COUNT] = {};
	VegetationNoiseInfo m_vegetationNoiseInfos[(int)VegetationNoiseType::COUNT] = {};

	//Terrain Texture and Shader values
	//---------------------------------------------------------------------------------------
	ShaderDX12* m_terrainShader = nullptr;
	std::string m_terrainShaderFilePath;
	BiomeTextureInfo m_biomeTextureInfos[(int)Biome::NUM_BIOMES] = {};
	TerrainBiomeTextureConstants m_textureConstants;

	//Water values
	//---------------------------------------------------------------------------------------
	bool m_activeWaterSurface = true;
	std::string m_waterSurfaceShaderFilePath;
	ShaderDX12* m_waterSurfaceShader = nullptr;
	std::string m_waterNormalTextureFilePath;
	TextureDX12* m_waterNormalTexture = nullptr;
	std::string m_waterFoamTextureFilePath;
	TextureDX12* m_waterFoamTexture = nullptr;
	WaterConstants m_waterConstants;

	bool m_activeWind[(int)WindType::COUNT] = {};
	WindConstants m_windConstants[(int)WindType::COUNT] = {};

	//Light
	//---------------------------------------------------------------------------------------
	SunConstants m_sunInfo;
	LightRayConstants m_lightRayConstants;
	bool m_activeLightRays = true;
	std::string m_lightRaysMaskShaderPath;
	ShaderDX12* m_lightRaysMaskShader = nullptr;
	std::string m_lightRaysVolumeComputeShaderPath;
	ShaderDX12* m_lightRaysVolumeComputeShader = nullptr;
	
	//Caustic Values
	//---------------------------------------------------------------------------------------
	bool m_activeCaustics = true;
	CausticsConstants m_causticConstants;

	//Fog values
	//---------------------------------------------------------------------------------------
	bool m_activeFog = true;
	std::string m_fogShaderFilePath;
	ShaderDX12* m_fogShader = nullptr;
	FogConstants m_fogConstants;

	bool m_activeParticles = false;
	ParticleConstants m_particleConstants;

	//Skybox
	//---------------------------------------------------------------------------------------
	std::string m_skyboxXMLFilePath;
	Skybox* m_skybox = nullptr;

	//Fish
	//---------------------------------------------------------------------------------------
	bool m_activeFish = true;
	int m_maxNumFish = 1000;
	int m_maxNumFishNeighborsToCheck = 25;
	FishInfo m_fishInfos[(int)FishType::COUNT];
	bool m_shouldLoadFishModels = true;

	//Vegetation
	//---------------------------------------------------------------------------------------
	bool m_activeVegetation = true;
	bool m_activeCoral = true;
	float m_coralClusterThickness = 0.f;
	CoralBlendConstants m_coralBlendConstants;
	ShaderDX12* m_coralBlendPostProcessShader = nullptr;
	std::string m_coralBlendPostProcessShaderFilePath;
	VegetationInfo m_vegetationInfos[(int)VegetationType::COUNT];
	bool m_shouldLoadVegetationModels = true;
	float m_smallestVegetationDotProductThreshold = 1.f;


public:
	static std::vector<WorldDefinition> s_worldDefinitions;

public:
	WorldDefinition() {}
	explicit WorldDefinition(XmlElement const& worldDefElement);
	explicit WorldDefinition(WorldDefinition const* copyFrom);
	~WorldDefinition();

	static WorldDefinition const* GetWorldDefinitionFromName(std::string const& name);
	static void		InitWorldDefinitionsFromFile(std::string const& filePath);
	static void		ClearWorldDefinitionData();

	static void		SaveDefinition(WorldDefinition const& worldDef);
	static size_t	GetTotalWorldDefSize();

	static void			LoadBiomeInfoTextureFromImage(std::string const& worldName, Biome biome, bool isFloorTexture, int textureMapIndex, Image* image);
	static void			LoadBiomeInfoTextureFromBindlessTextureIndex(std::string const& worldName, Biome biome, bool isFloorTexture, int textureMapIndex, unsigned int bindlessTextureIndex);
	static bool 		TryToGetFishInfoImagesAndShaderFile(std::string const& meshFolderPath, MeshImages& out_images, std::string& out_shaderFilePath);
	static void			LoadFishMeshInfo(std::string const& worldName, FishType fishType, MeshImages fishImages, VertTBNs const& verts, IndexList const& indexes, std::string const& shaderFilePath);
	static bool			TryToGetVegInfoImagesAndShaderFile(std::string const& meshFolderPath, MeshImages& out_images, std::string& out_shaderFilePath);
	static void			LoadVegetationMeshInfo(std::string const& worldName, VegetationType vegType, MeshImages vegImages, VertTBNs const& verts, IndexList const& indexes, std::string const& shaderFilePath, float meshRadius);

	//Parse Info
	//---------------------------------------------------------------------------------------
	void	ParseSkyBoxInfo(XmlElement const& worldDefElement);
	void	ParseNoiseInfo(XmlElement const& worldDefElement);
	void	ParseTextureInfo(XmlElement const& worldDefElement);
	void	ParseWaterInfo(XmlElement const& worldDefElement);
	void	ParseWindInfo(XmlElement const& worldDefElement);
	void	ParseSunInfo(XmlElement const& worldDefElement);
	void	ParseCausticsInfo(XmlElement const& worldDefElement);
	void	ParseFogInfo(XmlElement const& worldDefElement);
	void	ParseLightRayInfo(XmlElement const& worldDefElement);
	void	ParseParticleInfo(XmlElement const& worldDefElement);
	void	ParseFishInfo(XmlElement const& worldDefElement);
	void	ParseVegetationInfo(XmlElement const& worldDefElement);
	void	ParseTextureFiles(XmlElement const& textureFilesElement, BiomeTextureInfo& out_biomeTextureInfo);

	//Save Info
	//---------------------------------------------------------------------------------------
	void	SetUsableName();
	void	SaveDefinitionAsXML() const;

	//World Helpers
	//---------------------------------------------------------------------------------------
	float	GetSeaLevel() const {return m_waterConstants.m_seaLevel;}
	float	GetWorldMin() const {return m_waterConstants.m_seaLevel - m_waterConstants.m_maxTerrainDepth;}
	float	GetMaxDepth() const {return m_waterConstants.m_maxTerrainDepth;}
	IntVec3 GetChunkSizeWithVoxelScale() const;

	//Curve Helpers
	//---------------------------------------------------------------------------------------
	float	GetCurveValue(TerrainNoiseType noiseType, TerrainCurveType curveType, float t) const;
	CurveWithTStart<float> GetCurveAndTPlotFromXMLElement(XmlElement const& plotPointElement, Vec2& out_startPlotPoint) const;


	//Noise Helpers
	//---------------------------------------------------------------------------------------
	
	//Terrain
	float					GetCumulativeDensityAtPosition(Vec3 const& pos) const;
	float					GetBaseTerrainNoiseAtPos(Vec3 const& pos) const;
	TerrainNoiseValues2D	GetTerrainNoiseValuesAtPos2D(Vec2 const& pos) const;


	BiomeSplat				GetBiomeSplatFromNoiseValues(TerrainNoiseValues2D const& terrainNoiseValues, VegetationNoiseValues2D const& vegetationNoiseValues, Vec3 const& pos, Vec3 const& terrainNormal) const;
	void					GetBiomeScoresFromNoiseValues(float outBiomeScores[(int)Biome::NUM_BIOMES], TerrainNoiseValues2D const& terrainNoiseValues, VegetationNoiseValues2D const& vegetationNoiseValues, Vec3 const& pos, Vec3 const& terrainNormal) const;

	float					GetTerrainHeightOffsetAtPos2D(TerrainNoiseValues2D const& noiseValuesAtPos) const;
	float					GetSquashFactorAtPos2D(TerrainNoiseValues2D const& noiseValuesAtPos) const;

	//Vegetation
	void					UpdateVegetationSpawnRadiusInfo();
	VegetationNoiseValues2D GetVegetationNoiseValues2DAtPos2D(Vec2 const& pos) const;
	float					GetVegetationNoiseValueAtPos2D(Vec2 const& pos, VegetationNoiseType const& worldNoiseType) const;


};

