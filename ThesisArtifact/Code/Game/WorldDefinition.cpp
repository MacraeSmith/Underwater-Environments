#include "Game/WorldDefinition.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Renderer/RendererDX12.hpp"
#include "Engine/Renderer/ShaderDX12.hpp"
#include "Engine/Renderer/TextureDX12.hpp"
#include "Engine/Renderer/Skybox.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Renderer/DebugRender.hpp"
#include "Engine/Renderer/VertexBufferDX12.hpp"
#include "Engine/Renderer/IndexBufferDX12.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/Curves.hpp"
#include "Engine/Math/SmoothNoise.hpp"
#include "Engine/Math/RawNoise.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Window/Window.hpp"
#include "ThirdParty/TinyXML2/tinyxml2.h"
#include <algorithm>


std::vector<WorldDefinition> WorldDefinition::s_worldDefinitions;

WorldDefinition::WorldDefinition(XmlElement const& worldDefElement)
	:m_xmlGenerated(true)
	,m_shouldLoadFishModels(true)
	,m_shouldLoadVegetationModels(true)
{
	m_worldName = ParseXmlAttribute(worldDefElement, "worldName", m_worldName);
	m_voxelScale = ParseXmlAttribute(worldDefElement, "voxelScale", m_voxelScale);
	m_invVoxelScale = 1.f / (float)m_voxelScale;
	m_waterConstants.m_seaLevel = ParseXmlAttribute(worldDefElement, "seaLevel", m_waterConstants.m_seaLevel);
	m_worldHeight = ParseXmlAttribute(worldDefElement, "worldHeight", m_worldHeight);
	m_baseTerrainDepthBelowSeaLevel = ParseXmlAttribute(worldDefElement, "baseTerrainDepth", 0.f);
	m_biomeBlend = ParseXmlAttribute(worldDefElement, "biomeBlend", 1.f);

	ParseSkyBoxInfo(worldDefElement);
	ParseNoiseInfo(worldDefElement);
	ParseTextureInfo(worldDefElement);
	ParseWaterInfo(worldDefElement);
	ParseWindInfo(worldDefElement);
	ParseSunInfo(worldDefElement);
	ParseCausticsInfo(worldDefElement);
	ParseLightRayInfo(worldDefElement);
	ParseParticleInfo(worldDefElement);
	ParseFogInfo(worldDefElement);
	ParseFishInfo(worldDefElement);
	ParseVegetationInfo(worldDefElement);
}

WorldDefinition::WorldDefinition(WorldDefinition const* copyFrom)
	:m_worldName(copyFrom->m_worldName)
	,m_voxelScale(copyFrom->m_voxelScale)
	,m_worldHeight(copyFrom->m_worldHeight)
	,m_baseTerrainDepthBelowSeaLevel(copyFrom->m_baseTerrainDepthBelowSeaLevel)
	,m_biomeBlend(copyFrom->m_biomeBlend)
	,m_gameSeed(copyFrom->m_gameSeed)
	,m_defaultOctavePersistance(copyFrom->m_defaultOctavePersistance)
	,m_defaultOctaveScale(copyFrom->m_defaultOctaveScale)
	,m_warpNoiseScaleXY(copyFrom->m_warpNoiseScaleXY)
	,m_warpNoiseNumOctaves(copyFrom->m_warpNoiseNumOctaves)
	,m_baseTerrainBiasFactor(copyFrom->m_baseTerrainBiasFactor)
	,m_finalTerrainBiasFactor(copyFrom->m_finalTerrainBiasFactor)
	,m_terrainShaderFilePath(copyFrom->m_terrainShaderFilePath)
	,m_terrainShader(copyFrom->m_terrainShader)
	,m_textureConstants(copyFrom->m_textureConstants)
	,m_activeWaterSurface(copyFrom->m_activeWaterSurface)
	,m_waterSurfaceShaderFilePath(copyFrom->m_waterSurfaceShaderFilePath)
	,m_waterSurfaceShader(copyFrom->m_waterSurfaceShader)
	,m_waterNormalTextureFilePath(copyFrom->m_waterNormalTextureFilePath)
	,m_waterNormalTexture(copyFrom->m_waterNormalTexture)
	,m_waterFoamTextureFilePath(copyFrom->m_waterFoamTextureFilePath)
	,m_waterFoamTexture(copyFrom->m_waterFoamTexture)
	,m_waterConstants(copyFrom->m_waterConstants)
	,m_sunInfo(copyFrom->m_sunInfo)
	,m_lightRayConstants(copyFrom->m_lightRayConstants)
	,m_lightRaysMaskShaderPath(copyFrom->m_lightRaysMaskShaderPath)
	,m_lightRaysMaskShader(copyFrom->m_lightRaysMaskShader)
	,m_lightRaysVolumeComputeShader(copyFrom->m_lightRaysVolumeComputeShader)
	,m_lightRaysVolumeComputeShaderPath(copyFrom->m_lightRaysVolumeComputeShaderPath)
	,m_activeLightRays(copyFrom->m_activeLightRays)
	,m_activeParticles(copyFrom->m_activeParticles)
	,m_particleConstants(copyFrom->m_particleConstants)
	,m_activeCaustics(copyFrom->m_activeCaustics)
	,m_causticConstants(copyFrom->m_causticConstants)
	,m_activeFog(copyFrom->m_activeFog)
	,m_fogConstants(copyFrom->m_fogConstants)
	,m_fogShaderFilePath(copyFrom->m_fogShaderFilePath)
	,m_fogShader(copyFrom->m_fogShader)
	,m_skyboxXMLFilePath(copyFrom->m_skyboxXMLFilePath)
	,m_skybox(copyFrom->m_skybox)
	,m_activeFish(copyFrom->m_activeFish)
	,m_maxNumFish(copyFrom->m_maxNumFish)
	,m_maxNumFishNeighborsToCheck(copyFrom->m_maxNumFishNeighborsToCheck)
	,m_shouldLoadFishModels(false)
	,m_shouldLoadVegetationModels(false)
	,m_activeVegetation(copyFrom->m_activeVegetation)
	,m_activeCoral(copyFrom->m_activeCoral)
	,m_coralClusterThickness(copyFrom->m_coralClusterThickness)
	,m_coralBlendConstants(copyFrom->m_coralBlendConstants)
	,m_coralBlendPostProcessShader(copyFrom->m_coralBlendPostProcessShader)
	,m_coralBlendPostProcessShaderFilePath(copyFrom->m_coralBlendPostProcessShaderFilePath)
	,m_xmlGenerated(false)
{
	for (int i = 0; i < (int)Biome::NUM_BIOMES; ++i)
	{
		m_biomeTextureInfos[i] = copyFrom->m_biomeTextureInfos[i];
	}

	for (int i = 0; i < (int)TerrainNoiseType::COUNT; ++i)
	{
		m_terrainNoiseInfos[i] = copyFrom->m_terrainNoiseInfos[i];
	}

	for (int i = 0; i < (int)FishType::COUNT; ++i)
	{
		m_fishInfos[i] = copyFrom->m_fishInfos[i];
	}

	for (int i = 0; i < (int)VegetationType::COUNT; ++i)
	{
		m_vegetationInfos[i] = copyFrom->m_vegetationInfos[i];
	}

	UpdateVegetationSpawnRadiusInfo();

	for (int i = 0; i < (int)VegetationNoiseType::COUNT; ++i)
	{
		m_vegetationNoiseInfos[i] = copyFrom->m_vegetationNoiseInfos[i];
	}


	for (int i = 0; i < (int)WindType::COUNT; ++i)
	{
		m_windConstants[i] = copyFrom->m_windConstants[i];
		m_activeWind[i] = copyFrom->m_activeWind[i];
	}

	m_invVoxelScale = 1.f / (float)m_voxelScale;

	m_waterConstants.m_worldVoxelScale = (float)m_voxelScale;
}

WorldDefinition::~WorldDefinition()
{

}

//World Helpers
//----------------------------------------------------------------------------------------------------------------------------------------
void WorldDefinition::InitWorldDefinitionsFromFile(std::string const& filePath)
{
	XmlDocument worldDefXml;
	XmlResult result = worldDefXml.LoadFile(filePath.c_str());
	GUARANTEE_OR_DIE(result == tinyxml2::XML_SUCCESS, Stringf("Failed to open world definition document: %s", filePath.c_str()));

	XmlElement* rootElement = worldDefXml.RootElement();
	GUARANTEE_OR_DIE(rootElement, Stringf("Failed to find root element in world def xml: %s", filePath.c_str()));

	XmlElement* worldDefElement = rootElement->FirstChildElement();
	while (worldDefElement != nullptr)
	{
		std::string elementName = worldDefElement->Name();
		GUARANTEE_OR_DIE(elementName == "WorldDefinition", Stringf("Root child element in %s was <%s>, must be <WorldDefinition>", filePath.c_str(), elementName.c_str()));
		WorldDefinition* newWorldDef = new WorldDefinition(*worldDefElement);
		s_worldDefinitions.push_back(*newWorldDef);
		worldDefElement = worldDefElement->NextSiblingElement();
	}
}

void WorldDefinition::ClearWorldDefinitionData()
{
	for (int i = 0; i < (int)s_worldDefinitions.size(); ++i)
	{
		WorldDefinition& worldDef = s_worldDefinitions[i];
		if (worldDef.m_xmlGenerated)
		{
			delete worldDef.m_skybox;
		}
		worldDef.m_skybox = nullptr;

		for (int j = 0; j < (int)TerrainNoiseType::COUNT; ++j)
		{
			TerrainNoiseInfo& noiseInfo = worldDef.m_terrainNoiseInfos[j];
			if (worldDef.m_xmlGenerated)
			{
				delete noiseInfo.m_terrainHeightOffsetCurve;
				delete noiseInfo.m_squashCurve;
			}
			noiseInfo.m_terrainHeightOffsetCurve = nullptr;
			noiseInfo.m_squashCurve = nullptr;
		}

		for (int j = 0; j < (int)FishType::COUNT; ++j)
		{
			FishInfo& fishInfo = worldDef.m_fishInfos[j];
			if (worldDef.m_xmlGenerated)
			{
				delete fishInfo.m_vbo;
				delete fishInfo.m_ibo;
			}

			fishInfo.m_vbo = nullptr;
			fishInfo.m_ibo = nullptr;
		}

		for (int j = 0; j < (int)VegetationType::COUNT; ++j)
		{
			VegetationInfo& vegInfo = worldDef.m_vegetationInfos[j];
			if (worldDef.m_xmlGenerated)
			{
				delete vegInfo.m_instanceVbo;
				delete vegInfo.m_instanceIBO;
			}

			vegInfo.m_instanceVbo = nullptr;
			vegInfo.m_instanceIBO = nullptr;
		}
	}
}

WorldDefinition const* WorldDefinition::GetWorldDefinitionFromName(std::string const& name)
{
	for (int i = 0; i < (int)s_worldDefinitions.size(); ++i)
	{
		if (name == s_worldDefinitions[i].m_worldName)
		{
			return &s_worldDefinitions[i];
		}
	}

	return nullptr;
}

size_t WorldDefinition::GetTotalWorldDefSize()
{
	return (sizeof(WorldDefinition)) * WorldDefinition::s_worldDefinitions.capacity() + sizeof(WorldDefinition::s_worldDefinitions);
}


IntVec3 WorldDefinition::GetChunkSizeWithVoxelScale() const
{
	return IntVec3(CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z) * m_voxelScale;
}

//Parse XML
//----------------------------------------------------------------------------------------------------------------------------------------
void WorldDefinition::ParseSkyBoxInfo(XmlElement const& worldDefElement)
{
	m_skyboxXMLFilePath = ParseXmlAttribute(worldDefElement, "skyboxTextureInfo", "INVALID_FILE_PATH");

	XmlDocument skyBoxDefXML;
	XmlResult result = skyBoxDefXML.LoadFile(m_skyboxXMLFilePath.c_str());
	GUARANTEE_OR_DIE(result == tinyxml2::XML_SUCCESS, Stringf("Failed to open skybox definition document: %s", m_skyboxXMLFilePath.c_str()));
	XmlElement* rootElement = skyBoxDefXML.RootElement();
	GUARANTEE_OR_DIE(rootElement, Stringf("Failed to find root element in skybox def xml: %s", m_skyboxXMLFilePath.c_str()));

	std::string parentFolder = ParseXmlAttribute(*rootElement, "ParentFolder", "");
	std::string xFaceFilePath = parentFolder + ParseXmlAttribute(*rootElement, "XFace", "");
	std::string yFaceFilePath = parentFolder + ParseXmlAttribute(*rootElement, "YFace", "");
	std::string zFaceFilePath = parentFolder + ParseXmlAttribute(*rootElement, "ZFace", "");
	std::string negXFaceFilePath = parentFolder + ParseXmlAttribute(*rootElement, "NegXFace", "");
	std::string negYFaceFilePath = parentFolder + ParseXmlAttribute(*rootElement, "NegYFace", "");
	std::string negZFaceFilePath = parentFolder + ParseXmlAttribute(*rootElement, "NegZFace", "");
	
	std::vector<TextureDX12 const*> skyboxTextures;
	skyboxTextures.reserve(6);
	skyboxTextures.push_back(g_renderer->CreateOrGetTextureFromFile(xFaceFilePath.c_str()));
	skyboxTextures.push_back(g_renderer->CreateOrGetTextureFromFile(yFaceFilePath.c_str()));
	skyboxTextures.push_back(g_renderer->CreateOrGetTextureFromFile(zFaceFilePath.c_str()));
	skyboxTextures.push_back(g_renderer->CreateOrGetTextureFromFile(negXFaceFilePath.c_str()));
	skyboxTextures.push_back(g_renderer->CreateOrGetTextureFromFile(negYFaceFilePath.c_str()));
	skyboxTextures.push_back(g_renderer->CreateOrGetTextureFromFile(negZFaceFilePath.c_str()));

	m_skybox = new Skybox(g_renderer, skyboxTextures);

	std::string shaderFilePath = ParseXmlAttribute(*rootElement, "Shader", "");
	m_skybox->m_shader = g_renderer->CreateOrGetShaderFromFile(shaderFilePath.c_str());
	m_skybox->m_universalScale = SKYBOX_FAR_DISTANCE;
}

void WorldDefinition::ParseNoiseInfo(XmlElement const& worldDefElement)
{
	XmlElement const* noiseRoot = worldDefElement.FirstChildElement("Noise");
	m_gameSeed = ParseXmlAttribute(*noiseRoot, "gameSeed", m_gameSeed);
	m_defaultOctavePersistance = ParseXmlAttribute(*noiseRoot, "defaultOctavePersistance", m_defaultOctavePersistance);
	m_defaultOctaveScale = ParseXmlAttribute(*noiseRoot, "defaultOctaveScale", m_defaultOctaveScale);
	m_warpNoiseScaleXY = ParseXmlAttribute(*noiseRoot, "warpNoiseScale", m_warpNoiseScaleXY);
	m_warpNoiseNumOctaves = ParseXmlAttribute(*noiseRoot, "warpNoiseNumOctaves", m_warpNoiseNumOctaves);
	m_baseTerrainBiasFactor = ParseXmlAttribute(*noiseRoot, "baseTerrainBiasFactor", 0.f);
	m_finalTerrainBiasFactor = ParseXmlAttribute(*noiseRoot, "finalTerrainBiasFactor", 0.f);

	XmlElement const* noiseElement = noiseRoot->FirstChildElement("NoiseInfo");
	while (noiseElement != nullptr)
	{
		std::string worldNoiseTypeName = ParseXmlAttribute(*noiseElement, "type", "terrain");
		if (worldNoiseTypeName == "Terrain")
		{
			std::string terrainTypeName = ParseXmlAttribute(*noiseElement, "name", UN_INITIALIZED_WORLD_DATA_NAME);
			TerrainNoiseType terrainNoiseType = GetTerrainNoiseTypeFromName(terrainTypeName);

			TerrainNoiseInfo& terrainInfo = m_terrainNoiseInfos[(int)terrainNoiseType];
			terrainInfo.m_name = terrainTypeName;
			terrainInfo.m_worldNoiseType = WorldNoiseType::TERRAIN;
			terrainInfo.m_seed = m_gameSeed + ParseXmlAttribute(*noiseElement, "seed", 0);
			terrainInfo.m_isActive = ParseXmlAttribute(*noiseElement, "isActive", true);
			terrainInfo.m_scale = ParseXmlAttribute(*noiseElement, "scale", 1.f);
			terrainInfo.m_strength = ParseXmlAttribute(*noiseElement, "strength", 1.f);
			terrainInfo.m_numOctaves = ParseXmlAttribute(*noiseElement, "numOctaves", 2);
			terrainInfo.m_warpStrength = ParseXmlAttribute(*noiseElement, "warpStrength", 0.f);

			std::string noiseTypeName = ParseXmlAttribute(*noiseElement, "noiseType", "fractal");
			terrainInfo.m_noiseType = GetNoiseTypeFromName(noiseTypeName);

			XmlElement const* curveElement = noiseElement->FirstChildElement("Curve");
			while (curveElement != nullptr)
			{
				std::string curveType = ParseXmlAttribute(*curveElement, "type", "INVALID_TYPE");
				std::vector<CurveWithTStart<float>> curvesToAdd;
				XmlElement const* plotPointElement = curveElement->FirstChildElement("PlotPoint");
				if (plotPointElement == nullptr)
					continue;

				Vec2 curveStart = ParseXmlAttribute(*plotPointElement, "point", Vec2::ZERO);
				plotPointElement = plotPointElement->NextSiblingElement("PlotPoint");
				while (plotPointElement != nullptr)
				{
					CurveWithTStart<float> curve = GetCurveAndTPlotFromXMLElement(*plotPointElement, curveStart);
					curvesToAdd.push_back(curve);
					plotPointElement = plotPointElement->NextSiblingElement("PlotPoint");
				}

				if (curveType == "heightOffset")
				{
					terrainInfo.m_terrainHeightOffsetCurve = new PieceWiseCurve<float>(curvesToAdd, 1.f);
				}

				else if (curveType == "squashCurve")
				{
					terrainInfo.m_squashCurve = new PieceWiseCurve<float>(curvesToAdd, 1.f);
				}

				else
				{
					ERROR_AND_DIE(Stringf("Invalid Curve Name: %s", curveType.c_str()));
				}


				curveElement = curveElement->NextSiblingElement("Curve");
			}
		}

		else if (worldNoiseTypeName == "Vegetation")
		{
			std::string vegetationTypeName = ParseXmlAttribute(*noiseElement, "name", UN_INITIALIZED_WORLD_DATA_NAME);
			VegetationNoiseType vegetationNoiseType = GetVegetationNoiseTypeFromName(vegetationTypeName);

			VegetationNoiseInfo& vegInfo = m_vegetationNoiseInfos[(int)vegetationNoiseType];
			vegInfo.m_name = vegetationTypeName;
			vegInfo.m_worldNoiseType = WorldNoiseType::VEGETATION;
			vegInfo.m_seed = m_gameSeed + ParseXmlAttribute(*noiseElement, "seed", 0);
			vegInfo.m_scale = ParseXmlAttribute(*noiseElement, "scale", 1.f);
			vegInfo.m_strength = ParseXmlAttribute(*noiseElement, "strength", 1.f);
			vegInfo.m_numOctaves = ParseXmlAttribute(*noiseElement, "numOctaves", 2);
			vegInfo.m_warpStrength = ParseXmlAttribute(*noiseElement, "warpStrength", 0.f);
			std::string noiseTypeName = ParseXmlAttribute(*noiseElement, "noiseType", "fractal");
			vegInfo.m_noiseType = GetNoiseTypeFromName(noiseTypeName);
		}

		noiseElement = noiseElement->NextSiblingElement("NoiseInfo");
	}
}

void WorldDefinition::ParseTextureInfo(XmlElement const& worldDefElement)
{
	XmlElement const* textureInfoElement = worldDefElement.FirstChildElement("TextureInfo");
	GUARANTEE_OR_DIE(textureInfoElement != nullptr, Stringf("Could not find 'TextureInfo' Element for: %s", m_worldName.c_str()));

	m_terrainShaderFilePath = ParseXmlAttribute(*textureInfoElement, "terrainShader", "Data/Shaders/Terrain");
	m_terrainShader = g_renderer->CreateOrGetShaderFromFile(m_terrainShaderFilePath.c_str(), VertexType::VERTEX_TERRAIN);

	XmlElement const* biomeInfoElement = textureInfoElement->FirstChildElement("BiomeInfo");
	while (biomeInfoElement != nullptr)
	{
		std::string biomeName = ParseXmlAttribute(*biomeInfoElement, "biomeName", "UNAMED_BIOME");
		Biome biome = GetBiomeTypeFromName(biomeName);
		BiomeTextureInfo& biomeTextureInfo = m_biomeTextureInfos[(int)biome];
		biomeTextureInfo.m_biome = biome;

		XmlElement const* textureFilesElement = biomeInfoElement->FirstChildElement("TextureFiles");
		GUARANTEE_OR_DIE(textureFilesElement != nullptr, Stringf("Could not find first 'TextureFiles' child element for %s", biomeName.c_str()));
		ParseTextureFiles(*textureFilesElement, biomeTextureInfo);

		textureFilesElement = textureFilesElement->NextSiblingElement("TextureFiles");
		GUARANTEE_OR_DIE(textureFilesElement != nullptr, Stringf("Could not find second 'TextureFiles' child element for %s", biomeName.c_str()));
		ParseTextureFiles(*textureFilesElement, biomeTextureInfo);

		biomeInfoElement = biomeInfoElement->NextSiblingElement("BiomeInfo");
	}

}

void WorldDefinition::ParseTextureFiles(XmlElement const& textureFilesElement, BiomeTextureInfo& out_biomeTextureInfo)
{
	bool floors = ParseXmlAttribute(textureFilesElement, "floor", true);

	if (floors)
	{
		out_biomeTextureInfo.m_floorFiles.m_parentFolder = ParseXmlAttribute(textureFilesElement, "parentFolder", UN_INITIALIZED_WORLD_DATA_NAME);
		out_biomeTextureInfo.m_floorFiles.m_albedoFile = ParseXmlAttribute(textureFilesElement, "albedo", UN_INITIALIZED_WORLD_DATA_NAME);
		out_biomeTextureInfo.m_floorFiles.m_normalFile = ParseXmlAttribute(textureFilesElement, "normal", UN_INITIALIZED_WORLD_DATA_NAME);
		out_biomeTextureInfo.m_floorFiles.m_aoFile = ParseXmlAttribute(textureFilesElement, "ambientOcclusion", UN_INITIALIZED_WORLD_DATA_NAME);

		float floorTextureScale = ParseXmlAttribute(textureFilesElement, "textureScale", 1.f);
		float normalStrength = ParseXmlAttribute(textureFilesElement, "normalStrength", 1.f);
		int biomeIndex = (int)out_biomeTextureInfo.m_biome;
		m_textureConstants.m_textureScales[biomeIndex] = floorTextureScale;
		m_textureConstants.m_normalStrengths[biomeIndex] = normalStrength;

	}

	else
	{
		out_biomeTextureInfo.m_wallFiles.m_parentFolder = ParseXmlAttribute(textureFilesElement, "parentFolder", UN_INITIALIZED_WORLD_DATA_NAME);
		out_biomeTextureInfo.m_wallFiles.m_albedoFile = ParseXmlAttribute(textureFilesElement, "albedo", UN_INITIALIZED_WORLD_DATA_NAME);
		out_biomeTextureInfo.m_wallFiles.m_normalFile = ParseXmlAttribute(textureFilesElement, "normal", UN_INITIALIZED_WORLD_DATA_NAME);
		out_biomeTextureInfo.m_wallFiles.m_aoFile = ParseXmlAttribute(textureFilesElement, "ambientOcclusion", UN_INITIALIZED_WORLD_DATA_NAME);

		float wallTextureScale = ParseXmlAttribute(textureFilesElement, "textureScale", 1.f);
		float normalStrength = ParseXmlAttribute(textureFilesElement, "normalStrength", 1.f);
		int biomeIndex = (int)out_biomeTextureInfo.m_biome + (int)Biome::NUM_BIOMES;
		m_textureConstants.m_textureScales[biomeIndex] = wallTextureScale;
		m_textureConstants.m_normalStrengths[biomeIndex] = normalStrength;
	}
}

void WorldDefinition::ParseWaterInfo(XmlElement const& worldDefElement)
{
	XmlElement const* waterInfoElement = worldDefElement.FirstChildElement("WaterInfo");
	GUARANTEE_OR_DIE(waterInfoElement != nullptr, Stringf("Could not find 'WaterInfo' Element for %s", m_worldName.c_str()));
	m_waterSurfaceShaderFilePath = ParseXmlAttribute(*waterInfoElement, "waterShader", "Data/Shaders/WaterSurface");
	m_waterSurfaceShader = g_renderer->CreateOrGetShaderFromFile(m_waterSurfaceShaderFilePath.c_str(), VertexType::VERTEX_PCUTBN);
	m_waterNormalTextureFilePath = ParseXmlAttribute(*waterInfoElement, "normalTexture", "Data/Images/Water/WaterSurface_N.jpg");
	m_waterNormalTexture = g_renderer->CreateOrGetTextureFromFile(m_waterNormalTextureFilePath.c_str());
	m_waterFoamTextureFilePath = ParseXmlAttribute(*waterInfoElement, "foamTexture", UN_INITIALIZED_WORLD_DATA_NAME);
	m_waterFoamTexture = g_renderer->CreateOrGetTextureFromFile(m_waterFoamTextureFilePath.c_str());

	m_activeWaterSurface = ParseXmlAttribute(*waterInfoElement, "isActive", true);

	m_waterConstants.m_globalWaveSpeedScale = ParseXmlAttribute(*waterInfoElement, "globalWaveSpeedScale", 1.f);
	m_waterConstants.m_globalCrestSteepnessScale = ParseXmlAttribute(*waterInfoElement, "globalCrestSteepnessScale", 0.f);
	m_waterConstants.m_globalAmplitudeScale = ParseXmlAttribute(*waterInfoElement, "globalAmplitudeScale", 1.f);
	m_waterConstants.m_globalWavelengthScale = ParseXmlAttribute(*waterInfoElement, "globalWavelengthScale", 1.f);
	m_waterConstants.m_underwaterTintDepthScale = ParseXmlAttribute(*waterInfoElement, "underwaterTintDepthScale", m_waterConstants.m_underwaterTintDepthScale);
	m_waterConstants.m_underwaterDepthDarkenScale = ParseXmlAttribute(*waterInfoElement, "underwaterDepthDarkenScale", m_waterConstants.m_underwaterDepthDarkenScale);
	m_waterConstants.m_foaminess = ParseXmlAttribute(*waterInfoElement, "foaminess", m_waterConstants.m_foaminess);
	m_waterConstants.m_maxTerrainDepth = ParseXmlAttribute(worldDefElement, "maxTerrainDepth", 100.f);
	m_waterConstants.m_mirkyness= ParseXmlAttribute(*waterInfoElement, "mirkyness", 0.5f);
	m_waterConstants.m_reflectionStrength = ParseXmlAttribute(*waterInfoElement, "reflectionStrength", 0.5f);
	m_waterConstants.m_refractionStrength = ParseXmlAttribute(*waterInfoElement, "refractionStrength", 0.5f);
	m_waterConstants.m_shorelineFoamStrength = ParseXmlAttribute(*waterInfoElement, "shorelineFoamStrength", 0.5f);
	m_waterConstants.m_specularIntensity = ParseXmlAttribute(*waterInfoElement, "specularIntensity", 1.f);
	m_waterConstants.m_specularPower = ParseXmlAttribute(*waterInfoElement, "specularPower", 1.f);
	m_waterConstants.m_worldVoxelScale = (float)m_voxelScale;
	
	int numWaves = ParseXmlAttribute(*waterInfoElement, "numWaves", 0);
	numWaves = GetClampedInt(numWaves, 0, MAX_NUM_WAVES);

	int numFoundWaves = 0;
	XmlElement const* waveInfoElement = waterInfoElement->FirstChildElement("WaveInfo");
	while (waveInfoElement != nullptr)
	{
		WaveData waveData;
		waveData.m_directionXY = ParseXmlAttribute(*waveInfoElement, "directionXY", Vec2(1.f, 0.f));
		waveData.m_amplitude = ParseXmlAttribute(*waveInfoElement, "amplitude", 5.f);
		waveData.m_waveLength = ParseXmlAttribute(*waveInfoElement, "wavelength", 30.f);
		m_waterConstants.m_waveData[numFoundWaves] = waveData;

		WaveDataExtra extraWaveData;
		extraWaveData.m_waveSpeed = ParseXmlAttribute(*waveInfoElement, "speed", 1.f);
		extraWaveData.m_waveSteepness = ParseXmlAttribute(*waveInfoElement, "steepness", 1.f);
		extraWaveData.m_waveOffset = ParseXmlAttribute(*waveInfoElement, "waveOffset", 0.f);
		m_waterConstants.m_waveDataExtra[numFoundWaves] = extraWaveData;

		waveInfoElement = waveInfoElement->NextSiblingElement("WaveInfo");
		numFoundWaves++;
		if (numFoundWaves >= numWaves)
			break;

	}
	m_waterConstants.m_numWaves = GetClampedInt(numFoundWaves, 0, MAX_NUM_WAVES);

	XmlElement const* breakupInfoElement = waterInfoElement->FirstChildElement("WaveBreakupInfo");
	m_waterConstants.m_directionVarianceRegionScale = ParseXmlAttribute(*breakupInfoElement, "directionVarianceRegionScale", 0.f);
	m_waterConstants.m_amplitudeVarianceRegionScale = ParseXmlAttribute(*breakupInfoElement, "amplitudeVarianceRegionScale", 0.f);
	m_waterConstants.m_directionVariance = ParseXmlAttribute(*breakupInfoElement, "directionVariance", 0.f);
	m_waterConstants.m_amplitudeVariance = ParseXmlAttribute(*breakupInfoElement, "amplitudeVariance", 0.f);
}

void WorldDefinition::ParseWindInfo(XmlElement const& worldDefElement)
{
	XmlElement const* windRoot = worldDefElement.FirstChildElement("Wind");
	GUARANTEE_OR_DIE(windRoot, Stringf("Could not find 'Wind' Element for %s", m_worldName.c_str()));

	XmlElement const* windElement = windRoot->FirstChildElement("WindInfo");
	while (windElement)
	{
		std::string windTypeName = ParseXmlAttribute(*windElement,"type", UN_INITIALIZED_WORLD_DATA_NAME);
		WindType type = GetWindTypeFromName(windTypeName);

		m_activeWind[(int)type] = ParseXmlAttribute(*windElement, "isActive", false);

		WindConstants& currentConst = m_windConstants[(int)type];
		currentConst.m_baseSpeed = ParseXmlAttribute(*windElement, "baseSpeed", 1.f);
		currentConst.m_swirlStrength = ParseXmlAttribute(*windElement, "swirlStrength", 1.f);

		currentConst.m_warbleStrength = ParseXmlAttribute(*windElement, "warbleStrength", 1.f);
		currentConst.m_largeScale = ParseXmlAttribute(*windElement, "largeScale", 1.f);
		currentConst.m_largeSpeed = ParseXmlAttribute(*windElement, "largeSpeed", 1.f);
		currentConst.m_largeStrength = ParseXmlAttribute(*windElement, "largeStrength", 1.f);
		currentConst.m_mediumScale = ParseXmlAttribute(*windElement, "mediumScale", 1.f);
		currentConst.m_mediumSpeed = ParseXmlAttribute(*windElement, "mediumSpeed", 1.f);
		currentConst.m_mediumStrength = ParseXmlAttribute(*windElement, "mediumStrength", 1.f);
		currentConst.m_smallScale = ParseXmlAttribute(*windElement, "smallScale", 1.f);
		currentConst.m_smallSpeed = ParseXmlAttribute(*windElement, "smallSpeed", 1.f);
		currentConst.m_smallStrength = ParseXmlAttribute(*windElement, "smallStrength", 1.f);
		windElement = windElement->NextSiblingElement("WindInfo");
	}

}

void WorldDefinition::ParseSunInfo(XmlElement const& worldDefElement)
{
	XmlElement const* sunElement = worldDefElement.FirstChildElement("SunInfo");
	GUARANTEE_OR_DIE(sunElement, Stringf("Could not find 'SunInfo' Element for %s", m_worldName.c_str()));

	m_sunInfo.m_orientation = ParseXmlAttribute(*sunElement, "orientation", EulerAngles(45.f, 45.f, 0.f));
	m_sunInfo.m_intensity = ParseXmlAttribute(*sunElement, "intensity", 1.f);
	m_sunInfo.m_ambientIntensity = ParseXmlAttribute(*sunElement, "ambientIntensity", 0.5f);
	Rgba8 color = ParseXmlAttribute(*sunElement, "color", Rgba8::WHITE);
	color.GetAsFloats(m_sunInfo.m_color);
}

void WorldDefinition::ParseCausticsInfo(XmlElement const& worldDefElement)
{

	XmlElement const* causticInfoElement = worldDefElement.FirstChildElement("CausticsInfo");
	GUARANTEE_OR_DIE(causticInfoElement, Stringf("Could not find 'CausticInfo' Element for %s", m_worldName.c_str()));
	m_activeCaustics = ParseXmlAttribute(*causticInfoElement, "isActive", true);
	m_causticConstants.m_causticsIntensity = ParseXmlAttribute(*causticInfoElement,"intensity", 1.f);
	m_causticConstants.m_causticsDepthFadeRate = ParseXmlAttribute(*causticInfoElement,"depthFadeRate", 0.00015f);
	m_causticConstants.m_causticsWarpSpeed = ParseXmlAttribute(*causticInfoElement, "warpSpeed", 0.6f);
	m_causticConstants.m_causticsWarpStrength = ParseXmlAttribute(*causticInfoElement, "warpStrength", 0.05f);
	m_causticConstants.m_causticsLineThickness = ParseXmlAttribute(*causticInfoElement, "lineThickness", 0.3f);
	m_causticConstants.m_causticsRippleSpeed = ParseXmlAttribute(*causticInfoElement,"rippleSpeed", 1.f);
	m_causticConstants.m_causticsRippleStrength = ParseXmlAttribute(*causticInfoElement,"rippleStrength", 0.15f);
}

void WorldDefinition::ParseFogInfo(XmlElement const& worldDefElement)
{
	XmlElement const* fogInfoElement = worldDefElement.FirstChildElement("FogInfo");
	GUARANTEE_OR_DIE(fogInfoElement, Stringf("Could not find FogInfo element for: %s", m_worldName.c_str()));

	m_fogShaderFilePath = ParseXmlAttribute(*fogInfoElement,"fogShader","INVALID_SHADER");
	m_fogShader = g_renderer->CreateOrGetShaderFromFile(m_fogShaderFilePath.c_str());
	m_activeFog = ParseXmlAttribute(*fogInfoElement, "isActive", true);
	m_fogConstants.m_fogVolumeHeight = ParseXmlAttribute(*fogInfoElement, "fogVolumeHeight", 20.f);
	m_fogConstants.m_underwaterTintDepthScale = m_waterConstants.m_underwaterTintDepthScale;

	XmlElement const* fogTypeElement = fogInfoElement->FirstChildElement("FogType");
	while (fogTypeElement)
	{
		std::string typeName = ParseXmlAttribute(*fogTypeElement, "type", "INVALID_TYPE");
		int fogType = typeName == "underwater" ? UNDERWATER_FOG_INDEX : ABOVE_WATER_FOG_INDEX;
		//fogConst.m_isUnderwater = typeName == "underwater" ? 1 : 0;

		m_fogConstants.m_fogData[fogType].m_depth = ParseXmlAttribute(*fogTypeElement, "depth", 0.2f);
		m_fogConstants.m_fogData[fogType].m_opacity = ParseXmlAttribute(*fogTypeElement,"opacity", 1.f);
		m_fogConstants.m_fogData[fogType].m_transition = ParseXmlAttribute(*fogTypeElement, "transition", 1);

		Rgba8 color = ParseXmlAttribute(*fogTypeElement, "color", Rgba8::MAGENTA);
		color.GetAsFloats(m_fogConstants.m_fogData[fogType].m_color);

		fogTypeElement = fogTypeElement->NextSiblingElement("FogType");
	}
}

void WorldDefinition::ParseLightRayInfo(XmlElement const& worldDefElement)
{
	XmlElement const* lightRaysRoot = worldDefElement.FirstChildElement("LightRayInfo");
	GUARANTEE_OR_DIE(lightRaysRoot, Stringf("Could not find Light Rays root for: %s", m_worldName.c_str()));

	m_activeLightRays = ParseXmlAttribute(*lightRaysRoot, "isActive", true);
	m_lightRaysMaskShaderPath = ParseXmlAttribute(*lightRaysRoot, "maskShader", UN_INITIALIZED_WORLD_DATA_NAME);
	m_lightRaysMaskShader = g_renderer->CreateOrGetShaderFromFile(m_lightRaysMaskShaderPath.c_str());

	m_lightRaysVolumeComputeShaderPath = ParseXmlAttribute(*lightRaysRoot, "volumeComputeShader", UN_INITIALIZED_WORLD_DATA_NAME);
	m_lightRaysVolumeComputeShader = g_renderer->CreateOrGetComputeShaderFromFile(m_lightRaysVolumeComputeShaderPath.c_str());

	m_lightRayConstants.m_screenSize = Vec2(g_window->GetClientDimensions());
	m_lightRayConstants.m_shaftIntensity = ParseXmlAttribute(*lightRaysRoot,"shaftIntensity", 1.f);
	m_lightRayConstants.m_shaftScale = ParseXmlAttribute(*lightRaysRoot, "shaftScale", 1.f);
	m_lightRayConstants.m_shaftSpread = ParseXmlAttribute(*lightRaysRoot, "shaftSpread", 1.f);
	m_lightRayConstants.m_shaftWarpSpeed= ParseXmlAttribute(*lightRaysRoot, "shaftWarpSpeed", 1.f);
	m_lightRayConstants.m_shaftWarpStrength = ParseXmlAttribute(*lightRaysRoot, "shaftWarpStrength", 1.f);
	m_lightRayConstants.m_shaftDepthFade = ParseXmlAttribute(*lightRaysRoot, "shaftDepthFade", 1.f);
	m_lightRayConstants.m_shaftContrast = ParseXmlAttribute(*lightRaysRoot, "shaftContrast", 1.f);
	m_lightRayConstants.m_maxNumSteps = ParseXmlAttribute(*lightRaysRoot, "maxNumRaySteps", 64);
	m_lightRayConstants.m_renderLightRays = m_activeLightRays ? 1 : 0;


	Rgba8 shaftColor = ParseXmlAttribute(*lightRaysRoot, "shaftColor", Rgba8::WHITE);
	shaftColor.GetAsFloats(m_lightRayConstants.m_shaftColor);
}

void WorldDefinition::ParseParticleInfo(XmlElement const& worldDefElement)
{
	XmlElement const* particleRoot = worldDefElement.FirstChildElement("ParticleInfo");
	GUARANTEE_OR_DIE(particleRoot, Stringf("Could not find Particle root for: %s", m_worldName.c_str()));

	m_activeParticles = ParseXmlAttribute(*particleRoot, "isActive", true);
	m_particleConstants.m_activeParticles = m_activeParticles ? 1 : 0;
	m_particleConstants.m_density = ParseXmlAttribute(*particleRoot,"density", 0.f);
	m_particleConstants.m_sizeRange = ParseXmlAttribute(*particleRoot, "sizeRange", FloatRange::ZERO);
	m_particleConstants.m_cellSize = ParseXmlAttribute(*particleRoot, "cellSize", 0.f);
	m_particleConstants.m_particlesPerCell = ParseXmlAttribute(*particleRoot, "particlesPerCell", 1);
	m_particleConstants.m_density = ParseXmlAttribute(*particleRoot, "density", 0.f);
	m_particleConstants.m_depthFade = ParseXmlAttribute(*particleRoot, "depthFade", 0.f);
	m_particleConstants.m_driftSpeed = ParseXmlAttribute(*particleRoot, "driftSpeed", 0.f);
	m_particleConstants.m_intensity = ParseXmlAttribute(*particleRoot, "intensity", 0.f);
}

void WorldDefinition::ParseFishInfo(XmlElement const& worldDefElement)
{
	XmlElement const* fishRoot = worldDefElement.FirstChildElement("Fish");
	GUARANTEE_OR_DIE(fishRoot, Stringf("Could not find Fish root for: %s", m_worldName.c_str()));

	m_activeFish = ParseXmlAttribute(*fishRoot, "isActive", true);
	m_maxNumFish = ParseXmlAttribute(*fishRoot, "maxFish", 1000);
	m_maxNumFishNeighborsToCheck = ParseXmlAttribute(*fishRoot, "maxNeighborsToCheckPerFrame", 25);

	XmlElement const* fishElement = fishRoot->FirstChildElement("FishInfo");
	while (fishElement != nullptr)
	{
		std::string fishTypeName = ParseXmlAttribute(*fishElement, "type", UN_INITIALIZED_WORLD_DATA_NAME);
		FishType fishType = GetFishTypeFromName(fishTypeName);
		FishInfo& fishInfo = m_fishInfos[(int)fishType];
		fishInfo.m_type = fishType;
		fishInfo.m_isActive = ParseXmlAttribute(*fishElement, "isActive", true);
		fishInfo.m_meshInfo = ParseXmlAttribute(*fishElement, "meshInfo", "INVALID_PATH");
		fishInfo.m_maxSpeed = ParseXmlAttribute(*fishElement, "maxSpeed", 50.f);
		fishInfo.m_maxTurnSpeed = ParseXmlAttribute(*fishElement, "maxTurnSpeed", 50.f);
		fishInfo.m_spawnThickness = ParseXmlAttribute(*fishElement, "spawnThickness", 5);
		fishInfo.m_maxNumber = ParseXmlAttribute(*fishElement, "maxNumber", 300);
		float flockingRadius = ParseXmlAttribute(*fishElement, "flockingRadius", 20.f);
		fishInfo.m_flockingRadiusSqrd = flockingRadius * flockingRadius;
		float separationRadius = ParseXmlAttribute(*fishElement, "separationRadius", 10.f);
		fishInfo.m_separationRadiusSqrd = separationRadius * separationRadius;
		float playerFlockingRadius = ParseXmlAttribute(*fishElement, "playerFlockingRadius", 500.f);
		fishInfo.m_playerFlockingRadiusSqrd = playerFlockingRadius * playerFlockingRadius;
		float playerSeparationRadius = ParseXmlAttribute(*fishElement, "playerSeparationRadius", 20.f);
		fishInfo.m_playerSeparationRadiusSqrd = playerSeparationRadius * playerSeparationRadius;

		fishInfo.m_alignment = ParseXmlAttribute(*fishElement, "alignment", 3.f);
		fishInfo.m_cohesion = ParseXmlAttribute(*fishElement, "cohesion", 3.f);
		fishInfo.m_separation = ParseXmlAttribute(*fishElement, "separation", 3.f);
		fishInfo.m_terrainAvoidance = ParseXmlAttribute(*fishElement, "terrainAvoidance", 8.f);
		fishInfo.m_probeDistance = ParseXmlAttribute(*fishElement, "probeDistance", 3.f);
		fishInfo.m_minOceanDepth = ParseXmlAttribute(*fishElement, "minOceanDepth", 3.f);
		fishInfo.m_playerCohesion = ParseXmlAttribute(*fishElement, "playerCohesion", 10.f);
		fishInfo.m_playerSeparation = ParseXmlAttribute(*fishElement, "playerSeparation", 1.f);

		fishElement = fishElement->NextSiblingElement("FishInfo");
	}
}

void WorldDefinition::ParseVegetationInfo(XmlElement const& worldDefElement)
{
	m_smallestVegetationDotProductThreshold = 1.f;
	XmlElement const* vegRoot = worldDefElement.FirstChildElement("Vegetation");
	GUARANTEE_OR_DIE(vegRoot, Stringf("Could not find Vegetation root for: %s", m_worldName.c_str()));
	m_activeVegetation = ParseXmlAttribute(*vegRoot, "isActive", true);
	m_coralClusterThickness = ParseXmlAttribute(*vegRoot, "coralClusterThickness", 0.f);
	m_coralBlendConstants.m_depthBlendStrength = ParseXmlAttribute(*vegRoot, "coralBlendStrength", 0.f);
	m_coralBlendConstants.m_depthBlendThreshold = ParseXmlAttribute(*vegRoot, "coralBlendDepthThreshold", 0.f);
	m_coralBlendConstants.m_blendRadius = ParseXmlAttribute(*vegRoot, "coralBlendRadius", 0.f);
	m_coralBlendPostProcessShaderFilePath = ParseXmlAttribute(*vegRoot,"coralBlendShader", UN_INITIALIZED_WORLD_DATA_NAME);
	m_coralBlendPostProcessShader = g_renderer->CreateOrGetShaderFromFile(m_coralBlendPostProcessShaderFilePath.c_str(), VertexType::VERTEX_PCU);

	XmlElement const* vegElement = vegRoot->FirstChildElement("VegetationInfo");
	while (vegElement != nullptr)
	{
		std::string vegTypeName = ParseXmlAttribute(*vegElement, "type", UN_INITIALIZED_WORLD_DATA_NAME);
		VegetationType vegType = GetVegetationTypeFromName(vegTypeName);
		if (vegType != VegetationType::UN_INITIALIZED)
		{
			VegetationInfo& vegInfo = m_vegetationInfos[(int)vegType];
			vegInfo.m_type = vegType;
			vegInfo.m_isActive = ParseXmlAttribute(*vegElement, "isActive", false);
			vegInfo.m_worldDotUpThreshold = ParseXmlAttribute(*vegElement, "worldDotUpThreshold", 1.f);
			m_smallestVegetationDotProductThreshold = GetMin(m_smallestVegetationDotProductThreshold, vegInfo.m_worldDotUpThreshold);
			vegInfo.m_thickness = ParseXmlAttribute(*vegElement, "thickness", 0.f);

			vegInfo.m_computeShaderName = ParseXmlAttribute(*vegElement, "computeShader", UN_INITIALIZED_WORLD_DATA_NAME);
			vegInfo.m_computeShader = g_renderer->CreateOrGetComputeShaderFromFile(vegInfo.m_computeShaderName.c_str());
			vegInfo.m_diffuseTexName = ParseXmlAttribute(*vegElement, "texture", UN_INITIALIZED_WORLD_DATA_NAME);
			vegInfo.m_normalTexName = ParseXmlAttribute(*vegElement, "normalTexture", UN_INITIALIZED_WORLD_DATA_NAME);
			vegInfo.m_meshInfo = ParseXmlAttribute(*vegElement,"meshInfo", UN_INITIALIZED_WORLD_DATA_NAME);

			vegInfo.m_graphicsShaderName = ParseXmlAttribute(*vegElement, "graphicsShader", UN_INITIALIZED_WORLD_DATA_NAME);
			vegInfo.m_graphicsShader = g_renderer->CreateOrGetShaderFromFile(vegInfo.m_graphicsShaderName.c_str(), VertexType::VERTEX_PCUTBN);
			if (IsVegetationTypeCoral(vegType))
			{
				FloatRange scale = ParseXmlAttribute(*vegElement, "scale", FloatRange::ZERO_TO_ONE);
				vegInfo.m_vegConstants.m_heightRange = scale;
				vegInfo.m_vegConstants.m_widthRange = scale;
			}
			else
			{
				vegInfo.m_vegConstants.m_heightRange = ParseXmlAttribute(*vegElement, "height", FloatRange::ZERO_TO_ONE);
				vegInfo.m_vegConstants.m_widthRange = ParseXmlAttribute(*vegElement, "width", FloatRange::ZERO_TO_ONE);
			}

			vegInfo.m_vegConstants.m_rigidity = ParseXmlAttribute(*vegElement, "rigidity", 0.1f);
			vegInfo.m_vegConstants.m_emissiveStrength = ParseXmlAttribute(*vegElement, "emissiveStrength", 1.f);
		}
		
		vegElement = vegElement->NextSiblingElement("VegetationInfo");
	}
}



//Save Data
//----------------------------------------------------------------------------------------------------------------------------------------
void WorldDefinition::SetUsableName()
{
	std::string baseName = m_worldName;

	// Strip any existing suffix like "_1", "_2", etc.
	size_t underscorePos = baseName.find_last_of('_');
	if (underscorePos != std::string::npos)
	{
		std::string after = baseName.substr(underscorePos + 1);
		if (std::all_of(after.begin(), after.end(), ::isdigit))
		{
			baseName = baseName.substr(0, underscorePos);
		}
	}

	int highestNumber = 0;

	for (auto const& def : s_worldDefinitions)
	{
		// Skip self
		if (&def == this)
			continue;

		if (def.m_worldName == baseName)
		{
			highestNumber = GetMax(highestNumber, 1);
		}
		else if (def.m_worldName.rfind(baseName + "_", 0) == 0)
		{
			std::string suffix = def.m_worldName.substr(baseName.size() + 1);
			if (std::all_of(suffix.begin(), suffix.end(), ::isdigit))
			{
				int num = std::stoi(suffix);
				highestNumber = GetMax(highestNumber, num);
			}
		}
	}

	if (highestNumber > 0 || std::any_of(s_worldDefinitions.begin(), s_worldDefinitions.end(),
		[&](auto const& d) { return d.m_worldName == baseName; }))
	{
		m_worldName = Stringf("%s_%i", baseName.c_str(), highestNumber + 1);
	}
	else
	{
		m_worldName = baseName;
	}
}

void WorldDefinition::SaveDefinition(WorldDefinition const& worldDef)
{
	s_worldDefinitions.push_back(worldDef);
}

void WorldDefinition::SaveDefinitionAsXML() const
{
	std::string worldName = m_worldName;
	if (worldName == "DefaultWorld")
	{
		worldName = "DefaultWorld_Copy";
	}

	XmlDocument doc;

	XmlElement* root = doc.NewElement("Definitions");
	doc.InsertFirstChild(root);
	XmlElement* world = doc.NewElement("WorldDefinition");

	//Base world settings
	{
		world->SetAttribute("worldName", worldName.c_str());
		world->SetAttribute("voxelScale", m_voxelScale);
		world->SetAttribute("seaLevel", m_waterConstants.m_seaLevel);
		world->SetAttribute("baseTerrainDepth", m_baseTerrainDepthBelowSeaLevel);
		world->SetAttribute("maxTerrainDepth", m_waterConstants.m_maxTerrainDepth);
		world->SetAttribute("worldHeight", m_worldHeight);
		world->SetAttribute("skyboxTextureInfo", m_skyboxXMLFilePath.c_str());
		world->SetAttribute("biomeBlend", m_biomeBlend);
		root->InsertEndChild(world);
	}

	//Noise settings
	{
		XmlElement* noise = doc.NewElement("Noise");
		noise->SetAttribute("gameSeed", m_gameSeed);
		noise->SetAttribute("defaultOctavePersistance", m_defaultOctavePersistance);
		noise->SetAttribute("defaultOctaveScale", m_defaultOctaveScale);
		noise->SetAttribute("warpNoiseScale", m_warpNoiseScaleXY.GetAsText().c_str());
		noise->SetAttribute("warpNoiseNumOctaves", m_warpNoiseNumOctaves);
		noise->SetAttribute("baseTerrainBiasFactor", m_baseTerrainBiasFactor);
		noise->SetAttribute("finalTerrainBiasFactor", m_finalTerrainBiasFactor);

		for (int i = 0; i < (int)TerrainNoiseType::COUNT; ++i)
		{
			TerrainNoiseInfo const& info = m_terrainNoiseInfos[i];
			if(info.m_name == UN_INITIALIZED_WORLD_DATA_NAME)
				continue;

			XmlElement* terrainNoiseInfo = doc.NewElement("NoiseInfo");
			std::string noiseTypeName = GetNoiseTypeNameFromType(info.m_noiseType);

			terrainNoiseInfo->SetAttribute("name", info.m_name.c_str());
			terrainNoiseInfo->SetAttribute("type", "Terrain");
			terrainNoiseInfo->SetAttribute("noiseType", noiseTypeName.c_str());
			terrainNoiseInfo->SetAttribute("seed", info.m_seed - m_gameSeed);
			terrainNoiseInfo->SetAttribute("isActive", info.m_isActive);
			terrainNoiseInfo->SetAttribute("scale", info.m_scale);
			terrainNoiseInfo->SetAttribute("strength", info.m_strength);
			terrainNoiseInfo->SetAttribute("numOctaves", info.m_numOctaves);
			terrainNoiseInfo->SetAttribute("warpStrength", info.m_warpStrength);

			constexpr int NUM_CURVES = 2;

			PieceWiseCurve<float>* curves[NUM_CURVES] = { info.m_terrainHeightOffsetCurve , info.m_squashCurve };
			std::string curveTypes[NUM_CURVES] = { "heightOffset", "squashCurve" };

			for (int n = 0; n < NUM_CURVES; ++n)
			{
				if (curves[n])
				{
					XmlElement* curveElement = doc.NewElement("Curve");
					curveElement->SetAttribute("type", curveTypes[n].c_str());

					std::vector<CurveWithTStart<float>> curvesWithT = curves[n]->GetCurves();
					for (int j = 0; j < (int)curvesWithT.size(); ++j)
					{
						XmlElement* plotPointElement = doc.NewElement("PlotPoint");
						Vec2 curvePoint(curvesWithT[j].m_tStart, curvesWithT[j].m_curve->m_start);

						int curveIndex = j == 0 ? j : j - 1;
						int exponent = -1;
						std::string curveType = GetCurveTypeName(curvesWithT[curveIndex].m_curve, exponent);

						plotPointElement->SetAttribute("point", curvePoint.GetAsText().c_str());
						plotPointElement->SetAttribute("type", curveType.c_str());
						if (exponent > 0)
							plotPointElement->SetAttribute("exponent", exponent);

						curveElement->InsertEndChild(plotPointElement);

						if (j == (int)curvesWithT.size() - 1) // last curve needs to add an end point as well
						{
							plotPointElement = doc.NewElement("PlotPoint");
							curvePoint = Vec2(1.f, curvesWithT[j].m_curve->m_end);
							exponent = -1;
							curveType = GetCurveTypeName(curvesWithT[j - 1].m_curve, exponent);

							plotPointElement->SetAttribute("point", curvePoint.GetAsText().c_str());
							plotPointElement->SetAttribute("type", curveType.c_str());
							if (exponent > 0)
								plotPointElement->SetAttribute("exponent", exponent);

							curveElement->InsertEndChild(plotPointElement);
						}

					}
					terrainNoiseInfo->InsertEndChild(curveElement);
				}

				else
				{
					// Force closing tag by adding empty text
					terrainNoiseInfo->SetText("");
				}

			}

			noise->InsertEndChild(terrainNoiseInfo);
		}

		for (int i = 0; i < (int)VegetationNoiseType::COUNT; ++i)
		{

			VegetationNoiseInfo const& info = m_vegetationNoiseInfos[i];
			if (info.m_name == UN_INITIALIZED_WORLD_DATA_NAME)
				continue;

			XmlElement* vegetationNoiseInfo = doc.NewElement("NoiseInfo");
			std::string noiseTypeName = GetNoiseTypeNameFromType(info.m_noiseType);

			vegetationNoiseInfo->SetAttribute("name", info.m_name.c_str());
			vegetationNoiseInfo->SetAttribute("type", "Vegetation");
			vegetationNoiseInfo->SetAttribute("noiseType", noiseTypeName.c_str());
			vegetationNoiseInfo->SetAttribute("seed", info.m_seed - m_gameSeed);
			vegetationNoiseInfo->SetAttribute("scale", info.m_scale);
			vegetationNoiseInfo->SetAttribute("strength", info.m_strength);
			vegetationNoiseInfo->SetAttribute("numOctaves", info.m_numOctaves);
			vegetationNoiseInfo->SetAttribute("warpStrength", info.m_warpStrength);
			noise->InsertEndChild(vegetationNoiseInfo);
			
		}

		world->InsertEndChild(noise);
	}

	//Texture Settings
	{
		XmlElement* textureInfoRoot = doc.NewElement("TextureInfo");
		textureInfoRoot->SetAttribute("terrainShader", m_terrainShaderFilePath.c_str());
		world->InsertEndChild(textureInfoRoot);

		for (int i = 0; i < (int)Biome::NUM_BIOMES; ++i)
		{
			XmlElement* biomeRoot = doc.NewElement("BiomeInfo");

			BiomeTextureInfo biomeTexInfo = m_biomeTextureInfos[i];
			std::string biomeName = GetBiomeNameFromType((Biome)i);
			biomeRoot->SetAttribute("biomeName", biomeName.c_str());
			textureInfoRoot->InsertEndChild(biomeRoot);

			XmlElement* texFilesFloor = doc.NewElement("TextureFiles");
			texFilesFloor->SetAttribute("floor", true);
			texFilesFloor->SetAttribute("textureScale", m_textureConstants.m_textureScales[i]);
			texFilesFloor->SetAttribute("normalStrength", m_textureConstants.m_normalStrengths[i]);
			texFilesFloor->SetAttribute("parentFolder", biomeTexInfo.m_floorFiles.m_parentFolder.c_str());
			texFilesFloor->SetAttribute("albedo", biomeTexInfo.m_floorFiles.m_albedoFile.c_str());
			texFilesFloor->SetAttribute("normal", biomeTexInfo.m_floorFiles.m_normalFile.c_str());
			texFilesFloor->SetAttribute("ambientOcclusion", biomeTexInfo.m_floorFiles.m_aoFile.c_str());
			biomeRoot->InsertEndChild(texFilesFloor);

			XmlElement* texFilesWall = doc.NewElement("TextureFiles");
			texFilesWall->SetAttribute("floor", false);
			texFilesWall->SetAttribute("textureScale", m_textureConstants.m_textureScales[i + (int)Biome::NUM_BIOMES]);
			texFilesFloor->SetAttribute("normalStrength", m_textureConstants.m_normalStrengths[i + (int)Biome::NUM_BIOMES]);
			texFilesWall->SetAttribute("parentFolder", biomeTexInfo.m_wallFiles.m_parentFolder.c_str());
			texFilesWall->SetAttribute("albedo", biomeTexInfo.m_wallFiles.m_albedoFile.c_str());
			texFilesWall->SetAttribute("normal", biomeTexInfo.m_wallFiles.m_normalFile.c_str());
			texFilesWall->SetAttribute("ambientOcclusion", biomeTexInfo.m_wallFiles.m_aoFile.c_str());
			biomeRoot->InsertEndChild(texFilesWall);
		}

	}

	//Water settings
	{
		XmlElement* waterRoot = doc.NewElement("WaterInfo");
		waterRoot->SetAttribute("waterShader", m_waterSurfaceShaderFilePath.c_str());
		waterRoot->SetAttribute("normalTexture", m_waterNormalTextureFilePath.c_str());
		waterRoot->SetAttribute("foamTexture", m_waterFoamTextureFilePath.c_str());
		waterRoot->SetAttribute("isActive", m_activeWaterSurface);
		waterRoot->SetAttribute("waterShader", m_waterSurfaceShaderFilePath.c_str());
		waterRoot->SetAttribute("underwaterTintDepthScale", m_waterConstants.m_underwaterTintDepthScale);
		waterRoot->SetAttribute("underwaterDepthDarkenScale", m_waterConstants.m_underwaterDepthDarkenScale);
		waterRoot->SetAttribute("numWaves", m_waterConstants.m_numWaves);
		waterRoot->SetAttribute("globalWaveSpeedScale", m_waterConstants.m_globalWaveSpeedScale);
		waterRoot->SetAttribute("globalCrestSteepnessScale", m_waterConstants.m_globalCrestSteepnessScale);
		waterRoot->SetAttribute("globalAmplitudeScale", m_waterConstants.m_globalAmplitudeScale);
		waterRoot->SetAttribute("globalWavelengthScale", m_waterConstants.m_globalWavelengthScale);
		waterRoot->SetAttribute("foaminess", m_waterConstants.m_foaminess);
		waterRoot->SetAttribute("mirkyness", m_waterConstants.m_mirkyness);
		waterRoot->SetAttribute("reflectionStrength", m_waterConstants.m_reflectionStrength);
		waterRoot->SetAttribute("refractionStrength", m_waterConstants.m_reflectionStrength);
		waterRoot->SetAttribute("shorelineFoamStrength", m_waterConstants.m_shorelineFoamStrength);
		waterRoot->SetAttribute("specularIntensity", m_waterConstants.m_specularIntensity);
		waterRoot->SetAttribute("specularPower", m_waterConstants.m_specularPower);
		world->InsertEndChild(waterRoot);

		//Waves
		for (int i = 0; i < m_waterConstants.m_numWaves; ++i)
		{
			XmlElement* waveInfo = doc.NewElement("WaveInfo");
			waveInfo->SetAttribute("directionXY", m_waterConstants.m_waveData[i].m_directionXY.GetAsText().c_str());
			waveInfo->SetAttribute("amplitude", m_waterConstants.m_waveData[i].m_amplitude);
			waveInfo->SetAttribute("wavelength", m_waterConstants.m_waveData[i].m_waveLength);
			waveInfo->SetAttribute("speed", m_waterConstants.m_waveDataExtra[i].m_waveSpeed);
			waveInfo->SetAttribute("steepness", m_waterConstants.m_waveDataExtra[i].m_waveSteepness);
			waveInfo->SetAttribute("waveOffset", m_waterConstants.m_waveDataExtra[i].m_waveOffset);
			waterRoot->InsertEndChild(waveInfo);
		}

		XmlElement* waveBreakupInfo = doc.NewElement("WaveBreakupInfo");
		waveBreakupInfo->SetAttribute("directionVarianceRegionScale", m_waterConstants.m_directionVarianceRegionScale);
		waveBreakupInfo->SetAttribute("amplitudeVarianceRegionScale", m_waterConstants.m_amplitudeVarianceRegionScale);
		waveBreakupInfo->SetAttribute("directionVariance", m_waterConstants.m_directionVariance);
		waveBreakupInfo->SetAttribute("amplitudeVariance", m_waterConstants.m_amplitudeVariance);
		waterRoot->InsertEndChild(waveBreakupInfo);
	}

	//Wind
	{
		XmlElement* currentsRoot = doc.NewElement("Wind");
		for (int i = 0; i < (int)WindType::COUNT; ++i)
		{
			XmlElement* windElem = doc.NewElement("WindInfo");
			WindConstants const& windConst = m_windConstants[i];
			windElem->SetAttribute("isActive", m_activeWind[i]);
			std::string typeName = GetWindTypeNameFromType((WindType)i);
			windElem->SetAttribute("type", typeName.c_str());
			windElem->SetAttribute("baseSpeed", windConst.m_baseSpeed);
			windElem->SetAttribute("swirlStrength", windConst.m_swirlStrength);
			windElem->SetAttribute("warbleStrength", windConst.m_warbleStrength);
			windElem->SetAttribute("largeScale", windConst.m_largeScale);
			windElem->SetAttribute("largeSpeed", windConst.m_largeSpeed);
			windElem->SetAttribute("largeStrength", windConst.m_largeStrength);
			windElem->SetAttribute("mediumScale", windConst.m_mediumScale);
			windElem->SetAttribute("mediumSpeed", windConst.m_mediumSpeed);
			windElem->SetAttribute("mediumStrength", windConst.m_mediumStrength);
			windElem->SetAttribute("smallScale", windConst.m_smallScale);
			windElem->SetAttribute("smallSpeed", windConst.m_smallSpeed);
			windElem->SetAttribute("smallStrength", windConst.m_smallStrength);
			currentsRoot->InsertEndChild(windElem);
		}
		
		world->InsertEndChild(currentsRoot);

	}

	//Sun Info
	{
		XmlElement* sunRoot = doc.NewElement("SunInfo");
		sunRoot->SetAttribute("orientation", m_sunInfo.m_orientation.GetAsText().c_str());
		sunRoot->SetAttribute("intensity", m_sunInfo.m_intensity);
		sunRoot->SetAttribute("ambientIntensity", m_sunInfo.m_ambientIntensity);
		sunRoot->SetAttribute("color", Rgba8::GetAsDenormalizedColor(m_sunInfo.m_color).GetAsText().c_str());
		world->InsertEndChild(sunRoot);
	}

	//Caustics
	{
		XmlElement* causticsRoot = doc.NewElement("CausticsInfo");
		causticsRoot->SetAttribute("isActive", m_activeCaustics);
		causticsRoot->SetAttribute("intensity", m_causticConstants.m_causticsIntensity);
		causticsRoot->SetAttribute("depthFadeRate", m_causticConstants.m_causticsDepthFadeRate);
		causticsRoot->SetAttribute("warpSpeed", m_causticConstants.m_causticsWarpSpeed);
		causticsRoot->SetAttribute("warpStrength", m_causticConstants.m_causticsWarpStrength);
		causticsRoot->SetAttribute("lineThickness", m_causticConstants.m_causticsLineThickness);
		causticsRoot->SetAttribute("rippleSpeed", m_causticConstants.m_causticsRippleSpeed);
		causticsRoot->SetAttribute("rippleStrength", m_causticConstants.m_causticsRippleStrength);
		world->InsertEndChild(causticsRoot);
	}

	//Light Rays
	{
		XmlElement* lightRayRoot = doc.NewElement("LightRayInfo");
		lightRayRoot->SetAttribute("isActive", m_activeLightRays);
		lightRayRoot->SetAttribute("maskShader", m_lightRaysMaskShaderPath.c_str());
		lightRayRoot->SetAttribute("volumeComputeShader", m_lightRaysVolumeComputeShaderPath.c_str());
		lightRayRoot->SetAttribute("shaftIntensity", m_lightRayConstants.m_shaftIntensity);
		lightRayRoot->SetAttribute("shaftScale", m_lightRayConstants.m_shaftScale);
		lightRayRoot->SetAttribute("shaftSpread", m_lightRayConstants.m_shaftSpread);
		lightRayRoot->SetAttribute("shaftWarpSpeed", m_lightRayConstants.m_shaftWarpSpeed);
		lightRayRoot->SetAttribute("shaftWarpStrength", m_lightRayConstants.m_shaftWarpStrength);
		lightRayRoot->SetAttribute("shaftDepthFade", m_lightRayConstants.m_shaftDepthFade);
		lightRayRoot->SetAttribute("shaftContrast", m_lightRayConstants.m_shaftContrast);
		Rgba8 shaftColor = Rgba8::GetAsDenormalizedColor(m_lightRayConstants.m_shaftColor);
		lightRayRoot->SetAttribute("shaftColor", shaftColor.GetAsText().c_str());

		lightRayRoot->SetAttribute("maxNumRaySteps", m_lightRayConstants.m_maxNumSteps);
		
		world->InsertEndChild(lightRayRoot);
	}

	//Particles
	{
		XmlElement* particleRoot = doc.NewElement("ParticleInfo");
		particleRoot->SetAttribute("isActive", m_activeParticles);
		particleRoot->SetAttribute("density", m_particleConstants.m_density);
		particleRoot->SetAttribute("cellSize", m_particleConstants.m_cellSize);
		particleRoot->SetAttribute("particlesPerCell", m_particleConstants.m_particlesPerCell);
		particleRoot->SetAttribute("driftSpeed", m_particleConstants.m_driftSpeed);
		particleRoot->SetAttribute("intensity", m_particleConstants.m_intensity);
		particleRoot->SetAttribute("depthFade", m_particleConstants.m_depthFade);
		particleRoot->SetAttribute("sizeRange", m_particleConstants.m_sizeRange.GetAsText().c_str());
		world->InsertEndChild(particleRoot);
	}

	//Fog
	{
		XmlElement* fogRoot = doc.NewElement("FogInfo");
		fogRoot->SetAttribute("fogShader", m_fogShaderFilePath.c_str());
		fogRoot->SetAttribute("isActive", m_activeFog);
		fogRoot->SetAttribute("fogVolumeHeight", m_fogConstants.m_fogVolumeHeight);

		constexpr int NUM_FOG_TYPES = 2;
		std::string fogNames[NUM_FOG_TYPES] = { "underwater", "aboveSurface" };

		for (int i = 0; i < NUM_FOG_TYPES; ++i)
		{
			FogData const& fogData = m_fogConstants.m_fogData[i];
			XmlElement* fogTypeElement = doc.NewElement("FogType");
			fogTypeElement->SetAttribute("type", fogNames[i].c_str());
			fogTypeElement->SetAttribute("depth", fogData.m_depth);
			fogTypeElement->SetAttribute("opacity", fogData.m_opacity);
			fogTypeElement->SetAttribute("transition", fogData.m_transition);
			Rgba8 fogColor = Rgba8::GetAsDenormalizedColor(fogData.m_color);
			fogTypeElement->SetAttribute("color", fogColor.GetAsText().c_str());
			fogRoot->InsertEndChild(fogTypeElement);

		}

		world->InsertEndChild(fogRoot);
	}


	//Fish
	{
		XmlElement* fishRoot = doc.NewElement("Fish");
		fishRoot->SetAttribute("isActive", m_activeFish);
		fishRoot->SetAttribute("maxFish", m_maxNumFish);
		fishRoot->SetAttribute("maxNeighborsToCheckPerFrame", m_maxNumFishNeighborsToCheck);

		for (int i = 0; i < (int)FishType::COUNT; ++i)
		{
			FishInfo fishInfo = m_fishInfos[i];
			std::string fishName = GetFishTypeNameFromType(fishInfo.m_type);
			if(fishName == UN_INITIALIZED_WORLD_DATA_NAME)
				continue;

			XmlElement* fishInfoElement = doc.NewElement("FishInfo");


			fishInfoElement->SetAttribute("type", fishName.c_str());
			fishInfoElement->SetAttribute("isActive", fishInfo.m_isActive);
			fishInfoElement->SetAttribute("meshInfo", fishInfo.m_meshInfo.c_str());
			fishInfoElement->SetAttribute("maxNumber", fishInfo.m_maxNumber);
			fishInfoElement->SetAttribute("spawnThickness", fishInfo.m_spawnThickness);
			fishInfoElement->SetAttribute("maxSpeed", fishInfo.m_maxSpeed);
			fishInfoElement->SetAttribute("maxTurnSpeed", fishInfo.m_maxTurnSpeed);
			fishInfoElement->SetAttribute("flockingRadius", sqrtf(fishInfo.m_flockingRadiusSqrd));
			fishInfoElement->SetAttribute("separationRadius", sqrtf(fishInfo.m_separationRadiusSqrd));
			fishInfoElement->SetAttribute("playerFlockingRadius", sqrtf(fishInfo.m_playerFlockingRadiusSqrd));
			fishInfoElement->SetAttribute("playerSeparationRadius", sqrtf(fishInfo.m_playerSeparationRadiusSqrd));
			fishInfoElement->SetAttribute("probeDistance", fishInfo.m_probeDistance);
			fishInfoElement->SetAttribute("terrainAvoidance", fishInfo.m_terrainAvoidance);
			fishInfoElement->SetAttribute("cohesion", fishInfo.m_cohesion);
			fishInfoElement->SetAttribute("alignment", fishInfo.m_alignment);
			fishInfoElement->SetAttribute("separation", fishInfo.m_separation);
			fishInfoElement->SetAttribute("minOceanDepth", fishInfo.m_minOceanDepth);
			fishInfoElement->SetAttribute("playerCohesion", fishInfo.m_playerCohesion);
			fishInfoElement->SetAttribute("playerSeparation", fishInfo.m_playerSeparation);
			fishRoot->InsertEndChild(fishInfoElement);

		}

		world->InsertEndChild(fishRoot);

	}

	//Vegetation
	{
		XmlElement* vegRoot = doc.NewElement("Vegetation");
		vegRoot->SetAttribute("isActive", m_activeVegetation);
		vegRoot->SetAttribute("coralClusterThickness", m_coralClusterThickness);
		vegRoot->SetAttribute("coralBlendStrength", m_coralBlendConstants.m_depthBlendStrength);
		vegRoot->SetAttribute("coralBlendDepthThreshold", m_coralBlendConstants.m_depthBlendThreshold);
		vegRoot->SetAttribute("coralBlendRadius", m_coralBlendConstants.m_blendRadius);
		vegRoot->SetAttribute("coralBlendShader", m_coralBlendPostProcessShaderFilePath.c_str());

		for (int i = 0; i < (int)VegetationType::COUNT; ++i)
		{

			VegetationInfo const& vegInfo = m_vegetationInfos[i];

			if(vegInfo.m_type == VegetationType::UN_INITIALIZED)
				continue;


			VegetationConstants const& vegConst = vegInfo.m_vegConstants;

			XmlElement* vegElement = doc.NewElement("VegetationInfo");
			vegElement->SetAttribute("type", GetVegetationTypeNameFromType((VegetationType) i).c_str());
			vegElement->SetAttribute("isActive", vegInfo.m_isActive);
			vegElement->SetAttribute("worldDotUpThreshold", vegInfo.m_worldDotUpThreshold);
			vegElement->SetAttribute("thickness", vegInfo.m_thickness);

			if (vegInfo.m_diffuseTexName != UN_INITIALIZED_WORLD_DATA_NAME)
			{
				vegElement->SetAttribute("texture", vegInfo.m_diffuseTexName.c_str());
			}

			if (vegInfo.m_normalTexName != UN_INITIALIZED_WORLD_DATA_NAME)
			{
				vegElement->SetAttribute("texture", vegInfo.m_normalTexName.c_str());
			}

			if (vegInfo.m_graphicsShaderName != UN_INITIALIZED_WORLD_DATA_NAME)
			{
				vegElement->SetAttribute("graphicsShader", vegInfo.m_graphicsShaderName.c_str());
			}
			
			vegElement->SetAttribute("computeShader", vegInfo.m_computeShaderName.c_str());

			if (vegInfo.m_meshInfo != UN_INITIALIZED_WORLD_DATA_NAME)
			{
				vegElement->SetAttribute("meshInfo", vegInfo.m_meshInfo.c_str());
			}

			if (IsVegetationTypeCoral((VegetationType)i))
			{
				vegElement->SetAttribute("scale", vegConst.m_heightRange.GetAsText().c_str());
			}

			else
			{
				vegElement->SetAttribute("height", vegConst.m_heightRange.GetAsText().c_str());
				vegElement->SetAttribute("width", vegConst.m_widthRange.GetAsText().c_str());
			}

			vegElement->SetAttribute("rigidity", vegConst.m_rigidity);
			vegElement->SetAttribute("emissiveStrength", vegConst.m_emissiveStrength);
			vegRoot->InsertEndChild(vegElement);
		}

		world->InsertEndChild(vegRoot);
	}


	//Final save file
	std::string fileName = Stringf("Data/Definitions/WorldDefinitions/SavedWorlds/%s.xml", m_worldName.c_str());
	tinyxml2::XMLError eResult = doc.SaveFile(fileName.c_str());
	if (eResult != tinyxml2::XML_SUCCESS)
	{
		DebugAddMessage(Stringf("Could not save XML file: %s.xml", m_worldName.c_str()), 2.f, Rgba8::RED, Rgba8::RED);
	}

}




//Texture Helpers
//----------------------------------------------------------------------------------------------------------------------------------------
void WorldDefinition::LoadBiomeInfoTextureFromImage(std::string const& worldName, Biome biome, bool isFloorTexture, int textureMapIndex, Image* image)
{
	if (textureMapIndex >= NUM_TERRAIN_TEXTURE_MAPS || textureMapIndex < 0)
	{
		ERROR_AND_DIE(Stringf("Trying to load world def texture index: %i\nwhich is outside num texture maps range", textureMapIndex));
		return;
	}

	WorldDefinition* worldDef = nullptr;
	for (int i = 0; i < (int)s_worldDefinitions.size(); ++i)
	{
		if (worldName == s_worldDefinitions[i].m_worldName)
		{
			worldDef = &s_worldDefinitions[i];
			break;
		}
	}

	if(worldDef == nullptr)
		return;

	TerrainBiomeTextureConstants& textureConstants = worldDef->m_textureConstants;
	TextureDX12* texture = g_renderer->CreateOrGetTextureFromImage(*image, NUM_TERRAIN_MIPS);
	unsigned int bindlessTextureIndex = texture ? texture->GetBindlessIndex() : 0;
	
	if (isFloorTexture)
	{
		int textureConstantIndex = (int)biome;
		switch (textureMapIndex)
		{
		case(0):
			textureConstants.m_albedoTextureIndexes[textureConstantIndex] = bindlessTextureIndex;
			break;
		case(1):
			textureConstants.m_normalTextureIndexes[textureConstantIndex] = bindlessTextureIndex;
			break;
		case(2):
			textureConstants.m_aoTextureIndexes[textureConstantIndex] = bindlessTextureIndex;
			break;
		default:
			break;
		}
	}

	else
	{
		int textureConstantIndex = (int)biome + (int)Biome::NUM_BIOMES;
		switch (textureMapIndex)
		{
		case(0):
			textureConstants.m_albedoTextureIndexes[textureConstantIndex] = bindlessTextureIndex;
			break;
		case(1):
			textureConstants.m_normalTextureIndexes[textureConstantIndex] = bindlessTextureIndex;
			break;
		case(2):
			textureConstants.m_aoTextureIndexes[textureConstantIndex] = bindlessTextureIndex;
			break;
		default:
			break;
		}
	}

	
	delete image;
	image = nullptr;
}

void WorldDefinition::LoadBiomeInfoTextureFromBindlessTextureIndex(std::string const& worldName, Biome biome, bool isFloorTexture, int textureMapIndex, unsigned int bindlessTextureIndex)
{
	if (textureMapIndex >= NUM_TERRAIN_TEXTURE_MAPS || textureMapIndex < 0)
	{
		ERROR_AND_DIE(Stringf("Trying to load world def texture index: %i\nwhich is outside num texture maps range", textureMapIndex));
		return;
	}

	WorldDefinition* worldDef = nullptr;
	for (int i = 0; i < (int)s_worldDefinitions.size(); ++i)
	{
		if (worldName == s_worldDefinitions[i].m_worldName)
		{
			worldDef = &s_worldDefinitions[i];
			break;
		}
	}

	if (worldDef == nullptr)
		return;

	TerrainBiomeTextureConstants& textureConstants = worldDef->m_textureConstants;

	if (isFloorTexture)
	{
		int textureConstantIndex = (int)biome;
		switch (textureMapIndex)
		{
		case(0):
			textureConstants.m_albedoTextureIndexes[textureConstantIndex] = bindlessTextureIndex;
			break;
		case(1):
			textureConstants.m_normalTextureIndexes[textureConstantIndex] = bindlessTextureIndex;
			break;
		case(2):
			textureConstants.m_aoTextureIndexes[textureConstantIndex] = bindlessTextureIndex;
			break;
		default:
			break;
		}
	}

	else
	{
		int textureConstantIndex = (int)biome + (int)Biome::NUM_BIOMES;
		switch (textureMapIndex)
		{
		case(0):
			textureConstants.m_albedoTextureIndexes[textureConstantIndex] = bindlessTextureIndex;
			break;
		case(1):
			textureConstants.m_normalTextureIndexes[textureConstantIndex] = bindlessTextureIndex;
			break;
		case(2):
			textureConstants.m_aoTextureIndexes[textureConstantIndex] = bindlessTextureIndex;
			break;
		default:
			break;
		}
	}
}

bool WorldDefinition::TryToGetFishInfoImagesAndShaderFile(std::string const& meshFolderPath, MeshImages& out_images, std::string& out_shaderFilePath)
{
	out_shaderFilePath = UN_INITIALIZED_WORLD_DATA_NAME;
	//Organize File Path
	Strings filePathStrings = SplitStringOnDelimiter(meshFolderPath, '/');
	if (filePathStrings.size() <= 0)
		return false;

	std::string folder = Stringf("/%s", filePathStrings[filePathStrings.size() - 1].c_str());
	std::string xmlPath = Stringf("%s%s.xml", meshFolderPath.c_str(), folder.c_str());

	//Get XML Element
	XmlDocument objXML;
	XmlResult result = objXML.LoadFile(xmlPath.c_str());
	if (result != tinyxml2::XML_SUCCESS)
		return false;

	XmlElement* rootElement = objXML.RootElement();
	if (!rootElement)
		return false; //could not get xml root element

	std::string texturePaths = ParseXmlAttribute(*rootElement, "textureMaps", UN_INITIALIZED_WORLD_DATA_NAME);
	if(texturePaths == UN_INITIALIZED_WORLD_DATA_NAME)
		return false;

	Strings textureMaps = SplitStringOnDelimiter(texturePaths, ',');
	for (int i = 0; i < (int)textureMaps.size(); ++i)
	{
		std::string textureMapFilePath = ParseXmlAttribute(*rootElement, textureMaps[i].c_str(), "INVALID_PATH");

		if (textureMaps[i] == "diffuseMap")
		{
			out_images.m_diffuseImage = new Image(textureMapFilePath.c_str());
		}

		else if (textureMaps[i] == "normalMap")
		{
			out_images.m_normalImage = new Image(textureMapFilePath.c_str());
		}

		else if (textureMaps[i] == "specularMap")
		{
			out_images.m_specularImage = new Image(textureMapFilePath.c_str());
		}

		else if (textureMaps[i] == "aoMap")
		{
			out_images.m_aoImage = new Image(textureMapFilePath.c_str());
		}

		else
		{
			DebugAddMessage(Stringf("Could not find map type: %s in %s", textureMaps[i].c_str(), meshFolderPath.c_str()), 20.f, Rgba8::RED, Rgba8::RED);
		}
		
	}

	out_shaderFilePath = ParseXmlAttribute(*rootElement, "shader", UN_INITIALIZED_WORLD_DATA_NAME);

	return true;
}

void WorldDefinition::LoadFishMeshInfo(std::string const& worldName, FishType fishType, MeshImages fishImages, VertTBNs const& verts, IndexList const& indexes, std::string const& shaderFilePath)
{
	WorldDefinition* worldDef = nullptr;
	for (int i = 0; i < (int)s_worldDefinitions.size(); ++i)
	{
		if (worldName == s_worldDefinitions[i].m_worldName)
		{
			worldDef = &s_worldDefinitions[i];
			break;
		}
	}

	if (worldDef == nullptr)
		return;

	FishInfo& fishInfo = worldDef->m_fishInfos[(int)fishType];

	fishInfo.m_textures.LoadTexturesFromMeshImages(fishImages);

	std::string fishName = GetFishTypeNameFromType(fishType);

	if (verts.size() <= 0)
	{
		VertTBNs defaultVerts;
		IndexList defaultIndexes;
		AddVertsForIndexedCone3D(defaultVerts, defaultIndexes, Vec3(-1.f, 0.f, 0.f), Vec3(1.f, 0.f, 0.f), 1.f , Rgba8::ORANGE);
		fishInfo.m_vbo = g_renderer->CreateVertexBuffer(defaultVerts, "Default_" + fishName + "_VBO");
		fishInfo.m_ibo = g_renderer->CreateIndexBuffer(defaultIndexes, "Default_" + fishName + "_IBO");
		fishInfo.m_shader = nullptr;
	}

	else
	{
		fishInfo.m_vbo = g_renderer->CreateVertexBuffer(verts, "Default_" + fishName + "_VBO");
		fishInfo.m_ibo = g_renderer->CreateIndexBuffer(indexes, "Default_" + fishName + "_IBO");


		float frontPosX = -99999.f;
		float backPosX = 99999.f;
		for (int i = 0; i < (int)verts.size(); ++i)
		{
			Vec3 const& pos = verts[i].m_position;
			if (pos.x > frontPosX)
			{
				frontPosX = pos.x;
			}

			if (pos.x < backPosX)
			{
				backPosX = pos.x;
			}
		}

		float length = frontPosX - backPosX;
		fishInfo.m_swimConstants.m_modelLength = length;
	}

	if (shaderFilePath != UN_INITIALIZED_WORLD_DATA_NAME)
	{
		fishInfo.m_shader = g_renderer->CreateOrGetShaderFromFile(shaderFilePath.c_str(), VertexType::VERTEX_PCUTBN);
	}

	switch (fishInfo.m_type)
	{
	case FishType::ANGEL_FISH: fishInfo.m_swimConstants.m_tailStart = -0.5f; 
		break;
	case FishType::CHROMIS: fishInfo.m_swimConstants.m_tailStart = -0.5f;
		break;
	case FishType::SHARK: fishInfo.m_swimConstants.m_tailStart = 0.5f;
		break;
	default: 
		fishInfo.m_swimConstants.m_tailStart = -0.5f;
		break;
	}

	worldDef->m_shouldLoadFishModels = false;

}

bool WorldDefinition::TryToGetVegInfoImagesAndShaderFile(std::string const& meshFolderPath, MeshImages& out_images, std::string& out_shaderFilePath)
{
	//Organize File Path
	Strings filePathStrings = SplitStringOnDelimiter(meshFolderPath, '/');
	if (filePathStrings.size() <= 0)
		return false;

	std::string folder = Stringf("/%s", filePathStrings[filePathStrings.size() - 1].c_str());
	std::string xmlPath = Stringf("%s%s.xml", meshFolderPath.c_str(), folder.c_str());

	//Get XML Element
	XmlDocument objXML;
	XmlResult result = objXML.LoadFile(xmlPath.c_str());
	if (result != tinyxml2::XML_SUCCESS)
		return false;

	XmlElement* rootElement = objXML.RootElement();
	if (!rootElement)
		return false; //could not get xml root element

	std::string texturePaths = ParseXmlAttribute(*rootElement, "textureMaps", UN_INITIALIZED_WORLD_DATA_NAME);
	if (texturePaths == UN_INITIALIZED_WORLD_DATA_NAME)
		return false;

	Strings textureMaps = SplitStringOnDelimiter(texturePaths, ',');
	for (int i = 0; i < (int)textureMaps.size(); ++i)
	{
		std::string textureMapFilePath = ParseXmlAttribute(*rootElement, textureMaps[i].c_str(), "INVALID_PATH");

		if (textureMaps[i] == "diffuseMap")
		{
			out_images.m_diffuseImage = new Image(textureMapFilePath.c_str());
		}

		else if (textureMaps[i] == "normalMap")
		{
			out_images.m_normalImage = new Image(textureMapFilePath.c_str());
		}

		else if (textureMaps[i] == "specularMap")
		{
			out_images.m_specularImage = new Image(textureMapFilePath.c_str());
		}

		else if (textureMaps[i] == "emissiveMap")
		{
			out_images.m_emissiveImage = new Image(textureMapFilePath.c_str());
		}

		else
		{
			DebugAddMessage(Stringf("Could not find map type: %s in %s", textureMaps[i].c_str(), meshFolderPath.c_str()), 20.f, Rgba8::RED, Rgba8::RED);
		}
	}

	out_shaderFilePath = ParseXmlAttribute(*rootElement, "shader", UN_INITIALIZED_WORLD_DATA_NAME);
	return true;
}

void WorldDefinition::LoadVegetationMeshInfo(std::string const& worldName, VegetationType vegType, MeshImages vegImages, VertTBNs const& verts, IndexList const& indexes, std::string const& shaderFilePath, float meshRadius)
{
	WorldDefinition* worldDef = nullptr;
	for (int i = 0; i < (int)s_worldDefinitions.size(); ++i)
	{
		if (worldName == s_worldDefinitions[i].m_worldName)
		{
			worldDef = &s_worldDefinitions[i];
			break;
		}
	}

	if (worldDef == nullptr)
		return;


	VegetationInfo& vegInfo = worldDef->m_vegetationInfos[(int)vegType];
	VegetationConstants const& vegConstants = vegInfo.m_vegConstants;

	//Determine the spawn radius (how tightly the vegetation can be instanced)
	//-------------------------------------------------------------------------------------------------------------------
	float maxVariationWidth = GetMax(vegConstants.m_widthRange.m_min, vegConstants.m_widthRange.m_max);
	vegInfo.m_meshRadius = meshRadius;
	float spawnThicknessMeters = Lerp(MIN_VEGETATION_THICKNESS_METERS, MAX_VEGETATION_THICKNESS_METERS, vegInfo.m_thickness);
	float meshScaledRadius = vegInfo.m_meshRadius;
	if (IsVegetationTypeCoral(vegType))
	{
		meshScaledRadius *= Lerp(1.f, 0.1f, worldDef->m_coralClusterThickness);
	}

	vegInfo.m_instanceRadius = spawnThicknessMeters + (meshScaledRadius * maxVariationWidth);


	int mipLevels = DoesVegetationTypeHaveMaskedTexture(vegType) ? 2 : NUM_VEGETATION_MIPS;

	vegInfo.m_textures.LoadTexturesFromMeshImages(vegImages, mipLevels);
	std::string vegName = GetVegetationTypeNameFromType(vegType);
	vegInfo.m_instanceVbo = g_renderer->CreateVertexBuffer(verts, vegName + "_VBO");
	vegInfo.m_instanceIBO = g_renderer->CreateIndexBuffer(indexes, vegName + "_IBO");


	if (shaderFilePath != UN_INITIALIZED_WORLD_DATA_NAME)
	{
		vegInfo.m_graphicsShader = g_renderer->CreateOrGetShaderFromFile(shaderFilePath.c_str(), VertexType::VERTEX_PCUTBN);
	}

	worldDef->m_shouldLoadVegetationModels = false;
}



//Curve Helpers
//----------------------------------------------------------------------------------------------------------------------------------------
CurveWithTStart<float> WorldDefinition::GetCurveAndTPlotFromXMLElement(XmlElement const& plotPointElement, Vec2& out_startPlotPoint) const
{
	std::string curveType = ParseXmlAttribute(plotPointElement, "type", "linear");
	Vec2 curveStart = out_startPlotPoint;
	Vec2 curveEnd = ParseXmlAttribute(plotPointElement, "point", Vec2(1.f, 0.f));
	int exponent = ParseXmlAttribute(plotPointElement, "exponent", 1);
	out_startPlotPoint = curveEnd;

	if (curveType == "linear")
		return CurveWithTStart(new LinearCurve<float>(curveStart.y, curveEnd.y), curveStart.x);
	if (curveType == "smoothStep3")
		return CurveWithTStart(new SmoothStep3Curve<float>(curveStart.y, curveEnd.y), curveStart.x);
	if (curveType == "smoothStep5")
		return CurveWithTStart(new SmoothStep5Curve<float>(curveStart.y, curveEnd.y), curveStart.x);
	if(curveType == "smoothStart")
		return CurveWithTStart(new SmoothStartCurve<float>(curveStart.y, curveEnd.y, exponent), curveStart.x);
	if(curveType == "smoothStop")
		return CurveWithTStart(new SmoothStopCurve<float>(curveStart.y, curveEnd.y, exponent), curveStart.x);
	if(curveType == "hesitate3")
		return CurveWithTStart(new Hesitate3Curve<float>(curveStart.y, curveEnd.y), curveStart.x);
	if (curveType == "hesitate5")
		return CurveWithTStart(new Hesitate5Curve<float>(curveStart.y, curveEnd.y), curveStart.x);
	if (curveType == "easeInBack")
		return CurveWithTStart(new EaseInBackCurve<float>(curveStart.y, curveEnd.y), curveStart.x);
	if (curveType == "easeOutBack")
		return CurveWithTStart(new EaseOutBackCurve<float>(curveStart.y, curveEnd.y), curveStart.x);

	ERROR_AND_DIE("Could not create curve");
}

float WorldDefinition::GetCurveValue(TerrainNoiseType noiseType, TerrainCurveType curveType, float t) const
{
	TerrainNoiseInfo noiseInfo = m_terrainNoiseInfos[(int)noiseType];
	PieceWiseCurve<float>* curve = nullptr;
	switch (curveType)
	{
	case TerrainCurveType::HEIGHT_OFFSET:
		curve = noiseInfo.m_terrainHeightOffsetCurve;
		break;
	case TerrainCurveType::SQUASH:
		curve = noiseInfo.m_squashCurve;
		break;
	}

	if(!curve)
		return t;

	return curve->EvaluateAt(t);
}





//Noise
//----------------------------------------------------------------------------------------------------------------------------------------

//Terrain
float WorldDefinition::GetCumulativeDensityAtPosition(Vec3 const& pos) const
{
	const float BASE_TERRAIN_HEIGHT = (GetSeaLevel() - m_baseTerrainDepthBelowSeaLevel);
	const float MIN_TERRAIN_HEIGHT = (GetSeaLevel() - m_waterConstants.m_maxTerrainDepth);

	float normalizedZ = GetClampedFractionWithinRange(pos.z, MIN_TERRAIN_HEIGHT, m_worldHeight);
	const float WORLD_HALF_HEIGHT = (m_worldHeight * 0.5f);
	float squashCurve = (4.f * normalizedZ * (1.f - normalizedZ));
	const float BASE_TERRAIN_BIAS = (1.f - m_baseTerrainBiasFactor);
	const float FINAL_TERRAIN_BIAS = (1.f - m_finalTerrainBiasFactor);
	float verticalBias = Lerp(1.f, -1.f, SmoothStep3(normalizedZ));

	// Base Terrain 3D
	float baseNoise = GetBaseTerrainNoiseAtPos(pos);
	float density = (verticalBias * (1.f - BASE_TERRAIN_BIAS)) + (baseNoise * squashCurve * BASE_TERRAIN_BIAS);

	// 2D shaping
	TerrainNoiseValues2D noiseValues = GetTerrainNoiseValuesAtPos2D(pos.GetXY());

	float heightOffset = GetTerrainHeightOffsetAtPos2D(noiseValues);
	float squashFactor = GetSquashFactorAtPos2D(noiseValues);

	float baseTerrainHeightXY = (BASE_TERRAIN_HEIGHT + (heightOffset * WORLD_HALF_HEIGHT));
	float terrainOffsetFromBaseHeight = ((pos.z - baseTerrainHeightXY) / GetMax(baseTerrainHeightXY, 0.1f));

	density += heightOffset;
	density -= (squashFactor * terrainOffsetFromBaseHeight);

	density = (verticalBias * (1.f - FINAL_TERRAIN_BIAS)) + (density * squashCurve * FINAL_TERRAIN_BIAS);

	/*
	//Cheese Caves
	//----------------------------------------------------------------------------------------------------------------------
	noiseInfo = m_definition->m_terrainNoiseInfos[(int)TerrainNoiseType::CHEESE_CAVES];
	if (noiseInfo.m_isActive)
	{
		Vec2 cheeseWarp = warpedCoords * noiseInfo.m_warpStrength;
		float warpedX = pos.x + cheeseWarp.x;
		float warpedY = pos.y + cheeseWarp.y;
		float cheeseCaves = Compute3dFractalNoise(warpedX, warpedY, pos.z, noiseInfo.m_scale, noiseInfo.m_numOctaves, m_definition->m_defaultOctavePersistance, m_definition->m_defaultOctaveScale, true, noiseInfo.m_seed);
		cheeseCaves -= 0.1f; //bias which helps remove floating blobs
		float cheesiness = m_definition->GetCurveValue(TerrainNoiseType::CHEESE_CAVES, cheeseCaves) * noiseInfo.m_strength;

		constexpr float CAVE_FADE_RANGE = 30.f;
		constexpr float CAVE_SURFACE_OVERLAP = -2.5f;

		float caveMask = 0.f;
		float startDepth = m_definition->GetSeaLevel() - noiseInfo.m_startDepthBelowSeaLevel;
		float depthBelowCaveStart = GetMin(baseTerrainHeight, startDepth) - pos.z;
		caveMask = SmoothStep3(GetClamped((depthBelowCaveStart - CAVE_SURFACE_OVERLAP) / CAVE_FADE_RANGE, 0.f, 1.f));
		density -= cheesiness * caveMask;

	}
	*/


	/*
	//Rockiness
	//----------------------------------------------------------------------------------------------------------------------
	noiseInfo = m_definition->m_terrainNoiseInfos[(int)TerrainNoiseType::ROCKINESS];
	if (noiseInfo.m_isActive)
	{
		Vec2 rockinessWarp = warpedCoords * noiseInfo.m_warpStrength;
		float warpedX = pos.x + rockinessWarp.x;
		float warpedY = pos.y + rockinessWarp.y;
		float rockyNoise = Compute3dFractalNoise(warpedX, warpedY, pos.z, noiseInfo.m_scale, noiseInfo.m_numOctaves, m_definition->m_defaultOctavePersistance, m_definition->m_defaultOctaveScale, true, noiseInfo.m_seed);
		float rockyCarve = rockyNoise * noiseInfo.m_strength;

		if (rockyCarve > 0.f)
		{
			constexpr float ROCKINESS_HEIGHT_STRENGTH = 20.f;
			float rockinessHeight = ROCKINESS_HEIGHT_STRENGTH * fabs(Compute2dFractalNoise(pos.x, pos.y, noiseInfo.m_scale, 1, m_definition->m_defaultOctavePersistance, m_definition->m_defaultOctaveScale, true, noiseInfo.m_seed + 1));
			float rockyBand = 20.f;
			float rockyMask = SmoothStep3(GetClamped((baseTerrainHeight + rockinessHeight - pos.z) / rockyBand, 0.f, 1.f));
			density += rockyCarve * rockyMask; //makes density more positive ("more below ground")
		}
	}

	float fadeRange = 16.f;
	if (pos.z < m_definition->m_worldZRange.m_min + fadeRange)
	{
		float belowMin = pos.z - m_definition->m_worldZRange.m_min;
		float fadeFactor = GetClampedFractionWithinRange(belowMin, 0.f, fadeRange);
		float t = SmoothStep3(fadeFactor);
		float minDensity = 0.1f;
		density = GetMax(density, minDensity * t);
	}

	*/


	return density;
}

float WorldDefinition::GetBaseTerrainNoiseAtPos(Vec3 const& pos) const
{
	float noise = 0.f;
	TerrainNoiseInfo noiseInfo = m_terrainNoiseInfos[(int)TerrainNoiseType::BASE_TERRAIN];
	if (noiseInfo.m_isActive)
	{
		float posX = pos.x;
		float posY = pos.y;

		if (noiseInfo.m_warpStrength != 0)
		{
			Vec2 warpedCoords
			(
				Compute2dFractalNoise(pos.x, pos.y, m_warpNoiseScaleXY.x, m_warpNoiseNumOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, m_gameSeed + 1),
				Compute2dFractalNoise(pos.x, pos.y, m_warpNoiseScaleXY.y, m_warpNoiseNumOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, m_gameSeed + 2)
			);

			Vec2 baseWarp = warpedCoords * noiseInfo.m_warpStrength;
			posX = pos.x + baseWarp.x;
			posY = pos.y + baseWarp.y;
		}

		switch (noiseInfo.m_noiseType)
		{
		case NoiseType::FRACTAL:
			noise = Compute3dFractalNoise(posX, posY, pos.z, noiseInfo.m_scale, noiseInfo.m_numOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, noiseInfo.m_seed);
			break;
		case NoiseType::PERLIN:
			noise = Compute3dPerlinNoise(posX, posY, pos.z, noiseInfo.m_scale, noiseInfo.m_numOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, noiseInfo.m_seed);
			break;
		default:
			noise = Compute3dFractalNoise(posX, posY, pos.z, noiseInfo.m_scale, noiseInfo.m_numOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, noiseInfo.m_seed);
			break;
		}
		
		//noise = Lerp(noise, 1.0f - fabsf(noise), 0.35f);
	}

	return noise;
}

BiomeSplat WorldDefinition::GetBiomeSplatFromNoiseValues(TerrainNoiseValues2D const& terrainNoiseValues, VegetationNoiseValues2D const& vegetationNoiseValues, Vec3 const& pos, Vec3 const& terrainNormal) const
{
	float biomeScores[(int)Biome::NUM_BIOMES] = {};
	GetBiomeScoresFromNoiseValues(biomeScores, terrainNoiseValues, vegetationNoiseValues, pos, terrainNormal);
	return BuildBiomeSplatFromScores(biomeScores);
}

void WorldDefinition::GetBiomeScoresFromNoiseValues(float outBiomeScores[(int)Biome::NUM_BIOMES], TerrainNoiseValues2D const& terrainNoiseValues, VegetationNoiseValues2D const& vegetationNoiseValues, Vec3 const& pos, Vec3 const& terrainNormal) const
{
	for (int biomeIndex = 0; biomeIndex < (int)Biome::NUM_BIOMES; ++biomeIndex)
	{
		outBiomeScores[biomeIndex] = 0.f;
	}

	float const temperature = terrainNoiseValues.m_values[(int)TerrainNoiseType::TEMPERATURE];
	float const peaksAndValleys = terrainNoiseValues.m_values[(int)TerrainNoiseType::PEAKS_AND_VALLEYS];
	float const erosion = terrainNoiseValues.m_values[(int)TerrainNoiseType::EROSION];
	float const kelpNoise = vegetationNoiseValues.m_values[(int)VegetationNoiseType::KELP];

	float const seaLevel = GetSeaLevel();
	float const waterDepth = (seaLevel - pos.z);
	float const heightAboveSeaLevel = (pos.z - seaLevel);
	float const upness = terrainNormal.z;

	float const depthBlendWidth = Lerp(0.f, 20.f, m_biomeBlend);
	float const lateralBlendWidth = m_biomeBlend;

	float const shallowDepthEnd = 200.f;
	float const midDepthEnd = 400.f;

	float const grassyBeachStartHeight = 50.f;

	float const beachGrassThreshold = 0.01f;
	float const shallowGrassThreshold = 0.42f;
	float const shallowRockThreshold = 0.62f;
	float const midMossThreshold = 0.52f;
	float const deepSmoothThreshold = 0.58f;

	float const coralMinDepth = 8.f;
	float const coralMaxDepth = shallowDepthEnd;
	float const coralWarmthThreshold = 0.1f;
	float const coralRockSuppressThreshold = 0.95f;

	float const kelpMinDepth = 100.f;
	float const kelpMaxDepth = midDepthEnd;
	float const kelpCoolnessThreshold = 0.55f;
	float const kelpRockSuppressThreshold = 0.72f;

	float const flatGroundMinUpness = 0.70f;
	float const slopeRockReduction = 0.35f;

	float const warmth = temperature;
	float const coolness = (1.f - temperature);
	float const roughness = (1.f - erosion);
	float const smoothness = erosion;
	float const flatness = RangeMapClamped(upness, flatGroundMinUpness, 1.f, 0.f, 1.f);

	float groundType = ((0.55f * peaksAndValleys) + (0.45f * roughness));
	groundType = Lerp(groundType, 0.f, (flatness * slopeRockReduction));
	groundType = GetClampedZeroToOne(groundType);

	float fertility = ((0.65f * warmth) + (0.35f * smoothness));
	fertility = GetClampedZeroToOne(fertility);

	float grassClusterMask = GetVegetationClusterMask(vegetationNoiseValues.m_values[(int)VegetationNoiseType::GRASS], VegetationNoiseType::GRASS);
	float seaGrassClusterMask = GetVegetationClusterMask(vegetationNoiseValues.m_values[(int)VegetationNoiseType::SEA_GRASS], VegetationNoiseType::SEA_GRASS);
	float coralClusterMask = GetVegetationClusterMask(vegetationNoiseValues.m_values[(int)VegetationNoiseType::CORAL], VegetationNoiseType::CORAL);
	float vegetationPotential = GetVegetationPotential(temperature);

	float beachGrassMask = (grassClusterMask * vegetationPotential);
	float seaGrassMask = (seaGrassClusterMask * vegetationPotential);
	float coralGardenMask = (coralClusterMask * vegetationPotential);

	float const aboveWaterBand = (1.f - SmoothStep3Range(waterDepth, -depthBlendWidth, depthBlendWidth));
	float const shallowBand = SmoothPulse3(waterDepth, -depthBlendWidth, depthBlendWidth, (shallowDepthEnd - depthBlendWidth), (shallowDepthEnd + depthBlendWidth));
	float const midBand = SmoothPulse3(waterDepth, (shallowDepthEnd - depthBlendWidth), (shallowDepthEnd + depthBlendWidth), (midDepthEnd - depthBlendWidth), (midDepthEnd + depthBlendWidth));
	float const deepBand = SmoothStep3Range(waterDepth, (midDepthEnd - depthBlendWidth), (midDepthEnd + depthBlendWidth));

	float const beachGrassBaseMembership = SmoothStep3Range(fertility, (beachGrassThreshold - lateralBlendWidth), (beachGrassThreshold + lateralBlendWidth));
	float const grassyBeachHeightMembership = SmoothStep3Range(heightAboveSeaLevel, (grassyBeachStartHeight - depthBlendWidth), (grassyBeachStartHeight + depthBlendWidth));
	float grassyBeachMembership = (beachGrassBaseMembership * grassyBeachHeightMembership * beachGrassMask);
	grassyBeachMembership = GetClampedZeroToOne(grassyBeachMembership);

	float const shallowGrassBaseMembership = SmoothStep3Range(fertility, (shallowGrassThreshold - lateralBlendWidth), (shallowGrassThreshold + lateralBlendWidth));
	float const shallowRockMembership = SmoothStep3Range(groundType, (shallowRockThreshold - lateralBlendWidth), (shallowRockThreshold + lateralBlendWidth));

	float const midMossMembership = SmoothStep3Range(groundType, (midMossThreshold - lateralBlendWidth), (midMossThreshold + lateralBlendWidth));
	float const deepSmoothMembership = SmoothStep3Range(smoothness, (deepSmoothThreshold - lateralBlendWidth), (deepSmoothThreshold + lateralBlendWidth));

	float coralDepthMembershipA = SmoothStep3Range(waterDepth, (coralMinDepth - depthBlendWidth), (coralMinDepth + depthBlendWidth));
	float coralDepthMembershipB = (1.f - SmoothStep3Range(waterDepth, (coralMaxDepth - depthBlendWidth), (coralMaxDepth + depthBlendWidth)));
	float coralWarmthMembership = SmoothStep3Range(warmth, (coralWarmthThreshold - lateralBlendWidth), (coralWarmthThreshold + lateralBlendWidth));
	float coralRockSuppress = SmoothStep3Range(groundType, (coralRockSuppressThreshold - lateralBlendWidth), (coralRockSuppressThreshold + lateralBlendWidth));
	float coralMembership = (coralDepthMembershipA * coralDepthMembershipB * coralWarmthMembership * coralGardenMask * (1.f - coralRockSuppress));
	coralMembership = GetClampedZeroToOne(coralMembership);

	float kelpDepthMembershipA = SmoothStep3Range(waterDepth, (kelpMinDepth - depthBlendWidth), (kelpMinDepth + depthBlendWidth));
	float kelpDepthMembershipB = (1.f - SmoothStep3Range(waterDepth, (kelpMaxDepth - depthBlendWidth), (kelpMaxDepth + depthBlendWidth)));
	float kelpCoolnessMembership = SmoothStep3Range(coolness, (kelpCoolnessThreshold - lateralBlendWidth), (kelpCoolnessThreshold + lateralBlendWidth));
	float kelpRockSuppress = SmoothStep3Range(groundType, (kelpRockSuppressThreshold - lateralBlendWidth), (kelpRockSuppressThreshold + lateralBlendWidth));
	float kelpMembership = (kelpDepthMembershipA * kelpDepthMembershipB * kelpCoolnessMembership * kelpNoise * (1.f - kelpRockSuppress));
	kelpMembership = GetClampedZeroToOne(kelpMembership);

	float shallowGrassSandMembership = (shallowGrassBaseMembership * seaGrassMask * (1.f - shallowRockMembership));
	shallowGrassSandMembership = GetClampedZeroToOne(shallowGrassSandMembership);

	float shallowRockySandMembership = shallowRockMembership;
	float shallowSandMembership = ((1.f - shallowRockMembership) - shallowGrassSandMembership);
	shallowSandMembership = GetClampedZeroToOne(shallowSandMembership);

	float midRockySandMembership = (1.f - midMossMembership);
	float midMossyRockMembership = midMossMembership;

	float deepMossyRockMembership = (1.f - deepSmoothMembership);
	float deepSmoothRockMembership = deepSmoothMembership;

	outBiomeScores[(int)Biome::BEACH] += (aboveWaterBand * (1.f - grassyBeachMembership));
	outBiomeScores[(int)Biome::GRASSY_BEACH] += (aboveWaterBand * grassyBeachMembership);

	outBiomeScores[(int)Biome::SANDY_PLAINS] += (shallowBand * shallowSandMembership * (1.f - coralMembership));
	outBiomeScores[(int)Biome::GRASSY_SAND] += (shallowBand * shallowGrassSandMembership * (1.f - coralMembership));
	outBiomeScores[(int)Biome::ROCKY_SAND] += (shallowBand * shallowRockySandMembership * (1.f - coralMembership));
	outBiomeScores[(int)Biome::CORAL_GARDEN] += (shallowBand * coralMembership);

	outBiomeScores[(int)Biome::ROCKY_SAND] += (midBand * midRockySandMembership * (1.f - kelpMembership));
	outBiomeScores[(int)Biome::MOSSY_ROCK] += (midBand * midMossyRockMembership * (1.f - kelpMembership));
	outBiomeScores[(int)Biome::KELP_FOREST] += (midBand * kelpMembership);

	outBiomeScores[(int)Biome::MOSSY_ROCK] += (deepBand * deepMossyRockMembership);
	outBiomeScores[(int)Biome::SMOOTH_ROCK] += (deepBand * deepSmoothRockMembership);
}

TerrainNoiseValues2D WorldDefinition::GetTerrainNoiseValuesAtPos2D(Vec2 const& pos) const
{
	TerrainNoiseValues2D noiseValues;
	Vec2 warpedCoords
	(
		Compute2dFractalNoise(pos.x, pos.y, m_warpNoiseScaleXY.x, m_warpNoiseNumOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, m_gameSeed + 1),
		Compute2dFractalNoise(pos.x, pos.y, m_warpNoiseScaleXY.y, m_warpNoiseNumOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, m_gameSeed + 2)
	);


	for (int i = 0; i < NUM_2D_TERRAIN_NOISE_VALUES; ++i)
	{
		TerrainNoiseInfo noiseInfo = m_terrainNoiseInfos[i];
		Vec2 warp = warpedCoords * noiseInfo.m_warpStrength;
		float warpedX = pos.x + warp.x;
		float warpedY = pos.y + warp.y;

		float noise = 0.f;
		switch (noiseInfo.m_noiseType)
		{
		case NoiseType::PERLIN:
			noise = Compute2dPerlinNoise(warpedX, warpedY, noiseInfo.m_scale, noiseInfo.m_numOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, noiseInfo.m_seed);
		break;
		case NoiseType::FRACTAL:
			noise = Compute2dFractalNoise(warpedX, warpedY, noiseInfo.m_scale, noiseInfo.m_numOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, noiseInfo.m_seed);
			break;
		case NoiseType::RAW_NEG_ONE_TO_ONE:
			noise = Get2dNoiseNegOneToOne((int)warpedY, (int)warpedY, noiseInfo.m_seed);
			break;
		case NoiseType::RAW_ZERO_TO_ONE:
			noise = Get2dNoiseZeroToOne((int)warpedY, (int)warpedY, noiseInfo.m_seed);
			break;
		default:
			noise = Get2dNoiseNegOneToOne((int)warpedY,(int) warpedY, noiseInfo.m_seed);
			break;
		}

		noiseValues.m_values[i] = noise;
	}

	return noiseValues;
}

float WorldDefinition::GetTerrainHeightOffsetAtPos2D(TerrainNoiseValues2D const& noiseValuesAtPos) const
{
	float heightOffset = 0;
	TerrainNoiseInfo noiseInfo = m_terrainNoiseInfos[(int)TerrainNoiseType::CONTINENTALNESS];
	float continentalness = noiseValuesAtPos.m_values[(int)TerrainNoiseType::CONTINENTALNESS];
	if (noiseInfo.m_isActive)
	{
		continentalness = GetCurveValue(TerrainNoiseType::CONTINENTALNESS, TerrainCurveType::HEIGHT_OFFSET, noiseValuesAtPos.m_values[(int)TerrainNoiseType::CONTINENTALNESS]);
		heightOffset += noiseInfo.m_strength * continentalness;
	}

	noiseInfo = m_terrainNoiseInfos[(int)TerrainNoiseType::EROSION];
	if (noiseInfo.m_isActive)
	{
		float erosion = GetCurveValue(TerrainNoiseType::EROSION, TerrainCurveType::HEIGHT_OFFSET, continentalness);
		heightOffset += noiseInfo.m_strength * erosion * noiseValuesAtPos.m_values[(int)TerrainNoiseType::EROSION];
	}

	noiseInfo = m_terrainNoiseInfos[(int)TerrainNoiseType::PEAKS_AND_VALLEYS];
	if (noiseInfo.m_isActive)
	{
		float pv = GetCurveValue(TerrainNoiseType::PEAKS_AND_VALLEYS, TerrainCurveType::HEIGHT_OFFSET, continentalness);
		heightOffset += noiseInfo.m_strength * pv * noiseValuesAtPos.m_values[(int)TerrainNoiseType::PEAKS_AND_VALLEYS];
	}

	return heightOffset;
}

float WorldDefinition::GetSquashFactorAtPos2D( TerrainNoiseValues2D const& noiseValuesAtPos) const
{
	float squashFactor = 0;
	TerrainNoiseInfo noiseInfo = m_terrainNoiseInfos[(int)TerrainNoiseType::CONTINENTALNESS];
	float continentalness = noiseValuesAtPos.m_values[(int)TerrainNoiseType::CONTINENTALNESS];
	if (noiseInfo.m_isActive)
	{
		continentalness = GetCurveValue(TerrainNoiseType::CONTINENTALNESS, TerrainCurveType::SQUASH, noiseValuesAtPos.m_values[(int)TerrainNoiseType::CONTINENTALNESS]);
		squashFactor += noiseInfo.m_strength * continentalness;
	}

	noiseInfo = m_terrainNoiseInfos[(int)TerrainNoiseType::EROSION];
	if (noiseInfo.m_isActive)
	{
		float erosion = GetCurveValue(TerrainNoiseType::EROSION, TerrainCurveType::SQUASH, continentalness);
		squashFactor += noiseInfo.m_strength * erosion * noiseValuesAtPos.m_values[(int)TerrainNoiseType::EROSION];
	}

	noiseInfo = m_terrainNoiseInfos[(int)TerrainNoiseType::PEAKS_AND_VALLEYS];
	if (noiseInfo.m_isActive)
	{
		float pv = GetCurveValue(TerrainNoiseType::PEAKS_AND_VALLEYS, TerrainCurveType::SQUASH, continentalness);
		squashFactor += noiseInfo.m_strength * pv * noiseValuesAtPos.m_values[(int)TerrainNoiseType::PEAKS_AND_VALLEYS];
	}

	return squashFactor;
}

void WorldDefinition::UpdateVegetationSpawnRadiusInfo()
{
	m_smallestVegetationDotProductThreshold = 1.f;
	for (int i = 0; i < (int)VegetationType::COUNT; ++i)
	{
		VegetationInfo& vegInfo = m_vegetationInfos[i];
		VegetationConstants const& vegConstants = vegInfo.m_vegConstants;
		VegetationType vegType = (VegetationType)i;

		float maxVariationWidth = GetMax(vegConstants.m_widthRange.m_min, vegConstants.m_widthRange.m_max);

		float spawnThicknessMeters = Lerp(MIN_VEGETATION_THICKNESS_METERS, MAX_VEGETATION_THICKNESS_METERS, vegInfo.m_thickness);
		float meshScaledRadius = vegInfo.m_meshRadius;

		if (IsVegetationTypeCoral(vegType))
		{
			meshScaledRadius *= Lerp(1.f, 0.1f, m_coralClusterThickness);
		}

		vegInfo.m_instanceRadius = spawnThicknessMeters + (meshScaledRadius * maxVariationWidth);
		m_smallestVegetationDotProductThreshold = GetMin(m_smallestVegetationDotProductThreshold, vegInfo.m_worldDotUpThreshold);
	}
}

//Vegetation
VegetationNoiseValues2D WorldDefinition::GetVegetationNoiseValues2DAtPos2D(Vec2 const& pos) const
{
	VegetationNoiseValues2D vegNoiseValues;

	Vec2 warpedCoords
	(
		Compute2dFractalNoise(pos.x, pos.y, m_warpNoiseScaleXY.x, m_warpNoiseNumOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, m_gameSeed + 1),
		Compute2dFractalNoise(pos.x, pos.y, m_warpNoiseScaleXY.y, m_warpNoiseNumOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, m_gameSeed + 2)
	);

	for (int i = 0; i < (int)VegetationNoiseType::COUNT; ++i)
	{
		VegetationNoiseInfo noiseInfo = m_vegetationNoiseInfos[i];
		Vec2 warp = warpedCoords * noiseInfo.m_warpStrength;
		float warpedX = pos.x + warp.x;
		float warpedY = pos.y + warp.y;

		float noise = 0.f;
		switch (noiseInfo.m_noiseType)
		{
		case NoiseType::PERLIN:
			noise = Compute2dPerlinNoise(warpedX, warpedY, noiseInfo.m_scale, noiseInfo.m_numOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, noiseInfo.m_seed);
			break;
		case NoiseType::FRACTAL:
			noise = Compute2dFractalNoise(warpedX, warpedY, noiseInfo.m_scale, noiseInfo.m_numOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, noiseInfo.m_seed);
			break;
		case NoiseType::RAW_NEG_ONE_TO_ONE:
			noise = Get2dNoiseNegOneToOne((int)warpedY, (int)warpedY, noiseInfo.m_seed);
			break;
		case NoiseType::RAW_ZERO_TO_ONE:
			noise = Get2dNoiseZeroToOne((int)warpedY, (int)warpedY, noiseInfo.m_seed);
			break;
		default:
			noise = Get2dNoiseNegOneToOne((int)warpedY, (int)warpedY, noiseInfo.m_seed);
			break;
		}

		vegNoiseValues.m_values[i] = Compute2dPerlinNoise(warpedX, warpedY, noiseInfo.m_scale, noiseInfo.m_numOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, noiseInfo.m_seed);
	}

	return vegNoiseValues;
}

float WorldDefinition::GetVegetationNoiseValueAtPos2D(Vec2 const& pos, VegetationNoiseType const& worldNoiseType) const
{
	VegetationNoiseInfo noiseInfo = m_vegetationNoiseInfos[(int)worldNoiseType];
	float warpedX = pos.x;
	float warpedY = pos.y;
	if (noiseInfo.m_warpStrength > 0.f)
	{
		Vec2 warpedCoords
		(
			Compute2dFractalNoise(pos.x, pos.y, m_warpNoiseScaleXY.x, m_warpNoiseNumOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, m_gameSeed + 1),
			Compute2dFractalNoise(pos.x, pos.y, m_warpNoiseScaleXY.y, m_warpNoiseNumOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, m_gameSeed + 2)
		);

		Vec2 warp = warpedCoords * noiseInfo.m_warpStrength;
		warpedX = pos.x + warp.x;
		warpedY = pos.y + warp.y;
	}

	switch (noiseInfo.m_noiseType)
	{
	case NoiseType::PERLIN:
		return Compute2dPerlinNoise(warpedX, warpedY, noiseInfo.m_scale, noiseInfo.m_numOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, noiseInfo.m_seed);
	case NoiseType::FRACTAL:
		return Compute2dFractalNoise(warpedX, warpedY, noiseInfo.m_scale, noiseInfo.m_numOctaves, m_defaultOctavePersistance, m_defaultOctaveScale, true, noiseInfo.m_seed);
	case NoiseType::RAW_NEG_ONE_TO_ONE:
		return Get2dNoiseNegOneToOne((int)warpedY, (int)warpedY, noiseInfo.m_seed);
	case NoiseType::RAW_ZERO_TO_ONE:
		return Get2dNoiseZeroToOne((int)warpedY, (int)warpedY, noiseInfo.m_seed);
	default:
		return Get2dNoiseNegOneToOne((int)warpedY, (int)warpedY, noiseInfo.m_seed);
		break;
	};
}





