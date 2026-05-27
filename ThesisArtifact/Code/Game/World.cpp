#include "Game/World.hpp"
#include "Game/Chunk.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Player.hpp"
#include "Game/WorldDefinition.hpp"
#include "Game/TerrainUtils.hpp"

#include "Engine/Renderer/RendererDX12.hpp"
#include "Engine/Renderer/VertexBufferDX12.hpp"
#include "Engine/Renderer/IndexBufferDX12.hpp"
#include "Engine/Renderer/StructuredBufferDX12.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RawNoise.hpp"
#include "Engine/Math/SmoothNoise.hpp"
#include "Engine/Core/Timer.hpp"
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Math/Splines.hpp"
#include "Engine/Renderer/TextureDX12.hpp"
#include "Engine/Renderer/ShaderDX12.hpp"
#include "Engine/Renderer/Skybox.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Math/Curves.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Core/StaticMeshUtils.hpp"

#include "ThirdParty/imgui/imgui.h"


World::World(std::string const& worldName)
	: m_lightSettings(new LightConstants())
	, m_debugSunTimer(new Timer(1.f, g_game->m_gameClock, false))
	,m_canRebuildTimer(new Timer(5.f, &Clock::GetSystemClock(), true))
{
	m_definition = WorldDefinition::GetWorldDefinitionFromName(worldName);
	m_unsavedDef = WorldDefinition(m_definition);

	m_lightSettings->m_sunDirection = m_unsavedDef.m_sunInfo.m_orientation.Get_IFwd();
	m_lightSettings->m_sunIntensity = m_unsavedDef.m_sunInfo.m_intensity;
	m_lightSettings->m_ambientIntensity = m_unsavedDef.m_sunInfo.m_ambientIntensity;
	m_lightSettings->m_sunColorRGB[0] = m_unsavedDef.m_sunInfo.m_color[0];
	m_lightSettings->m_sunColorRGB[1] = m_unsavedDef.m_sunInfo.m_color[1];
	m_lightSettings->m_sunColorRGB[2] = m_unsavedDef.m_sunInfo.m_color[2];

	m_chunkSize = m_definition->GetChunkSizeWithVoxelScale();
	m_invChunkSize = Vec3(1.f / m_chunkSize.x, 1.f / m_chunkSize.y, 1.f / m_chunkSize.z);
	m_chunkZRange = IntRange(0, (int)(m_definition->m_worldHeight / m_chunkSize.z));


	int radiusX = 1 + (int)(CHUNK_ACTIVATION_RANGE / m_chunkSize.x);
	int radiusY = 1 + (int)(CHUNK_ACTIVATION_RANGE / m_chunkSize.y);
	int radiusZ = 1 + (int)(CHUNK_ACTIVATION_RANGE / m_chunkSize.z);
	m_maxActiveChunks = (2 * radiusX) * (2 * radiusY) * (2 * radiusZ);


	m_activeChunks.reserve(m_maxActiveChunks);
	m_coordsChunkPair.reserve(m_maxActiveChunks);
	m_pendingMeshBufferChunks.reserve(m_maxActiveChunks);
	m_pendingVegetationChunks.reserve(m_maxActiveChunks);

	Strings sunArgs;
	sunArgs.push_back("Reset");
	sunArgs.push_back("Intensity=");
	sunArgs.push_back("AmbientIntensity=");
	sunArgs.push_back("SunAngles=");
	SubscribeEventCallbackFunction("SunSettings", sunArgs, Event_SunSettings);
	ResetSun();
	
}

World::~World()
{
	UnsubscribeEventCallbackFunction("SunSettings", Event_SunSettings);
	
	delete m_debugSunTimer;
	m_debugSunTimer = nullptr;
	delete m_canRebuildTimer;
	m_canRebuildTimer = nullptr;

	delete m_lightSettings;
	m_lightSettings = nullptr;

	delete m_debugFishRadiusVBO;
	m_debugFishRadiusVBO = nullptr;

	delete m_sunVBO;
	m_sunVBO = nullptr;

	delete m_sunIBO;
	m_sunIBO = nullptr;

	for (int i = 0; i < (int)FishType::COUNT; ++i)
	{
		delete(m_fishBufferInfos[i].m_fishDataBuffer);
		m_fishBufferInfos[i].m_fishDataBuffer = nullptr;
	}

}

void World::Shutdown()
{

	//g_jobSystem->AbortAllJobs();
	
	while (!m_activeJobs.empty())
	{
		ProcessFinishedJobs();
	}
	

	m_pendingMeshBufferChunks.clear();
	m_pendingVegetationChunks.clear();

	for (int i = 0; i < (int)m_activeChunks.size(); ++i)
	{
		if (m_activeChunks[i])
		{
			DeleteChunk(m_activeChunks[i]);
			m_activeChunks[i] = nullptr;
		}
	}

	std::vector<Chunk*> chunksToDelete;
	chunksToDelete.reserve(m_coordsChunkPair.size());

	for (auto const& coordsChunk : m_coordsChunkPair)
	{
		if (coordsChunk.second)
		{
			chunksToDelete.push_back(coordsChunk.second);
		}
	}

	for (Chunk* chunkToDelete : chunksToDelete)
	{
		DeleteChunk(chunkToDelete);
	}



	g_jobSystem->ResetAbort();
	m_activeJobs.clear();
	m_activeChunks.clear();
	m_coordsChunkPair.clear();
}

//Update
//----------------------------------------------------------------------------------------------------------------------------------------
void World::AttractScreenUpdate()
{
	if (m_needsInitialization)
	{
		InitWorldObjects();
		m_needsInitialization = false;
		return;
	}

	CheckKeyboardControls();

	ResourceLoadingUpdate();

	if (m_state == WorldState::ACTIVE)
	{
		UpdateDebugMessages();
		ProcessChunkLoading();
		BuildChunkMeshBuffers();
	}

	ProcessFinishedJobs();
}

void World::ResourceLoadingUpdate()
{
	if(m_state != WorldState::LOADING_RESOURCES)
		return;

	if (m_numTexturesLoaded >= m_neededTexturesLoaded && m_numFishLoaded >= m_neededFishLoaded && m_numVegetationLoaded >= m_neededVegetationLoaded)
	{
		m_unsavedDef = WorldDefinition(m_definition);
		m_state = WorldState::ACTIVE;
	}

}

void World::Update()
{
	if (m_needsInitialization)
	{
		InitWorldObjects();
		m_needsInitialization = false;
		return;
	}

	m_gameplayFrameCount++;

	//Input
	CheckKeyboardControls();
	CheckControllerControls();

	ResourceLoadingUpdate();

	UpdateLighting();

	//Chunks
	if (m_state == WorldState::ACTIVE)
	{
		DispatchFogVolumeCompute();
		UpdateChunks();
		UpdateSkyBox();
		UpdateDebugMessages();
		ProcessChunkLoading();
		BuildChunkVegetation();
		BuildChunkMeshBuffers();
		BuildFishRenderData();
	}

	//Job System
	ProcessFinishedJobs();

	if (m_renderDebugSunDirection)
	{
		Vec3 pos = g_game->m_player->m_position + (g_game->m_player->GetForwardNormal() * 2.f);
		Vec3 direction = m_unsavedDef.m_sunInfo.m_orientation.Get_IFwd();
		//DebugAddWorldArrow(pos, pos + (direction * 1.5f), 0.1f, 0.0001f, Rgba8::YELLOW);

		if (m_debugSunTimer->Tick())
		{
			m_renderDebugSunDirection = false;
		}
	}

	UpdateImGui();


	int totalNumFish = GetTotalNumFish();
	if (m_unsavedDef.m_maxNumFish < totalNumFish)
	{
		TryToDeleteFish(totalNumFish - m_unsavedDef.m_maxNumFish);
	}

	if (m_shouldRebuild)
	{
		std::string defName = m_unsavedDef.m_worldName;
		g_game->QueueLoadWorldFromWorldName(defName);
		return;
	}
}

void World::InitWorldObjects()
{
	m_neededTexturesLoaded = NUM_TERRAIN_TEXTURE_MAPS * 2 * (int)Biome::NUM_BIOMES;
	m_state = WorldState::LOADING_RESOURCES;
	for (int i = 0; i < (int)Biome::NUM_BIOMES; ++i)
	{
		LoadTerrainImages(m_definition->m_biomeTextureInfos[i].m_floorFiles, (Biome)i, true);
		LoadTerrainImages(m_definition->m_biomeTextureInfos[i].m_wallFiles, (Biome)i, false);
	}

	if (m_definition->m_xmlGenerated && m_definition->m_shouldLoadFishModels)
	{
		LoadFishMeshInfo();
	}

	if (m_definition->m_xmlGenerated && m_definition->m_shouldLoadVegetationModels)
	{
		LoadVegetationMeshInfo();
	}

	Verts sphereVerts;
	AddVertsForUVSphereZ3D(sphereVerts, Vec3::ZERO, 1.f, 8, 4);
	m_debugFishRadiusVBO = g_renderer->CreateVertexBuffer(sphereVerts, "DebugFishRadius_VBO");


	m_fogVolumeTexture = g_renderer->CreateOrGetEmpty3DTexture(IntVec2(FOG_VOLUME_TEX_DIMS_X, FOG_VOLUME_TEX_DIMS_Y), FOG_VOLUME_TEX_DEPTH, Stringf("%s_FogVolumeTexture", m_definition->m_worldName.c_str()));
	m_fogVolumeUAV = m_fogVolumeTexture->GetUnorderedAccessView();
	m_fogVolumeSRV = m_fogVolumeTexture->GetShaderResourceView();

	Verts sunVerts;
	IndexList sunIndexes;
	AddVertsForInvertedIndexedZSphere3D(sunVerts, sunIndexes, Vec3::ZERO, 1.f, Rgba8::WHITE);
	m_sunVBO = g_renderer->CreateVertexBuffer(sunVerts, "Sun_VBO");
	m_sunIBO = g_renderer->CreateIndexBuffer(sunIndexes, "Sun_IBO");

	for (int i = 0; i < (int)FishType::COUNT; ++i)
	{
		FishBufferInfo& bufferInfo = m_fishBufferInfos[i];
		std::string fishName = GetFishTypeNameFromType((FishType)i);
		bufferInfo.m_fishDataBuffer = g_renderer->CreateEmptyStructuredBuffer((size_t)m_definition->m_maxNumFish, sizeof(FishInstanceData), fishName + "_InstanceSBO");
	}
	
}

void World::UpdateLighting()
{
	if (g_game->m_player->m_flashlightOn)
	{
		Vec3 pos = g_game->m_player->m_position;
		Vec3 direction = g_game->m_player->GetForwardNormal();
		Rgba8 color = Rgba8::WHITE;
		m_lightSettings->m_lights[0] = LightData(color, pos, 5.f, 100.f, direction, 0.9f, 0.85f);
		m_lightSettings->m_numLights = 1;
	}
	
	else
	{
		m_lightSettings->m_numLights = 0;
	}

}

void World::ProcessChunkLoading()
{
	if (m_loadChunks)
	{
		if (m_coordsChunkPair.size() < m_maxActiveChunks)
		{
			LoadChunksInActivationRange();
		}

		else
		{
			UnloadChunksOutsideActivationRange();
		}
	}
}

void World::UpdateSkyBox()
{
	Vec3 const& playerPos = g_game->m_player->m_position;
	Vec3 skyBoxPos = playerPos;
	//skyBoxPos.z = m_definition->GetSeaLevel() - 500.f;
	m_definition->m_skybox->UpdatePosition(skyBoxPos);
}

void World::UpdateChunks()
{
	Frustum cameraFrustum = g_game->m_player->GetViewFrustrum();

	for (int i = 0; i < (int)m_activeChunks.size(); ++i)
	{
		if (m_activeChunks[i])
		{
			m_activeChunks[i]->Update(cameraFrustum);
		}
	}

	for (int i = 0; i < (int)m_activeChunks.size(); ++i)
	{
		if (m_activeChunks[i])
		{
			m_activeChunks[i]->UpdateGarbageFish();
		}
	}
}

void World::UpdateDebugMessages()
{
	if (m_showJobInfo)
	{
		int numChunkTypes[(int)ChunkState::COUNT] = {};
		for (auto it = m_coordsChunkPair.begin(); it != m_coordsChunkPair.end(); ++it)
		{
			Chunk* chunk = it->second;
			ChunkState state = chunk->m_state;
			numChunkTypes[(int)state] += 1;
		}

		int totalNumChunks = 0;
		int numSolidChunks = 0;
		int numEmptyChunks = 0;
		int numMeshChunks = 0;
		for (int i = 0; i < (int)m_activeChunks.size(); ++i)
		{
			if (m_activeChunks[i])
			{
				totalNumChunks++;
				if (m_activeChunks[i]->m_isSolidChunk)
				{
					numSolidChunks++;
				}

				else if (m_activeChunks[i]->m_isEmptyChunk)
				{
					numEmptyChunks++;
				}

				else
				{
					numMeshChunks++;
				}
			}
		}

		AABB2 bounds = g_game->GetScreenBounds();
		bounds.AddPadding(-0.01f, -0.01f, -0.65f, -0.65f);

		constexpr int NUM_LINES = 25;
		std::vector<AABB2> boundsSlices = bounds.GetHorizontalSlicedBoxesTopToBottom(NUM_LINES);
		std::string lines[NUM_LINES] =
		{
			"-------Chunks-------",
			Stringf("Num Chunks:       %i / %i", totalNumChunks, m_maxActiveChunks),
			Stringf("Pending Generate:       %i", numChunkTypes[0]),
			Stringf("Generating:             %i", numChunkTypes[1]),
			Stringf("Pending Mesh:           %i", numChunkTypes[2]),
			Stringf("Pending Activate:       %i", numChunkTypes[3]),
			Stringf("Active:                 %i", numChunkTypes[4]),
			Stringf("Garbage:                %i", numChunkTypes[5]),
			" ",
			Stringf("Solid:                  %i", numSolidChunks),
			Stringf("Empty:                  %i", numEmptyChunks),
			Stringf("Mesh Chunks:            %i", numMeshChunks),
			" ",
			"--------Jobs--------",
			Stringf("Active Jobs:            %i", (int)m_activeJobs.size()),
			Stringf("Max Num Jobs:           %i", MAX_NUM_WORLD_JOBS),
			Stringf("Num Workers:            %i", g_jobSystem->GetNumWorkerThreads()),
			Stringf("Pending:                %i", g_jobSystem->GetNumPendingJobs()),
			Stringf("Executing:              %i", g_jobSystem->GetNumExecutingJobs()),
			Stringf("Completed:              %i", g_jobSystem->GetNumCompletedJobs()),
			" ",
			"-Avg Generate Speed-",
			Stringf("Mesh Chunk:             %.2f ms", m_averageMeshGenerateSpeed),
			Stringf("Empty Chunk:            %.2f ms", m_averageMeshlessGenerateSpeed),
			Stringf("Buffer Load:            %.2f ms", m_averageMeshBufferSpeed),
		};

		for (int i = 0; i < NUM_LINES; ++i)
		{
			DebugAddScreenText(lines[i], boundsSlices[i], bounds.GetHeight(), Vec2(0.f, 0.5f), 0.f);
		}

	}

	if (m_showMemoryInfo)
	{
		size_t voxelSize = 0;
		size_t cpuSize = 0;
		size_t gpuSize = 0;
		size_t totalMeshSize = 0;
		int numMeshChunks = 0;
		size_t totalMeshLessSize = 0;
		int numMeshlessChunks = 0;
		int numVerts = 0;
		int numIndexes = 0;

		for (int i = 0; i < (int)m_activeChunks.size(); ++i)
		{
			if (m_activeChunks[i])
			{
				size_t chunkVoxelSize = 0;
				size_t chunkCpuSize = 0;
				size_t chunkGpuSize = 0;
				m_activeChunks[i]->GetApproximateMemoryUsage(chunkVoxelSize, chunkCpuSize, chunkGpuSize);
				voxelSize += chunkVoxelSize;
				cpuSize += chunkCpuSize;
				gpuSize += chunkGpuSize;
				numVerts += m_activeChunks[i]->m_numVerts;
				numIndexes += m_activeChunks[i]->m_numIndexes;

				if (m_activeChunks[i]->IsMeshChunk())
				{
					totalMeshSize += chunkCpuSize;
					numMeshChunks++;
				}

				else
				{
					totalMeshLessSize += chunkCpuSize;
					numMeshlessChunks++;
				}
			}
		}

		const float avgMeshCPUSize = (float)totalMeshSize / GetClamped((float)numMeshChunks, 1.f, (float)m_maxActiveChunks);
		const int avgMeshLessCpuSize = (int)totalMeshLessSize / GetClampedInt(numMeshlessChunks, 1, m_maxActiveChunks);

		const int avgNumVerts = numVerts / GetClampedInt(numMeshChunks, 1, m_maxActiveChunks);
		const int avgNumIndexes = numIndexes / GetClampedInt(numMeshChunks, 1, m_maxActiveChunks);

		constexpr float BYTES_TO_GB = 0.0000000009313225746f;
		constexpr float BYTES_TO_MB = 0.000000953674316f;

		//World Def Size
		size_t worldDefTotalSize = WorldDefinition::GetTotalWorldDefSize();

		AABB2 bounds = g_game->GetScreenBounds();
		bounds.AddPadding(-0.75f, -0.01f, -0.01f, -0.65f);

		constexpr int NUM_LINES = 14;
		std::vector<AABB2> boundsSlices = bounds.GetHorizontalSlicedBoxesTopToBottom(NUM_LINES);
		std::string lines[NUM_LINES] =
		{
			"-------Chunks Memory-------",
			Stringf("Total Voxel Memory:        %.2f gb", (float)voxelSize * BYTES_TO_GB),
			Stringf("Total CPU Memory:          %.2f gb", (float)cpuSize * BYTES_TO_GB),
			Stringf("Total GPU Memory:          %.2f gb", (float)gpuSize * BYTES_TO_GB),
			Stringf("Num Mesh Chunks:           %i", numMeshChunks),
			Stringf("Avg Mesh Chunk Size:       %.2f mb", (float)avgMeshCPUSize * BYTES_TO_MB),
			Stringf("Num Meshless Chunks:       %i", numMeshlessChunks),
			Stringf("Avg Meshless Chunk Size:   %i bytes", avgMeshLessCpuSize),
			" ",
			Stringf("Avg Mesh Chunk Verts:      %i", avgNumVerts),
			Stringf("Avg Mesh Chunk Indexes:    %i", avgNumIndexes),
			" ",
			"-------World Definition Memory-------",
			Stringf("World Definitions:         %.2f mb", (float)worldDefTotalSize * BYTES_TO_MB),
		};

		for (int i = 0; i < NUM_LINES; ++i)
		{
			DebugAddScreenText(lines[i], boundsSlices[i], bounds.GetHeight(), Vec2(0.f, 0.5f), 0.f);
		}
	}

	switch (m_debugView)
	{
	case WorldDebugView::NONE:
		break;
	case WorldDebugView::WIRE_FRAME:
		DebugAddMessage("Debug View: Terrain Wireframe", 0.f, Rgba8::ORANGE);
		break;
	case WorldDebugView::CHUNK_BOUNDS:
		DebugAddMessage("Debug View: Chunks", 0.f, Rgba8::ORANGE);
		break;
	case WorldDebugView::MESH_CHUNK_BOUNDS:
		DebugAddMessage("Debug View: Mesh Chunks", 0.f, Rgba8::ORANGE);
		break;
	case WorldDebugView::DEBUG_FISH:
		DebugAddMessage("-Separation Radius\n", 0.f, Rgba8::RED);
		DebugAddMessage("-Flocking Radius\n", 0.f, Rgba8::GREEN);
		DebugAddMessage("-Collision Radius\n", 0.f, Rgba8::MAGENTA);
		DebugAddMessage("Debug View: Fish", 0.f, Rgba8::ORANGE);
		break;
	default:
		break;
	}
}

void World::UpdateImGui()
{
	if(!g_imGuiSystem)
		return;

	ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("Underwater World Editor");
	if (ImGui::BeginTabBar("Underwater World"))
	{
		if (ImGui::BeginTabItem("Load Worlds"))
		{
			ImGui::SeparatorText("Worlds");
			ImGui::TextWrapped("Press 'Generate Changes' to see changes or press 'Reset to Default' to reset. 'Recent Changes' will let you reload previous worlds that are not saved on disk");
			ImGui::Spacing();

			ImGui::BeginDisabled(!m_canRebuildTimer->HasPeriodElapsed());
			if (ImGui::Button("Generate Changes"))
			{
				LoadChangedWorld();
			}

			if (ImGui::Button("Reset to Default"))
			{
				ResetWorldToDefault();
			}
			ImGui::EndDisabled();

			ImGui::InputText("World Name", &m_unsavedDef.m_worldName[0], 600);

			if (ImGui::CollapsingHeader("Recent Changes"))
			{
				for (int i = (int)WorldDefinition::s_worldDefinitions.size() - 1; i >= 0; --i)
				{
					std::string saveName = WorldDefinition::s_worldDefinitions[i].m_worldName;

					if (ImGui::Button(saveName.c_str()))
					{
						SelectMostRecentSaves(i);
					}
				}
			}

			if (ImGui::CollapsingHeader("XML"))
			{
				const int INPUT_TEXT_LENGTH = 600;
				ImGui::Text("Data/Definitions/WorldDefinitions/SavedWorlds/....xml");
				ImGui::InputText("World Name ##XML", &m_unsavedDef.m_worldName[0], INPUT_TEXT_LENGTH);
				if (ImGui::Button("Save World As XML Doc"))
				{
					m_unsavedDef.SaveDefinitionAsXML();
				}

				ImGui::Text("Data/Definitions/WorldDefinitions/SavedWorlds/....xml");
				ImGui::InputText("World to Load File Path", &m_loadXMLName[0], INPUT_TEXT_LENGTH);
				if (ImGui::Button("Load from XML"))
				{
					TryLoadFromXML(Stringf("Data/Definitions/WorldDefinitions/SavedWorlds/%s.xml", m_loadXMLName.c_str()));
				}
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Terrain"))
		{
			ImGui::SeparatorText("Procedural Terrain");
			ImGui::TextWrapped("Use the controls below to customize the terrain. For any changes to be seen, go to the 'Load Worlds Page'");
			ImGui::Spacing();

			if (ImGui::CollapsingHeader("World Base Settings"))
			{
				ImGui::SliderFloat("World Max Height", &m_unsavedDef.m_worldHeight, (float)CHUNK_SIZE_Z * 2.f, 2000.f);
				ImGui::SliderFloat("Base Terrain Depth Below Sea Level", &m_unsavedDef.m_baseTerrainDepthBelowSeaLevel, -500, 500);
				ImGui::SliderFloat("Max Terrain Depth Below Sea Level", &m_unsavedDef.m_waterConstants.m_maxTerrainDepth, 100.f, m_unsavedDef.GetSeaLevel());

				if (ImGui::SliderInt("Voxel Scale", &m_unsavedDef.m_voxelScale, 1, 16))
				{
					m_unsavedDef.m_invVoxelScale = 1.f / m_unsavedDef.m_voxelScale;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Smaller voxel scale will have more world detail. Larger scale = less detail, but better performance");
				}
			}

			if (ImGui::CollapsingHeader("Terrain Noise"))
			{
				if (ImGui::TreeNode("Default Settings"))
				{
					ImGui::InputInt("Game Seed", &m_unsavedDef.m_gameSeed);
					ImGui::SliderFloat("Default Octave Persistence", &m_unsavedDef.m_defaultOctavePersistance, 0.f, 1.f);
					ImGui::SliderFloat("Default Octave Scale", &m_unsavedDef.m_defaultOctaveScale, 1.f, 10.f);

					ImGui::SliderFloat("Biome Blend", &m_unsavedDef.m_biomeBlend, 0.f, 1.f);

					ImGui::Spacing();
					float warpScale[2] = { m_unsavedDef.m_warpNoiseScaleXY.x , m_unsavedDef.m_warpNoiseScaleXY.y };
					if (ImGui::SliderFloat2("Warp Noise Scale", warpScale, 1.f, 2000.f))
					{
						m_unsavedDef.m_warpNoiseScaleXY.x = warpScale[0];
						m_unsavedDef.m_warpNoiseScaleXY.y = warpScale[1];
					}
					ImGui::SliderInt("Warp Noise Num Octaves", &m_unsavedDef.m_warpNoiseNumOctaves, 1, 15);

					ImGui::Spacing();
					ImGui::SliderFloat("Base Terrain Bias Factor ", &m_unsavedDef.m_baseTerrainBiasFactor, 0.01f, 1.f);
					ImGui::SliderFloat("Final Terrain Bias Factor ", &m_unsavedDef.m_finalTerrainBiasFactor, 0.01f, 1.f);
					ImGui::TreePop();
				}

				std::string toolTips[(int)TerrainNoiseType::COUNT]
				{
					"Used to determine where vegetation should be"
					"Used to determine where vegetation should be"
					"Tiny rapid break up of smooth terrain",
					"Small gradual changes in elevation. Helps break up continentalness smoothness",
					"Fast elevation changes. Creates channels and canyons",
					"Large gradual changes in terrain elevation",
					"Initial 3D Noise Layer. Will help generate overhangs",
				};

				for (int i = (int)TerrainNoiseType::COUNT - 1; i >= 0; --i)
				{
					TerrainNoiseInfo& noiseInfo = m_unsavedDef.m_terrainNoiseInfos[i];
					std::string noiseName = noiseInfo.m_name;

					if(noiseName == UN_INITIALIZED_WORLD_DATA_NAME)
						continue;

					if (ImGui::TreeNode(Stringf("%s ##%i", noiseName.c_str(), i).c_str()))
					{
						ImGui::Checkbox(noiseName.c_str(), &noiseInfo.m_isActive);
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip(toolTips[i].c_str());
						}
						ImGui::InputInt(Stringf("Seed ##%i", i).c_str(), &noiseInfo.m_seed);
						ImGui::SliderFloat(Stringf("Scale ##%i", i).c_str(), &noiseInfo.m_scale, 50.f, 1000.f);
						ImGui::SliderInt(Stringf("Num Noise Octaves ##%i", i).c_str(), &noiseInfo.m_numOctaves, 1, 15);
						ImGui::SliderFloat(Stringf("Strength ##i", i).c_str(), &noiseInfo.m_strength, 0.f, 10.f);
						ImGui::SliderFloat(Stringf("Warp Strength ##i", i).c_str(), &noiseInfo.m_warpStrength, 0.f, 2000.f);

						ImGui::TreePop();
					}
				}
			}

			if (ImGui::CollapsingHeader("Terrain Textures"))
			{
				const int INPUT_TEXT_LENGTH = 600;
				for (int i = 0; i < (int)Biome::NUM_BIOMES; ++i)
				{
					std::string biomeName = GetBiomeNameFromType((Biome)i);
					if (ImGui::CollapsingHeader(biomeName.c_str()))
					{
						BiomeTextureInfo& biomeInfo = m_unsavedDef.m_biomeTextureInfos[i];
						TerrainBiomeTextureConstants& textureConstants = m_unsavedDef.m_textureConstants;

						if (ImGui::TreeNode(Stringf("Floor ##%s", biomeName.c_str()).c_str()))
						{
							std::string tag = Stringf("##%s Floor", biomeName.c_str());
							ImGui::SliderFloat(Stringf("Texture Scale %s", tag.c_str()).c_str(), &textureConstants.m_textureScales[i], 0.f, 1.f);
							ImGui::SliderFloat(Stringf("Normals Strength %s", tag.c_str()).c_str(), &textureConstants.m_normalStrengths[i], 0.f, 10.f);
							ImGui::InputText(Stringf("Parent Folder %s", tag.c_str()).c_str(), &biomeInfo.m_floorFiles.m_parentFolder[0], INPUT_TEXT_LENGTH);
							if (ImGui::Button(Stringf("Find Textures %s", tag.c_str()).c_str()))
							{
								LoadChildrenTexturePathsFromParentFolder(biomeInfo.m_floorFiles, biomeInfo.m_floorFiles.m_parentFolder);
							}

							if (ImGui::TreeNode(Stringf("Textures %s", tag.c_str()).c_str()))
							{
								ImGui::InputText("Albedo", &biomeInfo.m_floorFiles.m_albedoFile[0], INPUT_TEXT_LENGTH);
								ImGui::InputText("Normal", &biomeInfo.m_floorFiles.m_normalFile[0], INPUT_TEXT_LENGTH);
								ImGui::InputText("Ambient Occlusion", &biomeInfo.m_floorFiles.m_aoFile[0], INPUT_TEXT_LENGTH);
								ImGui::TreePop();
							}

							ImGui::TreePop();
						}

						if (ImGui::TreeNode(Stringf("Walls ##%s", biomeName.c_str()).c_str()))
						{
							std::string tag = Stringf("##%s Walls", biomeName.c_str());
							ImGui::SliderFloat(Stringf("Texture Scale %s", tag.c_str()).c_str(), &textureConstants.m_textureScales[i + (int)Biome::NUM_BIOMES], 0.f, 1.f);
							ImGui::SliderFloat(Stringf("Normals Strength %s", tag.c_str()).c_str(), &textureConstants.m_normalStrengths[i + (int)Biome::NUM_BIOMES], 0.f, 10.f);
							ImGui::InputText(Stringf("Parent Folder %s", tag.c_str()).c_str(), &biomeInfo.m_wallFiles.m_parentFolder[0], INPUT_TEXT_LENGTH);
							if (ImGui::Button(Stringf("Find Textures %s", tag.c_str()).c_str()))
							{
								LoadChildrenTexturePathsFromParentFolder(biomeInfo.m_wallFiles, biomeInfo.m_wallFiles.m_parentFolder);
							}

							if (ImGui::TreeNode(Stringf("Textures %s", tag.c_str()).c_str()))
							{
								ImGui::InputText("Albedo", &biomeInfo.m_wallFiles.m_albedoFile[0], INPUT_TEXT_LENGTH);
								ImGui::InputText("Normal", &biomeInfo.m_wallFiles.m_normalFile[0], INPUT_TEXT_LENGTH);
								ImGui::InputText("Ambient Occlusion", &biomeInfo.m_wallFiles.m_aoFile[0], INPUT_TEXT_LENGTH);
								ImGui::TreePop();
							}

							ImGui::TreePop();
						}
					}
				}
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Water/Wind"))
		{
			ImGui::SeparatorText("Water");
			ImGui::TextWrapped("Use the controls below to customize the ocean surface and underwater currents");
			ImGui::Spacing();

			if (ImGui::CollapsingHeader("Ocean Surface"))
			{
				ImGui::Checkbox("Ocean Surface ##checkbox", &m_unsavedDef.m_activeWaterSurface);
				ImGui::SliderFloat("Sea Level Height", &m_unsavedDef.m_waterConstants.m_seaLevel, 0.f, m_unsavedDef.m_worldHeight);

				ImGui::SliderFloat("Foaminess", &m_unsavedDef.m_waterConstants.m_foaminess, 0.f, 1.f);
				ImGui::SliderFloat("Shoreline Foam Strength", &m_unsavedDef.m_waterConstants.m_shorelineFoamStrength, 0.f, 1.f);
				ImGui::SliderFloat("Mirkyness", &m_unsavedDef.m_waterConstants.m_mirkyness, 0.f, 1.f);
				ImGui::SliderFloat("Reflection Strength", &m_unsavedDef.m_waterConstants.m_reflectionStrength, 0.f, 1.f);
				ImGui::SliderFloat("Refraction Strength", &m_unsavedDef.m_waterConstants.m_refractionStrength, 0.f, 1.f);
				ImGui::SliderFloat("Specular Intensity", &m_unsavedDef.m_waterConstants.m_specularIntensity, 0.f, 1.f);
				ImGui::SliderFloat("Specular Power", &m_unsavedDef.m_waterConstants.m_specularPower, 0.f, 1.f);

				if (ImGui::TreeNode("Waves"))
				{
					ImGui::SeparatorText("Global Wave Multipliers");
					ImGui::SliderFloat("Amplitude", &m_unsavedDef.m_waterConstants.m_globalAmplitudeScale, 0.f, 5.f);
					ImGui::SliderFloat("Wavelength", &m_unsavedDef.m_waterConstants.m_globalWavelengthScale, 0.f, 5.f);
					ImGui::SliderFloat("Speed", &m_unsavedDef.m_waterConstants.m_globalWaveSpeedScale, 0.f, 10.f);
					ImGui::SliderFloat("Crest Steepness", &m_unsavedDef.m_waterConstants.m_globalCrestSteepnessScale, 0.f, 5.f);

					ImGui::SeparatorText("Noise Variation");
					ImGui::SliderFloat("Direction Variance Region Scale", &m_unsavedDef.m_waterConstants.m_directionVarianceRegionScale, 0.f, 1.f);
					ImGui::SliderFloat("Direction Variance", &m_unsavedDef.m_waterConstants.m_directionVariance, 0.f, 1.f);
					ImGui::SliderFloat("Amplitude Variance Region Scale", &m_unsavedDef.m_waterConstants.m_amplitudeVarianceRegionScale, 0.f, 1.f);
					ImGui::SliderFloat("Amplitude Variance", &m_unsavedDef.m_waterConstants.m_amplitudeVariance, 0.f, 1.f);

					ImGui::SeparatorText("Individual Waves");
					ImGui::SliderInt("Num Waves", &m_unsavedDef.m_waterConstants.m_numWaves, 0, MAX_NUM_WAVES);
					for (int i = 0; i < m_unsavedDef.m_waterConstants.m_numWaves; ++i)
					{
						if (ImGui::TreeNode(Stringf("Wave %i ##%i", i, i).c_str()))
						{
							float xyDirection[2] = { m_unsavedDef.m_waterConstants.m_waveData[i].m_directionXY.x, m_unsavedDef.m_waterConstants.m_waveData[i].m_directionXY.y };
							if (ImGui::SliderFloat2(Stringf("XY Direction ##%i", i).c_str(), xyDirection, -1.f, 1.f))
							{
								m_unsavedDef.m_waterConstants.m_waveData[i].m_directionXY.x = xyDirection[0];
								m_unsavedDef.m_waterConstants.m_waveData[i].m_directionXY.y = xyDirection[1];
							}

							ImGui::Spacing();
							ImGui::SliderFloat(Stringf("Amplitude ##%i", i).c_str(), &m_unsavedDef.m_waterConstants.m_waveData[i].m_amplitude, 0.f, 100.f);
							ImGui::SliderFloat(Stringf("Wavelength ##%i", i).c_str(), &m_unsavedDef.m_waterConstants.m_waveData[i].m_waveLength, 0.f, 1000.f);
							ImGui::SliderFloat(Stringf("Wave Speed ##%i", i).c_str(), &m_unsavedDef.m_waterConstants.m_waveDataExtra[i].m_waveSpeed, 0.f, 10.f);
							ImGui::SliderFloat(Stringf("Wave Crest Steepness ##%i", i).c_str(), &m_unsavedDef.m_waterConstants.m_waveDataExtra[i].m_waveSteepness, 0.f, 1.f);
							ImGui::SliderFloat(Stringf("Wave Offset ##%i", i).c_str(), &m_unsavedDef.m_waterConstants.m_waveDataExtra[i].m_waveOffset, 0.f, 2.f * pi);
							ImGui::TreePop();
						}
					}

					ImGui::TreePop();
				}
			}

			if (ImGui::CollapsingHeader("Wind/Currents"))
			{
				for (int i = 0; i < (int)WindType::COUNT; ++i)
				{
					std::string windName = GetWindTypeNameFromType((WindType)i);
					if (ImGui::TreeNode(windName.c_str()))
					{
						WindConstants& windInfo = m_unsavedDef.m_windConstants[i];

						ImGui::Checkbox(Stringf("%s ##%i",windName.c_str(), i).c_str(), &m_unsavedDef.m_activeWind[i]);
						ImGui::SliderFloat(Stringf("Swirl Strength ##%i", i).c_str(), &windInfo.m_swirlStrength, 0.f, 1.f);
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("How much verticality or 'lift' in wind direction");
						}
						ImGui::SliderFloat(Stringf("Warble Strength ##%i", i).c_str(), &windInfo.m_warbleStrength, 0.f, 1.f);
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("How much deviation from base roam direction at any xy position");
						}
						ImGui::SliderFloat(Stringf("Base Speed ##%i", i).c_str(), &windInfo.m_baseSpeed, 0.f, 1.f);

						ImGui::Spacing();
						ImGui::SeparatorText("Wind Currents");
						ImGui::TextWrapped("Individual currents which affect the overall wind direction. 'Large' current has largest influence on base roam direction ");
						ImGui::SliderFloat(Stringf("Large Current Scale ##%i", i).c_str(), &windInfo.m_largeScale, 0.f, 1.f);
						ImGui::SliderFloat(Stringf("Large Current Speed ##%i", i).c_str(), &windInfo.m_largeSpeed, 0.f, 1.f);
						ImGui::SliderFloat(Stringf("Large Current Strength ##%i", i).c_str(), &windInfo.m_largeStrength, 0.f, 1.f);
						ImGui::Spacing();
						ImGui::SliderFloat(Stringf("Medium Current Scale ##%i", i).c_str(), &windInfo.m_mediumScale, 0.f, 1.f);
						ImGui::SliderFloat(Stringf("Medium Current Speed ##%i", i).c_str(), &windInfo.m_mediumSpeed, 0.f, 1.f);
						ImGui::SliderFloat(Stringf("Medium Current Strength ##%i", i).c_str(), &windInfo.m_mediumStrength, 0.f, 1.f);
						ImGui::Spacing();
						ImGui::SliderFloat(Stringf("Small Current Scale ##%i", i).c_str(), &windInfo.m_smallScale, 0.f, 1.f);
						ImGui::SliderFloat(Stringf("Small Current Speed ##%i", i).c_str(), &windInfo.m_smallSpeed, 0.f, 1.f);
						ImGui::SliderFloat(Stringf("Small Current Strength ##%i", i).c_str(), &windInfo.m_smallStrength, 0.f, 1.f);
						ImGui::TreePop();
					}
					
				}
				
			}


			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Lighting/Fog"))
		{
			ImGui::SeparatorText("Lighting and Fog");
			ImGui::TextWrapped("Use the controls below to customize the lighting and fog");
			ImGui::Spacing();

			if (ImGui::CollapsingHeader("Sun"))
			{
				SunConstants& sunInfo = m_unsavedDef.m_sunInfo;
				float orientationF[3] = {sunInfo.m_orientation.m_yawDegrees, sunInfo.m_orientation.m_pitchDegrees, sunInfo.m_orientation.m_rollDegrees};
				if (ImGui::SliderFloat3("Sun Direction", orientationF, -360.f, 360.f))
				{
					EulerAngles orientation(orientationF[0], orientationF[1], orientationF[2]);
					sunInfo.m_orientation = orientation;
					m_lightSettings->m_sunDirection = orientation.Get_IFwd();
				}

				if (ImGui::SliderFloat("Sun Intensity", &sunInfo.m_intensity, 0.f, 1.f))
				{
					m_lightSettings->m_sunIntensity = sunInfo.m_intensity;
				}

				if (ImGui::SliderFloat("Ambient Intensity", &sunInfo.m_ambientIntensity, 0.f, 1.f))
				{
					m_lightSettings->m_ambientIntensity = sunInfo.m_ambientIntensity;
				}

				if (ImGui::ColorPicker3("Color ##Sun", sunInfo.m_color))
				{
					m_lightSettings->m_sunColorRGB[0] = sunInfo.m_color[0];
					m_lightSettings->m_sunColorRGB[1] = sunInfo.m_color[1];
					m_lightSettings->m_sunColorRGB[2] = sunInfo.m_color[2];
				}
			}

			if (ImGui::CollapsingHeader("Depth Attenuation"))
			{
				ImGui::SliderFloat("Underwater Color Attenutation Depth Scale", &m_unsavedDef.m_waterConstants.m_underwaterTintDepthScale, 0.f, 1.f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How quickly everything turns blue");
				}

				ImGui::SliderFloat("Underwater Depth Darken Scale", &m_unsavedDef.m_waterConstants.m_underwaterDepthDarkenScale, 0.f, 3.f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How quickly light is absorbed in water");
				}
			}

			if (ImGui::CollapsingHeader("Caustics"))
			{

				ImGui::Checkbox("Caustics ##checkbox", &m_unsavedDef.m_activeCaustics);
				ImGui::SliderFloat("Intensity ##caustics", &m_unsavedDef.m_causticConstants.m_causticsIntensity, 0.f, 5.f);
				ImGui::SliderFloat("Depth Fade Rate", &m_unsavedDef.m_causticConstants.m_causticsDepthFadeRate, 0.f, 1.f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How deep caustics can be seen");
				}
				ImGui::SliderFloat("Warp Speed", &m_unsavedDef.m_causticConstants.m_causticsWarpSpeed, 0.f, 1.f);
				ImGui::SliderFloat("Warp Strength", &m_unsavedDef.m_causticConstants.m_causticsWarpStrength, 0.f, 0.5f);
				ImGui::SliderFloat("Line Thickness", &m_unsavedDef.m_causticConstants.m_causticsLineThickness, 0.f, 5.f);
				ImGui::SliderFloat("Ripple Speed", &m_unsavedDef.m_causticConstants.m_causticsRippleSpeed, 0.f, 5.f);
				ImGui::SliderFloat("Ripple Strength", &m_unsavedDef.m_causticConstants.m_causticsRippleStrength, 0.f, 1.f);
			}

			if (ImGui::CollapsingHeader("Fog"))
			{
				ImGui::Checkbox("Fog ##checkbox", &m_unsavedDef.m_activeFog);
				FogConstants& fogConst = m_unsavedDef.m_fogConstants;
				fogConst.m_underwaterTintDepthScale = m_unsavedDef.m_waterConstants.m_underwaterTintDepthScale;
				ImGui::SliderFloat("Fog Volume Height", &fogConst.m_fogVolumeHeight, 10.f, 500.f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How far above the Sea Level that fog persists");
				}

				if (ImGui::TreeNode("Underwater Fog"))
				{
					FogData& fogData = fogConst.m_fogData[UNDERWATER_FOG_INDEX];
					ImGui::SliderFloat(Stringf("Fog Depth ##%i", 0).c_str(), &fogData.m_depth, 0.f, 1000.f);
					ImGui::SliderFloat(Stringf("Fog Max Opacity ##%i", 0).c_str(), &fogData.m_opacity, 0.f, 1.f);
					ImGui::SliderInt(Stringf("Fog Transition Exponent ##%i", 0).c_str(), &fogData.m_transition, 0, 10);
					ImGui::Spacing();
					ImGui::ColorPicker4(Stringf("Color ##%i", 0).c_str(), fogData.m_color);
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Above Surface Fog"))
				{
					FogData& fogData = fogConst.m_fogData[ABOVE_WATER_FOG_INDEX];
					
					ImGui::SliderFloat(Stringf("Fog Depth ##%i", 1).c_str(), &fogData.m_depth, 0.f, 1000.f);
					ImGui::SliderFloat(Stringf("Fog Max Opacity ##%i", 1).c_str(), &fogData.m_opacity, 0.f, 1.f);
					ImGui::SliderInt(Stringf("Fog Transition Exponent ##%i", 1).c_str(), &fogData.m_transition, 0, 10);
					ImGui::Spacing();
					ImGui::ColorPicker4(Stringf("Color ##%i", 1).c_str(), fogData.m_color);
					ImGui::TreePop();
				}

			}

			if (ImGui::CollapsingHeader("Light Rays"))
			{
				LightRayConstants& lightRayConst = m_unsavedDef.m_lightRayConstants;
				if (ImGui::Checkbox("Underwater Light Rays", &m_unsavedDef.m_activeLightRays))
				{
					lightRayConst.m_renderLightRays = m_unsavedDef.m_activeLightRays ? 1 : 0;
				}

				ImGui::SliderFloat("Intensity ##lightrays", &lightRayConst.m_shaftIntensity, 0.f, 1.f);
				ImGui::SliderFloat("Scale", &lightRayConst.m_shaftScale, 0.f, 1.f);
				ImGui::SliderFloat("Spread", &lightRayConst.m_shaftSpread, 0.f, 1.f);
				ImGui::SliderFloat("Depth Fade", &lightRayConst.m_shaftDepthFade, 0.f, 1.f);
				ImGui::SliderFloat("Threshold", &lightRayConst.m_shaftThreshold, 0.f, 1.f);
				ImGui::SliderFloat("Warp Speed", &lightRayConst.m_shaftWarpSpeed, 0.f, 1.f);
				ImGui::SliderFloat("Contrast", &lightRayConst.m_shaftContrast, 0.f, 1.f);

				ImGui::SliderFloat("Warp Strength", &lightRayConst.m_shaftWarpStrength, 0.f, 1.f);
				int numSteps = lightRayConst.m_maxNumSteps;
				if (ImGui::SliderInt("Max Num Shaft Ray Steps", &numSteps, 4, 256))
				{
					numSteps = ((numSteps + 2) / 4) * 4;
					lightRayConst.m_maxNumSteps = numSteps;
				}

				ImGui::ColorPicker4("Shaft Color", lightRayConst.m_shaftColor);
			}

			if (ImGui::CollapsingHeader("Particles"))
			{
				ParticleConstants& particleData = m_unsavedDef.m_particleConstants;
				if (ImGui::Checkbox("Underwater Particles", &m_unsavedDef.m_activeParticles))
				{
					particleData.m_activeParticles = m_unsavedDef.m_activeParticles ? 1 : 0;
				}
				ImGui::SliderFloat("Density", &particleData.m_density, 0.f, 1.f);

				float sizeRange[2] = {particleData.m_sizeRange.m_min, particleData.m_sizeRange.m_max};
				if (ImGui::SliderFloat2("Size Range", sizeRange, 0.f, 1.f))
				{
					particleData.m_sizeRange = FloatRange(sizeRange[0], sizeRange[1]);
				}

				ImGui::SliderFloat("Cell Size", &particleData.m_cellSize, 0.f, 1.f);
				ImGui::SliderInt("Particles Per Cell", &particleData.m_particlesPerCell, 1, 20);
				ImGui::SliderFloat("Intensity ##particle", &particleData.m_intensity, 0.f, 1.f);
				ImGui::SliderFloat("Depth Fade", &particleData.m_depthFade, 0.f, 1.f);
				ImGui::SliderFloat("Drift Speed", &particleData.m_driftSpeed, 0.f, 1.f);

			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Vegetation"))
		{
			ImGui::SeparatorText("Vegetation");
			ImGui::TextWrapped("Use the controls below to customize the vegetation. For any changes to be seen, go to the 'Load Worlds Page'");
			ImGui::Spacing();

			ImGui::Checkbox("Vegetation", &m_unsavedDef.m_activeVegetation);

			for (int i = 0; i < NUM_NON_CORAL_VEGETATION_TYPES; ++i)
			{
				VegetationInfo& vegInfo = m_unsavedDef.m_vegetationInfos[i];
				VegetationConstants& vegConstants = vegInfo.m_vegConstants;
				std::string vegName = GetVegetationTypeNameFromType((VegetationType)i);

				if(vegName == UN_INITIALIZED_WORLD_DATA_NAME)
					continue;

				if (ImGui::CollapsingHeader(Stringf("%s ##%i",vegName.c_str(), i).c_str()))
				{
					
					ImGui::Checkbox(vegName.c_str(), &vegInfo.m_isActive);
					ImGui::SliderFloat(Stringf("Thickness ##%i", i).c_str(), &vegInfo.m_thickness, 0.f, 1.f);
					ImGui::SliderFloat(Stringf("Dot Product World Up Threshold ##%i", i).c_str(), &vegInfo.m_worldDotUpThreshold, -1.f, 1.f);
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("How flat the terrain must be to spawn vegetation. 1 = terrain normal must point straight upwards, -1 = any terrain can spawn vegetation");
					}

					ImGui::Spacing();
					float heightRange[2] = { vegConstants.m_heightRange.m_min , vegConstants.m_heightRange.m_max };
					if (ImGui::SliderFloat2(Stringf("Height Range ##%i", i).c_str(), heightRange, 1.f, 200.f))
					{
						vegConstants.m_heightRange.m_min = heightRange[0];
						vegConstants.m_heightRange.m_max = heightRange[1];
					}


					float widthRange[2] = { vegConstants.m_widthRange.m_min , vegConstants.m_widthRange.m_max };
					if (ImGui::SliderFloat2(Stringf("Width Range ##%i", i).c_str(), widthRange, 1.f, 50.f))
					{
						vegConstants.m_widthRange.m_min = widthRange[0];
						vegConstants.m_widthRange.m_max = widthRange[1];
					}

					ImGui::SliderFloat(Stringf("Rigidity ##%i", i).c_str(), &vegConstants.m_rigidity, 0.f, 1.f);

					if ((VegetationType)i == VegetationType::KELP)
					{
						ImGui::SliderFloat(Stringf("Emissive Strength ##%i").c_str(), &vegConstants.m_emissiveStrength, 0.f, 20.f);
					}

				}
			}

			if (ImGui::CollapsingHeader("Coral"))
			{
				ImGui::Checkbox("Coral ##1", &m_unsavedDef.m_activeCoral);

				ImGui::SliderFloat("Coral Spawn Thickness", &m_unsavedDef.m_coralClusterThickness, 0.f, 1.f);
				ImGui::SliderFloat("Coral Depth Blend Strength", &m_unsavedDef.m_coralBlendConstants.m_depthBlendStrength, 0.f, 1.f);
				ImGui::SliderFloat("Coral Depth Blend Threshold", &m_unsavedDef.m_coralBlendConstants.m_depthBlendThreshold, 0.f, 1.f);
				ImGui::SliderFloat("Coral Blend Radius", &m_unsavedDef.m_coralBlendConstants.m_blendRadius, 0.f, 1.f);

				for (int i = NUM_NON_CORAL_VEGETATION_TYPES; i < (int)VegetationType::COUNT; ++i)
				{
					VegetationInfo& vegInfo = m_unsavedDef.m_vegetationInfos[i];
					VegetationConstants& vegConstants = vegInfo.m_vegConstants;
					std::string vegName = GetVegetationTypeNameFromType((VegetationType)i);

					if (vegName == UN_INITIALIZED_WORLD_DATA_NAME)
						continue;

					Strings strings = SplitStringOnDelimiter(vegName,' ');
					vegName = strings[1];
					for (int j = 2; j < (int)strings.size(); ++j)
					{
						vegName += " " + strings[j];
					}

					if (ImGui::TreeNode(Stringf("%s ##%i", vegName.c_str(), i).c_str()))
					{

						ImGui::Checkbox(vegName.c_str(), &vegInfo.m_isActive);
						ImGui::SliderFloat(Stringf("Thickness ##%i", i).c_str(), &vegInfo.m_thickness, 0.f, 1.f);
						ImGui::SliderFloat(Stringf("Dot Product World Up Threshold ##%i", i).c_str(), &vegInfo.m_worldDotUpThreshold, -1.f, 1.f);
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("How flat the terrain must be to spawn vegetation. 1 = terrain normal must point straight upwards, -1 = any terrain can spawn vegetation");
						}

						ImGui::Spacing();
						float scaleRange[2] = { vegConstants.m_widthRange.m_min , vegConstants.m_widthRange.m_max };
						if (ImGui::SliderFloat2(Stringf("Scale Range ##%i", i).c_str(), scaleRange, 0.1f, 5.f))
						{
							vegConstants.m_widthRange.m_min = scaleRange[0];
							vegConstants.m_widthRange.m_max = scaleRange[1];
							vegConstants.m_heightRange = vegConstants.m_widthRange;
						}

						ImGui::SliderFloat(Stringf("Emissive Strength ##%i").c_str(), &vegConstants.m_emissiveStrength, 0.f, 20.f);

						ImGui::TreePop();
					}
				}
			}

			ImGui::SeparatorText("Vegetation Noise");
			for (int i = 0; i < (int)VegetationNoiseType::COUNT; ++i)
			{
				VegetationNoiseInfo& noiseInfo = m_unsavedDef.m_vegetationNoiseInfos[i];
				std::string vegName = GetVegetationNoiseTypeNameFromType((VegetationNoiseType)i);

				if (vegName == UN_INITIALIZED_WORLD_DATA_NAME)
					continue;

				if (ImGui::CollapsingHeader(Stringf("%s Noise ##%i", vegName.c_str(), i).c_str()))
				{
					ImGui::InputInt(Stringf("Seed ##%i", i).c_str(), &noiseInfo.m_seed);
					ImGui::SliderFloat(Stringf("Scale ##%i", i).c_str(), &noiseInfo.m_scale, 5.f, 1000.f);
					ImGui::SliderInt(Stringf("Num Noise Octaves ##%i", i).c_str(), &noiseInfo.m_numOctaves, 1, 15);
					ImGui::SliderFloat(Stringf("Strength ##i", i).c_str(), &noiseInfo.m_strength, 0.f, 10.f);
					ImGui::SliderFloat(Stringf("Warp Strength ##i", i).c_str(), &noiseInfo.m_warpStrength, 0.f, 2000.f);
				}
			}
			

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Fish"))
		{
			ImGui::SeparatorText(Stringf("Fish (%i / %i)", GetTotalNumFish(), m_unsavedDef.m_maxNumFish).c_str());
			ImGui::TextWrapped("Use the controls below to customize the fish behavior");
			ImGui::Spacing();

			ImGui::Checkbox("Fish ##checkbox", &m_unsavedDef.m_activeFish);
            ImGui::SliderInt("Max Number", &m_unsavedDef.m_maxNumFish, 0, 2000);

			ImGui::SliderInt("Max Number Neighbors to Check Per Frame", &m_unsavedDef.m_maxNumFishNeighborsToCheck, 1, 100);

			for (int i = 0; i < (int)FishType::COUNT; ++i)
			{
				FishInfo& fishInfo = m_unsavedDef.m_fishInfos[i];
				if(fishInfo.m_type == FishType::UN_INITIALIZED)
					continue;

				std::string fishTypeName = GetFishTypeNameFromType((FishType)i);

				if (ImGui::TreeNode(Stringf("%s ##%i", fishTypeName.c_str(), i).c_str()))
				{
					int numFishOfType = m_totalNumFishByType[i];
					ImGui::Checkbox(Stringf("%s (%i) ##%i", fishTypeName.c_str(), numFishOfType, i).c_str(), &fishInfo.m_isActive);
					ImGui::SliderInt(Stringf("Max Number ##%i", i).c_str(), &fishInfo.m_maxNumber, 0, m_unsavedDef.m_maxNumFish);
					ImGui::SeparatorText("Swim and Spawn Behavior");
					ImGui::SliderInt(Stringf("Spawn Thickness ##%i", i).c_str(), &fishInfo.m_spawnThickness, 0, MAX_FISH_SPAWNED_IN_CHUNK);
					ImGui::SliderFloat(Stringf("Max Speed ##%i", i).c_str(), &fishInfo.m_maxSpeed, 0.f, 200.f);
					ImGui::SliderFloat(Stringf("Max Turn Speed ##%i", i).c_str(), &fishInfo.m_maxTurnSpeed, 0.f, 100.f);
					ImGui::SliderFloat(Stringf("Min Ocean Depth ##%i", i).c_str(), &fishInfo.m_minOceanDepth, 0.f, 200.f);
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("How close to the ocean surface a fish can get before altering direction");
					}


					ImGui::SeparatorText("Schooling Behavior");
					if (fishInfo.m_type == FishType::SHARK)
					{
						ImGui::Text("Sharks will not school together,\nbut will chase after other types of fish (Cohesion and Alignment)");
					}
					float flockingRadius = sqrtf(fishInfo.m_flockingRadiusSqrd);
					if (ImGui::SliderFloat(Stringf("Flocking Radius ##%i", i).c_str(), &flockingRadius, 0.f, 100.f))
					{
						fishInfo.m_flockingRadiusSqrd = flockingRadius * flockingRadius;
					}

					float separationRadius = sqrtf(fishInfo.m_separationRadiusSqrd);
					if(ImGui::SliderFloat(Stringf("Separation Radius ##%i", i).c_str(), &separationRadius, 0.f, 100.f))
					{
						fishInfo.m_separationRadiusSqrd = separationRadius * separationRadius;
					}
					ImGui::SliderFloat(Stringf("Cohesion ##%i", i).c_str(), &fishInfo.m_cohesion, 0.f, 1.f);
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("How much the fish will flock towards each other");
					}

					ImGui::SliderFloat(Stringf("Alignment ##%i", i).c_str(), &fishInfo.m_alignment, 0.f, 1.f);
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("How much fish will orient themselves to neighbors");
					}

					ImGui::SliderFloat(Stringf("Separation ##i", i).c_str(), &fishInfo.m_separation, 0.f, 1.f);
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("How much fish will avoid other fish");
					}

					ImGui::SeparatorText("Terrain Behavior");

					ImGui::SliderFloat(Stringf("Terrain Probe Distance ##%i", i).c_str(), &fishInfo.m_probeDistance, 0.f, 100.f);
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("How close to terrain the fish can come, before trying to avoid it");
					}

					ImGui::SliderFloat(Stringf("Terrain Avoidance ##%i", i).c_str(), &fishInfo.m_terrainAvoidance, 0.f, 1.f);
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("How strongly fish will avoid terrain");
					}
					
					ImGui::SeparatorText("Player Behavior");

					float playerFlockingRadius = sqrtf(fishInfo.m_playerFlockingRadiusSqrd);
					if (ImGui::SliderFloat(Stringf("Flocking Player Radius ##%i", i).c_str(), &playerFlockingRadius, 0.f, 1000.f))
					{
						fishInfo.m_playerFlockingRadiusSqrd = playerFlockingRadius * playerFlockingRadius;
					}
					float playerSeparationRadius = sqrtf(fishInfo.m_playerSeparationRadiusSqrd);
					if (ImGui::SliderFloat(Stringf("Player Separation Radius ##%i", i).c_str(), &playerSeparationRadius, 0.f, 100.f))
					{
						fishInfo.m_playerSeparationRadiusSqrd = playerSeparationRadius * playerSeparationRadius;
					}
					ImGui::SliderFloat(Stringf("Player Cohesion ##%i", i).c_str(), &fishInfo.m_playerCohesion, 0.f, 1.f);

					ImGui::SliderFloat(Stringf("Player Separation ##%i", i).c_str(), &fishInfo.m_playerSeparation, 0.f, 1.f);
					ImGui::TreePop();
				}
			}

			ImGui::EndTabItem();
		}

		if(ImGui::BeginTabItem("Debug Render"))
		{
			if (ImGui::Selectable("Default", g_debugShaderInt == 0))
				g_debugShaderInt = 0;

			if (ImGui::Selectable("Albedo", g_debugShaderInt == 1))
				g_debugShaderInt = 1;

			if (ImGui::Selectable("Normal Map", g_debugShaderInt == 2))
				g_debugShaderInt = 2;

			if (ImGui::Selectable("Ambient Occlusion", g_debugShaderInt == 3))
				g_debugShaderInt = 3;

			if (ImGui::Selectable("Light Strength", g_debugShaderInt == 4))
				g_debugShaderInt = 4;

			if (ImGui::Selectable("No Normal Map", g_debugShaderInt == 5))
				g_debugShaderInt = 5;

			if (ImGui::Selectable("No Ambient Occlusion", g_debugShaderInt == 6))
				g_debugShaderInt = 6;

			if (ImGui::Selectable("Specular Attenuation", g_debugShaderInt == 7))
				g_debugShaderInt = 7;

			if (ImGui::Selectable("No Specular Attenuation", g_debugShaderInt == 8))
				g_debugShaderInt = 8;

			if (ImGui::Selectable("Scene Depth", g_debugShaderInt == 9))
				g_debugShaderInt = 9;

			if (ImGui::Selectable("Emission", g_debugShaderInt == 10))
				g_debugShaderInt = 10;

			if (ImGui::Selectable("Biome Weight 1", g_debugShaderInt == 11))
				g_debugShaderInt = 11;

			if (ImGui::Selectable("Biome Weight 2", g_debugShaderInt == 12))
				g_debugShaderInt = 12;

			if (ImGui::Selectable("Biome Weight 3", g_debugShaderInt == 13))
				g_debugShaderInt = 13;

			if (ImGui::Selectable("Biome Weight 4", g_debugShaderInt == 14))
				g_debugShaderInt = 14;

			if (ImGui::Selectable("Strongest Biome", g_debugShaderInt == 15))
				g_debugShaderInt = 15;

			if (ImGui::Selectable("Fog Volume Masks", g_debugShaderInt == 16))
				g_debugShaderInt = 16;

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Masks for Light Rays, Particles, and Above Water Fog");
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Controls"))
		{
			ImGui::SeparatorText("Debug");
			ImGui::Text("[F1]                     Toggle Debug Messages\n");
			ImGui::Text("[F2]                     Chunk Debug Views");
			ImGui::Text("                             -Mesh Chunks\n                             -All Chunks\n                             -Wireframe\n                             -Fish Radii\n");
			ImGui::Text("[F3]                     Toggle Chunk Loading\n");
			ImGui::Text("[F5]                     Job and Memory Stats\n");
			ImGui::Text("[F8]                     Reload World\n");
			ImGui::Separator();
			ImGui::Text("[0-9]                    Render Modes 0 - 9\n");
			ImGui::Text("[SHIFT] + [0-9]          Render Modes 10 - 19\n");
			ImGui::Text("[ARROWS]                 Directional Light Direction\n");
			ImGui::Separator();


			ImGui::Text("[~]                      Dev Console\n");
			ImGui::Text("[TAB]                    Mouse Visibility\n");
			ImGui::Text("[P]                      Pause\n");
			ImGui::Text("[O]                      Step Single Frame\n");
			ImGui::Spacing();

			ImGui::SeparatorText("Movement");
			ImGui::Text("[WASD]                   Move\n");
			ImGui::Text("[E/F]                    Move Up and Down\n");
			ImGui::Text("[MOUSE]                  Look Around\n");
			ImGui::Text("[SHIFT]                  Sprint\n");
			ImGui::Text("[SHIFT]+ [MOUSE WHEEL]   Change Sprint Speed\n");
			ImGui::Spacing();

			ImGui::SeparatorText("Camera Modes");
			ImGui::Text("[C]                      Change Camera/Physics Mode");
			ImGui::Text("                             -Free Fly\n                             -No Clip\n                             -World Aligned\n                             -Frustum Cull\n");
			ImGui::Spacing();

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();

}

void World::ManageMapContainers()
{
	if (m_coordsChunkPair.size() < m_maxActiveChunks * 2)
	{
		m_coordsChunkPair.rehash(0);
	}

	if (m_pendingMeshBufferChunks.size() < m_pendingMeshBufferChunks.max_bucket_count() / 4)
	{
		m_pendingMeshBufferChunks.rehash(0);
	}

	if (m_pendingVegetationChunks.size() < m_pendingVegetationChunks.max_bucket_count() / 4)
	{
		m_pendingVegetationChunks.rehash(0);
	}
}

void World::IterateDebugView()
{
	int debugView = (int)m_debugView;
	debugView++;
	if (debugView >= (int)WorldDebugView::COUNT)
	{
		debugView = 0;
	}

	m_debugView = (WorldDebugView)debugView;
}

//Render
//----------------------------------------------------------------------------------------------------------------------------------------
void World::Render() const
{
	if(m_state != WorldState::ACTIVE || m_shouldRebuild || m_needsInitialization)
		return;

	g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::DEBUG_RENDER, DebugRenderConstants());

	FogConstants const& fogConst = m_unsavedDef.m_fogConstants;;
	CausticsConstants causticConst = m_unsavedDef.m_causticConstants;

	Mat44 clipToWorldTransform = g_game->m_player->m_camera->GetClipToWorldTransform();
	g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::CLIP_TO_WORLD, clipToWorldTransform);


	if(!m_unsavedDef.m_activeCaustics)
		causticConst.m_causticsIntensity = 0.f;



	g_renderer->SetLightConstants(*m_lightSettings);
	RenderSun();
	//---Debug Chunks---
	//---------------------------------------------------------------------------------------------------------------
	RenderChunkDebug();

	//---Terrain---
	//---------------------------------------------------------------------------------------------------------------
	g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::FOG, fogConst);
	g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::CAUSTICS, causticConst);
	g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::WATER, m_unsavedDef.m_waterConstants);

	RenderChunkTerrain();
	RenderTarget terrainRT = g_renderer->GetCopyOfCurrentRenderTarget();

	//---Vegetation Grass---
	//---------------------------------------------------------------------------------------------------------------
	RenderChunkVegetation();

	RenderTarget terrainAndVegetationRT = g_renderer->GetCopyOfCurrentRenderTarget();
	

	//---Fish---
	//---------------------------------------------------------------------------------------------------------------
	RenderFish();

	RenderTarget terrainVegAndFishRT = g_renderer->GetCopyOfCurrentRenderTarget();

	//---Water Surface---
	//---------------------------------------------------------------------------------------------------------------
	g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::FOG, fogConst);
	RenderWaterSurface(terrainVegAndFishRT, terrainRT);


	//---Light Rays---
	//---------------------------------------------------------------------------------------------------------------
	RenderTarget worldRT = g_renderer->GetCopyOfCurrentRenderTarget();
	RenderLightRayAndParticleMask(terrainVegAndFishRT);


	//---Fog---
	//---------------------------------------------------------------------------------------------------------------
	g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::FOG, fogConst);
	RenderTarget lightRayRT = g_renderer->GetCopyOfCurrentRenderTarget();
	RenderFog(worldRT, lightRayRT.m_color);


	
	FishInfo const& fishInfo = m_unsavedDef.m_fishInfos[(int)FishType::SHARK];
	g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::FISH, fishInfo.m_swimConstants);
	
}


void World::RenderSkyBox() const
{
	if(m_state != WorldState::ACTIVE)
		return;
	//SkyBox
	g_renderer->BeginRendererEvent("Draw - Skybox");
	g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::LIGHT_RAYS, m_unsavedDef.m_lightRayConstants);
	m_definition->m_skybox->Render();
	g_renderer->EndRendererEvent();
}

void World::RenderResourceLoading() const
{
	if(m_state != WorldState::LOADING_RESOURCES)
		return;

	AABB2 screenBounds = g_game->GetScreenBounds();
	AABB2 barBounds = screenBounds.GetBoxAtUVS(Vec2(0.3f, 0.05f), Vec2(0.7f, 0.1f));
	AABB2 loadingBounds = barBounds.GetBoxAtUVS(Vec2(0.f, 0.f), Vec2(GetLoadingPercent(), 1.f));

	Verts verts;
	AddVertsForAABB2D(verts, barBounds, Rgba8::DARK_GREY);
	AddVertsForAABB2D(verts, loadingBounds, Rgba8::GREEN);

	g_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_renderer->SetDepthMode(DepthMode::DISABLED);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->SetModelConstants();
	g_renderer->BindTexture(nullptr);
	g_renderer->BindShader(nullptr);

	g_renderer->DrawVertexArray(verts);

	AABB2 textBounds = screenBounds.GetBoxAtUVS(Vec2(0.3f, 0.1f), Vec2(0.7f, 0.15f));
	DebugAddScreenText("Loading Texture and Mesh Resources...", textBounds, 300.f, Vec2(0.f, 0.5f),0.f);
}

void World::RenderChunkDebug() const
{
	if (m_debugView == WorldDebugView::CHUNK_BOUNDS || m_debugView == WorldDebugView::MESH_CHUNK_BOUNDS)
	{
		g_renderer->BeginRendererEvent("Draw - Chunk Debug");
		for (int i = 0; i < (int)m_activeChunks.size(); ++i)
		{
			if (m_activeChunks[i])
			{
				m_activeChunks[i]->RenderDebug();
			}
		}

		g_renderer->EndRendererEvent();
	}

	else if (m_debugView == WorldDebugView::DEBUG_FISH)
	{
		
		g_renderer->BeginRendererEvent("Draw - Fish Debug");
		for (int i = 0; i < (int)m_activeChunks.size(); ++i)
		{
			if (m_activeChunks[i])
			{
				m_activeChunks[i]->RenderDebugFish(m_debugFishRadiusVBO);
			}
		}
		g_renderer->EndRendererEvent();
	}
}

void World::RenderChunkTerrain() const
{
	g_renderer->BeginRendererEvent("Draw - Chunk Terrain");

	g_renderer->SetConstantBufferData((uint32_t)(uint32_t)GAME_ROOT_PARAMETERS::TEXTURES, m_definition->m_textureConstants);

	g_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_renderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	if (m_debugView == WorldDebugView::WIRE_FRAME)
	{
		g_renderer->SetRasterizerMode(RasterizerMode::WIREFRAME_CULL_BACK);
	}

	g_renderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP);
	g_renderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP, 1);
	g_renderer->BindRootSignature(nullptr);
	g_renderer->BindShader(m_definition->m_terrainShader);
	g_renderer->SetModelConstants();

	for (int i = 0; i < (int)m_activeChunks.size(); ++i)
	{
		if (m_activeChunks[i])
		{
			m_activeChunks[i]->RenderTerrain();
		}
	}

	g_renderer->EndRendererEvent();
}

void World::RenderChunkVegetation() const
{
	if(!m_definition->m_activeVegetation || !m_unsavedDef.m_activeVegetation || m_debugView == WorldDebugView::WIRE_FRAME)
		return;

	g_renderer->BeginRendererEvent("Draw - Vegetation");

	//Render non - coral
	for (int i = 0; i < NUM_NON_CORAL_VEGETATION_TYPES; ++i)
	{
		VegetationType vegType = (VegetationType)i;
		VegetationInfo const& vegInfo = m_unsavedDef.m_vegetationInfos[i];
		if(!vegInfo.m_isActive || vegInfo.m_type == VegetationType::UN_INITIALIZED)
			continue;

		WindType windType = GetWindTypeFromVegetationType(vegType);
		WindConstants const& wind = m_unsavedDef.m_windConstants[(int)windType];
		g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::WIND, wind);

		VegetationConstants const& vegConstants = m_unsavedDef.m_vegetationInfos[i].m_vegConstants;
		g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::VEGETATION, vegConstants);

		std::string vegName = GetVegetationTypeNameFromType(vegType);
		g_renderer->BeginRendererEvent(Stringf("Draw - %s", vegName.c_str()).c_str());
		g_renderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);

		RasterizerMode rasterMode = IsVegetationTypeTwoSided(vegType) ? rasterMode = RasterizerMode::SOLID_CULL_NONE : RasterizerMode::SOLID_CULL_BACK;

		g_renderer->SetRasterizerMode(rasterMode);
		g_renderer->SetBlendMode(BlendMode::OPAQUE);
		g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);

		g_renderer->BindTexture(vegInfo.m_textures.m_diffuseTex);
		g_renderer->BindTexture(vegInfo.m_textures.m_normalTex,1);
		g_renderer->BindTexture(vegInfo.m_textures.m_aoTex,2);
		g_renderer->BindTexture(vegInfo.m_textures.m_specularTex,3);

		g_renderer->BindShader(vegInfo.m_graphicsShader);
		g_renderer->BindRootSignature(nullptr);
		g_renderer->SetModelConstants();

		for (int j = 0; j < (int)m_activeChunks.size(); ++j)
		{
			if (m_activeChunks[j])
			{
				m_activeChunks[j]->RenderVegetation(vegType);
			}
		}

		g_renderer->EndRendererEvent();
	}

	if(!m_definition->m_activeCoral || !m_unsavedDef.m_activeCoral)
		return;

	RenderTarget sceneBeforeCoralRT = g_renderer->GetCopyOfCurrentRenderTarget();

	//Render coral Color and Mask
	for (int i = NUM_NON_CORAL_VEGETATION_TYPES; i < (int)VegetationType::COUNT; ++i)
	{
		VegetationType vegType = (VegetationType)i;
		VegetationInfo const& vegInfo = m_unsavedDef.m_vegetationInfos[i];
		if (!vegInfo.m_isActive || vegInfo.m_type == VegetationType::UN_INITIALIZED)
			continue;

		VegetationConstants const& vegConstants = m_unsavedDef.m_vegetationInfos[i].m_vegConstants;
		g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::VEGETATION, vegConstants);

		std::string vegName = GetVegetationTypeNameFromType(vegType);
		g_renderer->BeginRendererEvent(Stringf("Draw - %s", vegName.c_str()).c_str());
		g_renderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);

		RasterizerMode rasterMode = IsVegetationTypeTwoSided(vegType) ? RasterizerMode::SOLID_CULL_NONE : RasterizerMode::SOLID_CULL_BACK;

		g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::CORAL_CONSTANTS, m_unsavedDef.m_coralBlendConstants);

		g_renderer->SetRasterizerMode(rasterMode);
		g_renderer->SetBlendMode(BlendMode::ALPHA);
		g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);

		g_renderer->BindTexture(vegInfo.m_textures.m_diffuseTex);
		g_renderer->BindTexture(vegInfo.m_textures.m_normalTex, 1);
		g_renderer->BindTexture(vegInfo.m_textures.m_aoTex, 2);
		g_renderer->BindTexture(vegInfo.m_textures.m_specularTex, 3);
		RenderTarget coralRT = g_renderer->GetCopyOfCurrentRenderTarget();
		g_renderer->BindTexture(coralRT.m_depth, 4);

		g_renderer->BindShader(vegInfo.m_graphicsShader);
		g_renderer->BindRootSignature(nullptr);
		g_renderer->SetModelConstants();

		for (int j = 0; j < (int)m_activeChunks.size(); ++j)
		{
			if (m_activeChunks[j])
			{
				m_activeChunks[j]->RenderVegetation(vegType);
			}
		}

		g_renderer->EndRendererEvent();
	}


	g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::LIGHT_RAYS, m_unsavedDef.m_lightRayConstants);
	RenderTarget sceneRT = g_renderer->GetCopyOfCurrentRenderTarget();
	g_renderer->BeginRendererEvent("Draw - Coral Blend Post Process");
	g_renderer->BindTexture(sceneRT.m_mask, 2);
	g_renderer->BindTexture(sceneBeforeCoralRT.m_color, 3);
	g_renderer->DrawFullScreenQuad(sceneRT.m_color, sceneRT.m_depth, m_definition->m_coralBlendPostProcessShader);

	g_renderer->EndRendererEvent();


	g_renderer->EndRendererEvent();
}


void World::RenderFish() const
{
	if (!m_unsavedDef.m_activeFish)
		return;

	g_renderer->BeginRendererEvent("Draw - Fish");
	g_renderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP,1);

	for (int i = 0; i < (int)FishType::COUNT; ++i)
	{
		FishInfo const& fishInfo = m_unsavedDef.m_fishInfos[i];

		g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::FISH, fishInfo.m_swimConstants);

		g_renderer->BindTexture(fishInfo.m_textures.m_diffuseTex, 0);
		g_renderer->BindTexture(fishInfo.m_textures.m_normalTex, 1);
		g_renderer->BindShader(fishInfo.m_shader);

		StructuredBufferDX12* buffer = m_fishBufferInfos[i].m_fishDataBuffer;
		unsigned int instanceCount = m_fishBufferInfos[i].m_instanceCount;
		if (instanceCount <= 0 || !buffer)
			continue;

		g_renderer->BindStructuredBuffer(buffer, 0);
		g_renderer->DrawInstancedIndexedVertexBufer(fishInfo.m_vbo, fishInfo.m_ibo, fishInfo.m_ibo->GetNumIndexes(), instanceCount);
	}

	g_renderer->EndRendererEvent();
}

void World::RenderSun() const
{

	g_renderer->BeginRendererEvent("Draw - Sun");
	RenderTarget sceneRT = g_renderer->GetCopyOfCurrentRenderTarget();
	g_renderer->ClearScreen(Rgba8::BLACK);

	Vec3 sunPosition = g_game->m_player->m_position + (-m_lightSettings->m_sunDirection * 25.f);
	Mat44 sunTransform = Mat44::MakeTranslation3D(sunPosition);

	Rgba8 sunColor = Rgba8::WHITE;
	g_renderer->SetModelConstants(sunTransform, sunColor);
	g_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->SetDepthMode(DepthMode::DISABLED);
	g_renderer->BindShader(nullptr);
	g_renderer->BindTexture(nullptr);

	g_renderer->DrawIndexedVertexBuffer(m_sunVBO, m_sunIBO, m_sunIBO->GetNumIndexes());

	RenderTarget sunRT = g_renderer->GetCopyOfCurrentRenderTarget();

	ShaderDX12 const* blurShader = g_renderer->CreateOrGetShaderFromFile("Data/Shaders/SunBlur");
	g_renderer->DrawFullScreenQuad(sceneRT.m_color, sunRT.m_color, blurShader);



	g_renderer->EndRendererEvent();

}

void World::RenderWaterSurface( RenderTarget const& worldRT, RenderTarget const& terrainRT) const
{
	g_renderer->BeginRendererEvent("Draw - Water Surface");
	g_renderer->BindTexture(worldRT.m_color);
	g_renderer->BindTexture(worldRT.m_depth, 1);
	g_renderer->BindTexture(terrainRT.m_color, 2);
	g_renderer->BindTexture(terrainRT.m_depth, 3);
	g_renderer->BindTexture(terrainRT.m_mask, 4);
	g_renderer->BindTexture(m_definition->m_waterNormalTexture, 5);
	g_renderer->BindTexture(m_definition->m_waterFoamTexture, 6);

	Skybox const* skybox = m_definition->m_skybox;
	int textureIndexOffset = 7;
	for (int i = 0; i < 6; ++i)
	{
		g_renderer->BindTexture(skybox->m_textures[i], textureIndexOffset + i);
	}

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	if (m_debugView == WorldDebugView::WIRE_FRAME)
	{
		g_renderer->SetRasterizerMode(RasterizerMode::WIREFRAME_CULL_NONE);
	}

	g_renderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP);
	g_renderer->BindRootSignature(nullptr);
	g_renderer->BindShader(m_definition->m_waterSurfaceShader);

	if (m_unsavedDef.m_activeWaterSurface && (m_debugView != WorldDebugView::CHUNK_BOUNDS && m_debugView != WorldDebugView::MESH_CHUNK_BOUNDS))
	{

		for (int i = 0; i < (int)m_activeChunks.size(); ++i)
		{
			if (m_activeChunks[i] && m_activeChunks[i]->m_isSeaSurfaceChunk)
			{
				m_activeChunks[i]->RenderWaterSurface();
			}
		}

	}
	g_renderer->EndRendererEvent();
}

void World::DispatchFogVolumeCompute() const
{
	if(!m_unsavedDef.m_activeLightRays && !m_unsavedDef.m_activeParticles)
		return;

	g_renderer->TransitionResource(*m_fogVolumeTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_COMMAND_LIST_TYPE_DIRECT);

	g_renderer->BindComputeShader(m_definition->m_lightRaysVolumeComputeShader);

	g_renderer->SetComputeUAV((uint32_t)ComputeRootParameters::UAV, 0, m_fogVolumeTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, nullptr, D3D12_COMMAND_LIST_TYPE_DIRECT);

	FogVolumeConstants fogVolumeConstants;
	fogVolumeConstants.m_clipToWorldTransform = g_game->m_player->m_camera->GetClipToWorldTransform();
	fogVolumeConstants.m_cameraPosition = g_game->m_player->m_camera->GetPosition();
	fogVolumeConstants.m_seaLevel = m_definition->GetSeaLevel();
	fogVolumeConstants.m_fogDepth = m_unsavedDef.m_fogConstants.m_fogData[UNDERWATER_FOG_INDEX].m_depth;
	fogVolumeConstants.m_sunDirection = m_unsavedDef.m_sunInfo.m_orientation.Get_IFwd();
	fogVolumeConstants.m_time = g_game->m_gameClock->GetTotalSeconds();

	g_renderer->SetComputeConstantBufferData((uint32_t)ComputeRootParameters::CBV, fogVolumeConstants, D3D12_COMMAND_LIST_TYPE_DIRECT);
	g_renderer->SetComputeConstantBufferData((uint32_t)ComputeRootParameters::CBV_1, m_unsavedDef.m_lightRayConstants, D3D12_COMMAND_LIST_TYPE_DIRECT);
	g_renderer->SetComputeConstantBufferData((uint32_t)ComputeRootParameters::CBV_2, m_unsavedDef.m_particleConstants, D3D12_COMMAND_LIST_TYPE_DIRECT);
	g_renderer->Dispatch((FOG_VOLUME_TEX_DIMS_X + 7) / 8, (FOG_VOLUME_TEX_DIMS_Y + 7) / 8, (FOG_VOLUME_TEX_DEPTH + 3) / 4, D3D12_COMMAND_LIST_TYPE_DIRECT);
	g_renderer->TransitionResource(*m_fogVolumeTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_COMMAND_LIST_TYPE_DIRECT);
}

void World::RenderLightRayAndParticleMask(RenderTarget const& worldRT) const
{
	if (!m_unsavedDef.m_activeLightRays && !m_unsavedDef.m_activeParticles)
		return;

	g_renderer->BeginRendererEvent("Draw - Light Ray and Particle Mask");
	g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::LIGHT_RAYS, m_unsavedDef.m_lightRayConstants);
	g_renderer->SetConstantBufferData((uint32_t)GAME_ROOT_PARAMETERS::PARTICLES, m_unsavedDef.m_particleConstants);
	g_renderer->BindTexture(m_fogVolumeTexture, 2);

	g_renderer->DrawFullScreenQuad(worldRT.m_color, worldRT.m_depth, m_unsavedDef.m_lightRaysMaskShader);
	g_renderer->EndRendererEvent();
}

void World::RenderFog(RenderTarget const& worldRT, TextureDX12 const* lightRaysMask) const
{
	//Screen-Space Fog
	//-----------------------------------------------------------------------------------------------------------------------
	g_renderer->BeginRendererEvent("Draw - ScreenSpace Fog");
	g_renderer->SetConstantBufferData((uint32_t)RootParameters::CAMERA_CONSTANTS, g_game->m_player->GetCameraConstants());	

	ShaderDX12* postProcessShader = m_unsavedDef.m_activeFog && g_game->m_player->GetCameraMode() != CameraMode::FRUSTUM_CULL ? m_definition->m_fogShader : nullptr;

	g_renderer->BindTexture(lightRaysMask, 2);
	g_renderer->DrawFullScreenQuad(worldRT.m_color, worldRT.m_depth, postProcessShader);

	g_renderer->EndRendererEvent();
}


void World::BuildFishRenderData()
{
	if(!m_unsavedDef.m_activeFish)
		return;

	std::vector<std::vector<FishInstanceData>> fishDataGroups;
	fishDataGroups.resize((int)FishType::COUNT);

	for (int i = 0; i < (int)FishType::COUNT; ++i)
	{
		m_fishBufferInfos[i].m_instanceCount = 0;
		fishDataGroups[i].reserve(m_totalNumFishByType[i]);
	}

	for (int i = 0; i < (int)m_activeChunks.size(); ++i)
	{
		if (m_activeChunks[i])
		{
			m_activeChunks[i]->GatherFishRenderData(fishDataGroups);
		}
	}


	const unsigned int maxInstanceCount = (unsigned int)m_definition->m_maxNumFish;
	for (int i = 0; i < (int)FishType::COUNT; ++i)
	{
		if (fishDataGroups[i].size() <= 0)
			continue;

		FishBufferInfo& bufferInfo = m_fishBufferInfos[i];
		std::vector<FishInstanceData>& fishDataGroup = fishDataGroups[i];
		if (fishDataGroup.empty())
		{
			bufferInfo.m_instanceCount = 0;
			continue;
		}

		bufferInfo.m_instanceCount = (unsigned int)(fishDataGroup.size());
		if (bufferInfo.m_instanceCount > maxInstanceCount)
		{
			bufferInfo.m_instanceCount = maxInstanceCount;
		}

		if ((bufferInfo.m_fishDataBuffer == nullptr))
		{
			continue;
		}

		g_renderer->UploadDataIntoStructuredBuffer(bufferInfo.m_fishDataBuffer, fishDataGroup);
	}

}


//Input
//----------------------------------------------------------------------------------------------------------------------------------------
void World::CheckKeyboardControls()
{
	ImGuiIO const& imguiIO = ImGui::GetIO();
	if (imguiIO.WantCaptureMouse)
		return;

	if (g_inputSystem->WasKeyJustPressed(KEYCODE_F5))
	{
		m_showMemoryInfo = !m_showMemoryInfo;
		m_showJobInfo = !m_showJobInfo;
	}

	if(g_game->m_gameState != GameState::GAMEPLAY)
		return;

	if (g_inputSystem->WasKeyJustPressed(KEYCODE_F8))
	{
		LoadChangedWorld();
	}

	if (g_inputSystem->WasKeyJustPressed(KEYCODE_F2))
	{
		IterateDebugView();
	}

	if (g_inputSystem->WasKeyJustPressed(KEYCODE_F3))
	{
		m_loadChunks = !m_loadChunks;
	}

	if (g_inputSystem->WasKeyJustPressed(KEYCODE_RIGHTARROW))
	{
		m_unsavedDef.m_sunInfo.m_orientation.m_yawDegrees += 30.f;
		m_renderDebugSunDirection = true;
		m_debugSunTimer->Restart();
	}

	if (g_inputSystem->WasKeyJustPressed(KEYCODE_LEFTARROW))
	{
		m_unsavedDef.m_sunInfo.m_orientation.m_yawDegrees -= 30.f;
		m_renderDebugSunDirection = true;
		m_debugSunTimer->Restart();
	}

	if (g_inputSystem->WasKeyJustPressed(KEYCODE_UPARROW))
	{
		m_unsavedDef.m_sunInfo.m_orientation.m_pitchDegrees -= 30.f;
		m_renderDebugSunDirection = true;
		m_debugSunTimer->Restart();
	}

	if (g_inputSystem->WasKeyJustPressed(KEYCODE_DOWNARROW))
	{
		m_unsavedDef.m_sunInfo.m_orientation.m_pitchDegrees += 30.f;
		m_renderDebugSunDirection = true;
		m_debugSunTimer->Restart();
	}

	Vec3 sunDir = m_unsavedDef.m_sunInfo.m_orientation.Get_IFwd();
	m_lightSettings->m_sunDirection = sunDir;
}

void World::CheckControllerControls()
{
	XboxController controller = g_inputSystem->GetController(0);
	if (controller.WasButtonJustPressed(XboxButtonID::BUTTON_B))
	{
		LoadChangedWorld();
	}
}

//Chunk Loading
//----------------------------------------------------------------------------------------------------------------------------------------
void World::AddNewChunkAtCoords(IntVec3 const& coords)
{
	Chunk* newChunk = new Chunk(this, coords);
	auto found = m_coordsChunkPair.find(coords);
	if (found != m_coordsChunkPair.end())
	{
		ERROR_RECOVERABLE("Adding chunk at already registered coords");
		found->second = newChunk;
		ExecuteChunkGenerateJob(newChunk);
		return;
	}

	m_coordsChunkPair.insert({ coords, newChunk });
	ExecuteChunkGenerateJob(newChunk);
}

void World::LoadChunksInActivationRange()
{
	const IntVec3 playerChunk = GetChunkCoordsFromPosition(g_game->m_player->m_position);
	const Vec3 playerCenter = GetGlobalCenterPosFromChunkCoords(playerChunk);
	const Frustum frustum = g_game->m_player->GetViewFrustrum();

	// Always guarantee current chunk exists
	if (!DoesChunkExist(playerChunk) && m_chunkZRange.IsOnRange(playerChunk.z))
	{
		AddNewChunkAtCoords(playerChunk);
	}

	constexpr float ACTIVATION_SQ = (float)CHUNK_ACTIVATION_RANGE * (float)CHUNK_ACTIVATION_RANGE;
	constexpr int MAX_CHUNKS_PER_FRAME = MAX_NUM_WORLD_JOBS / 4;
	constexpr float INV_ACTIVATION_SQ = 1.f / ((float)CHUNK_ACTIVATION_RANGE * (float)CHUNK_ACTIVATION_RANGE);


	static IntVec3 lastCenterChunk = playerChunk;
	if (playerChunk != lastCenterChunk)
	{
		m_checkedCoords.clear();
		while (!m_chunkLoadQueue.empty()) { m_chunkLoadQueue.pop(); }
		lastCenterChunk = playerChunk;
		m_chunkLoadQueue.push({ playerChunk, 0.0f });
		m_checkedCoords.insert(playerChunk);
	}
	else if (m_checkedCoords.empty())
	{
		m_chunkLoadQueue.push({ playerChunk, 0.0f });
		m_checkedCoords.insert(playerChunk);
	}

	int chunksAddedThisFrame = 0;

	double frameStart = GetCurrentTimeSeconds();
	constexpr double LOAD_TIME_BUDGET_MS = 0.025;

	constexpr int MAX_EXPLORED_PER_FRAME = 256;
	int exploredThisFrame = 0;

	while (!m_chunkLoadQueue.empty() &&
		m_activeJobs.size() < MAX_NUM_WORLD_JOBS &&
		chunksAddedThisFrame < MAX_CHUNKS_PER_FRAME)
	{
		ChunkLoadCandidate current = m_chunkLoadQueue.top();
		m_chunkLoadQueue.pop();
		const IntVec3 cur = current.coords;

		for (const IntVec3& dir : IntVec3::CARDINAL_DIRECTIONS)
		{
			IntVec3 next = cur + dir;
			if (!m_chunkZRange.IsOnRange(next.z))
				continue;
			if (!m_checkedCoords.insert(next).second)
				continue;

			Vec3 nextCenter = GetGlobalCenterPosFromChunkCoords(next);
			float distSq = GetDistanceSquared3D(nextCenter, playerCenter);
			if (distSq > ACTIVATION_SQ)
				continue;

			// cheap frustum test (approx front half-space)
			Vec3 toChunk = (nextCenter - playerCenter).GetNormalized();
			bool inFrustum = (DotProduct3D(toChunk, g_game->m_player->GetForwardNormal()) > 0.25f);

			float distanceScore = GetClamped((distSq * INV_ACTIVATION_SQ), 0.f, 1.f);
			float frustumBoost = inFrustum ? (-150.0f * (1.0f - distanceScore)) : 0.0f;
			float totalScore = distanceScore + frustumBoost;

			if (m_chunkLoadQueue.size() < 4096)
				m_chunkLoadQueue.push({ next, totalScore });

			if (!DoesChunkExist(next))
			{
				AddNewChunkAtCoords(next);
				chunksAddedThisFrame++;
				if (chunksAddedThisFrame >= MAX_CHUNKS_PER_FRAME)
					return;
			}

			exploredThisFrame++;
			if (exploredThisFrame >= MAX_EXPLORED_PER_FRAME)
				break;
		}

		double now = GetCurrentTimeSeconds();
		double elapsedMS = now - frameStart;
		if(elapsedMS > LOAD_TIME_BUDGET_MS)
			return;
	}
}

void World::UnloadChunksOutsideActivationRange()
{
	constexpr int DEACTIVATION_RANGE_SQRD = CHUNK_DEACTIVATION_RANGE * CHUNK_DEACTIVATION_RANGE;
	Vec3 const PLAYER_POS = g_game->m_player->m_position;
	int numDeleted = 0;
	for (int i = 0; i < (int)m_activeChunks.size(); ++i)
	{
		
		if(numDeleted >= MAX_NUM_CHUNKS_DELETE_PER_FRAME)
			return;
		

		if (m_activeChunks[i])
		{
			if (GetDistanceSquared3D(m_activeChunks[i]->GetCenterPos(), PLAYER_POS) > DEACTIVATION_RANGE_SQRD)
			{
				DeleteChunk(m_activeChunks[i]);
				m_activeChunks[i] = nullptr;
				numDeleted++;
			}
		}
	}
}

void World::ActivateChunk(Chunk* chunk)
{
	for (int i = 0; i < (int)m_activeChunks.size(); ++i)
	{
		if (m_activeChunks[i] == nullptr)
		{
			m_activeChunks[i] = chunk;
			chunk->m_state = ChunkState::ACTIVE;
			chunk->InitNeighbors();
			if (chunk->IsPendingVegetation())
			{
				QueueChunkForVegetation(chunk, chunk->m_chunkCoords);
			}

			if (m_numFishLoaded < m_unsavedDef.m_maxNumFish)
			{
				chunk->TrySpawnFish();
			}
			return;
		}
	}

	m_activeChunks.push_back(chunk);
	chunk->m_state = ChunkState::ACTIVE;
	chunk->InitNeighbors();
	if (chunk->IsPendingVegetation())
	{
		QueueChunkForVegetation(chunk, chunk->m_chunkCoords);
	}
	if (m_numFishLoaded < m_unsavedDef.m_maxNumFish)
	{
		chunk->TrySpawnFish();
	}
}

void World::DeleteChunk(Chunk* chunk)
{
	if (!m_shouldRebuild)
	{
		chunk->ClearNeighbors();
	}
	m_pendingMeshBufferChunks.erase(chunk->m_chunkCoords);
	m_pendingVegetationChunks.erase(chunk->m_chunkCoords);
	m_coordsChunkPair.erase(chunk->m_chunkCoords);
	chunk->m_state = ChunkState::GARBAGE;
	delete chunk;
	chunk = nullptr;
}

//Chunk Buffers
//----------------------------------------------------------------------------------------------------------------------------------------
void World::BuildChunkMeshBuffers()
{
	if(m_pendingMeshBufferChunks.empty())
		return;

	IntVec3 startingCoords = GetChunkCoordsFromPosition(g_game->m_player->m_position);
	Vec3 startingPos = GetGlobalCenterPosFromChunkCoords(startingCoords);

	constexpr float ACTIVATION_SQRD = (float)CHUNK_ACTIVATION_RANGE * CHUNK_ACTIVATION_RANGE;
	std::queue<IntVec3>chunkCoordsQueue;
	chunkCoordsQueue.push(startingCoords);
	std::unordered_set<IntVec3> checkedCoordsSet;
	checkedCoordsSet.reserve(m_maxActiveChunks);
	IntVec3 currentCoords = startingCoords;

	std::vector<IntVec3> builtCoords;
	builtCoords.reserve(NUM_MESHBUFFER_BUILDS_PER_FRAME);

	int numMeshBuilds = 0;
	while (!chunkCoordsQueue.empty() && numMeshBuilds < NUM_MESHBUFFER_BUILDS_PER_FRAME)
	{
		currentCoords = chunkCoordsQueue.front();
		auto found = m_pendingMeshBufferChunks.find(currentCoords);
		if (found != m_pendingMeshBufferChunks.end())
		{
			Chunk* chunk = found->second;
			if (chunk && chunk->m_state == ChunkState::PENDING_MESH_BUFFERS)
			{
				double time = GetCurrentTimeSeconds();
				if (chunk->TryGenerateMeshBuffers())
				{
					numMeshBuilds++;
					double speed = (GetCurrentTimeSeconds() - time) * 1000.0;
					m_totalMeshBufferSpeed += speed;
					m_numMeshBufferGenerates++;
					m_averageMeshBufferSpeed = m_totalMeshBufferSpeed / m_numMeshBufferGenerates;
				}
				builtCoords.push_back(currentCoords);

				ActivateChunk(chunk);
			}
		}

		for (int i = 0; i < IntVec3::NUM_CARDINAL_DIRECTIONS; ++i)
		{
			IntVec3 direction = IntVec3::CARDINAL_DIRECTIONS[i];

			//For checked coords outside of z range, force looking in the direction to get back in z range
			//-----------------------------------------------------------------------
			
			if (currentCoords.z >= m_chunkZRange.m_max)
			{
				if (currentCoords.z > m_chunkZRange.m_max && direction != IntVec3::DOWN)
					continue;

				else if (direction == IntVec3::UP)
					continue;
			}

			if (currentCoords.z <= m_chunkZRange.m_min)
			{
				if (currentCoords.z < m_chunkZRange.m_min && direction != IntVec3::UP)
					continue;

				else if (direction == IntVec3::DOWN)
					continue;
			}
			//-----------------------------------------------------------------------
			


			//If coords have been checked or are out of the activation range, do not keep searching in that direction
			//-----------------------------------------------------------------------
			IntVec3 coordsToCheck = currentCoords + IntVec3::CARDINAL_DIRECTIONS[i];

			if (checkedCoordsSet.find(coordsToCheck) != checkedCoordsSet.end()) //coords have already been checked
				continue;
				
				
			Vec3 centerPosToCheck = GetGlobalCenterPosFromChunkCoords(coordsToCheck);
			if (GetDistanceSquared3D(centerPosToCheck, startingPos) > ACTIVATION_SQRD)
				continue;
				

			chunkCoordsQueue.push(coordsToCheck);
			checkedCoordsSet.insert(coordsToCheck);
		}

		chunkCoordsQueue.pop();
	}

	for (int i = 0; i < (int)builtCoords.size(); ++i)
	{
		m_pendingMeshBufferChunks.erase(builtCoords[i]);
	}
}

void World::BuildChunkVegetation()
{

	if (m_pendingVegetationChunks.empty())
		return;

	int numBuffersToMakePerChunk = 0;
	for (int i = 0; i < (int)VegetationType::COUNT; ++i)
	{
		VegetationInfo const& vegInfo = m_definition->m_vegetationInfos[i];
		if (vegInfo.m_isActive)
		{
			numBuffersToMakePerChunk += 2;
		}
	}

	if (!g_renderer->CanMakeStructuredBuffers(numBuffersToMakePerChunk * NUM_VEGETATION_BUILDS_PER_FRAME))
		return;

	IntVec3 startingCoords = GetChunkCoordsFromPosition(g_game->m_player->m_position);
	Vec3 startingPos = GetGlobalCenterPosFromChunkCoords(startingCoords);

	constexpr float ACTIVATION_SQRD = (float)CHUNK_ACTIVATION_RANGE * CHUNK_ACTIVATION_RANGE;
	std::queue<IntVec3>chunkCoordsQueue;
	chunkCoordsQueue.push(startingCoords);
	std::unordered_set<IntVec3> checkedCoordsSet;
	checkedCoordsSet.reserve(m_maxActiveChunks);
	IntVec3 currentCoords = startingCoords;

	std::vector<IntVec3> builtCoords;
	builtCoords.reserve(NUM_VEGETATION_BUILDS_PER_FRAME);

	int numVegetationBuilds = 0;
	while (!chunkCoordsQueue.empty() && numVegetationBuilds < NUM_VEGETATION_BUILDS_PER_FRAME)
	{
		currentCoords = chunkCoordsQueue.front();
		auto found = m_pendingVegetationChunks.find(currentCoords);
		if (found != m_pendingVegetationChunks.end())
		{
			Chunk* chunk = found->second;
			if (chunk && chunk->IsPendingVegetation())
			{
				if (chunk->TryGenerateVegetationBuffers())
				{
					numVegetationBuilds++;
				}

				builtCoords.push_back(currentCoords);
			}
		}

		for (int i = 0; i < IntVec3::NUM_CARDINAL_DIRECTIONS; ++i)
		{
			IntVec3 direction = IntVec3::CARDINAL_DIRECTIONS[i];

			//For checked coords outside of z range, force looking in the direction to get back in z range
			//-----------------------------------------------------------------------

			if (currentCoords.z >= m_chunkZRange.m_max)
			{
				if (currentCoords.z > m_chunkZRange.m_max && direction != IntVec3::DOWN)
					continue;

				else if (direction == IntVec3::UP)
					continue;
			}

			if (currentCoords.z <= m_chunkZRange.m_min)
			{
				if (currentCoords.z < m_chunkZRange.m_min && direction != IntVec3::UP)
					continue;

				else if (direction == IntVec3::DOWN)
					continue;
			}
			//-----------------------------------------------------------------------



			//If coords have been checked or are out of the activation range, do not keep searching in that direction
			//-----------------------------------------------------------------------
			IntVec3 coordsToCheck = currentCoords + IntVec3::CARDINAL_DIRECTIONS[i];

			if (checkedCoordsSet.find(coordsToCheck) != checkedCoordsSet.end()) //coords have already been checked
				continue;


			Vec3 centerPosToCheck = GetGlobalCenterPosFromChunkCoords(coordsToCheck);
			if (GetDistanceSquared3D(centerPosToCheck, startingPos) > ACTIVATION_SQRD)
				continue;


			chunkCoordsQueue.push(coordsToCheck);
			checkedCoordsSet.insert(coordsToCheck);
		}

		chunkCoordsQueue.pop();
	}

	for (int i = 0; i < (int)builtCoords.size(); ++i)
	{
		m_pendingVegetationChunks.erase(builtCoords[i]);
	}
}

//Chunk Helpers
//----------------------------------------------------------------------------------------------------------------------------------------
IntVec3 World::GetChunkCoordsFromPosition(Vec3 const& pos) const
{
	return IntVec3(
		(int)floorf(pos.x * m_invChunkSize.x),
		(int)floorf(pos.y * m_invChunkSize.y),
		(int)floorf(pos.z * m_invChunkSize.z)
	);
}

Vec3 World::GetGlobalCenterPosFromChunkCoords(IntVec3 const& chunkCoords) const
{
	float sizeX = (float)m_chunkSize.x;
	float sizeY = (float)m_chunkSize.y;
	float sizeZ = (float)m_chunkSize.z;

	float minX = ((float)chunkCoords.x * sizeX);
	float minY = ((float)chunkCoords.y * sizeY);
	float minZ = ((float)chunkCoords.z * sizeZ);

	return Vec3(
		(minX + (sizeX * 0.5f)),
		(minY + (sizeY * 0.5f)),
		(minZ + (sizeZ * 0.5f))
	);
}

Chunk* World::GetChunkFromChunkCoords(IntVec3 const& chunkCoords) const
{
	auto found = m_coordsChunkPair.find(chunkCoords);
	if (found != m_coordsChunkPair.end())
		return found->second;

	return nullptr;
}

Chunk* World::GetActiveChunkFromPosition(Vec3 const& pos) const
{
	IntVec3 chunkCoords = GetChunkCoordsFromPosition(pos);
	Chunk* chunk = GetChunkFromChunkCoords(chunkCoords);
	if(chunk && chunk->m_state == ChunkState::ACTIVE)
		return chunk;

	return nullptr;
}

void World::QueueChunkForVegetation(Chunk* chunk, IntVec3 const& chunkCoords)
{
	m_pendingVegetationChunks.insert({chunkCoords, chunk});
}

uint32_t World::GetFishID()
{
	return m_nextFishID.fetch_add(1, std::memory_order_relaxed);
}

bool World::DoesChunkExist(IntVec3 const& chunkCoords) const
{
	auto found = m_coordsChunkPair.find(chunkCoords);
	if (found != m_coordsChunkPair.end() && found->second != nullptr)
	{
		return true;
	}

	return false;
}

bool World::DoesChunkExist(IntVec3 const& chunkCoords, Chunk*& out_foundChunk) const
{
	auto found = m_coordsChunkPair.find(chunkCoords);
	if (found != m_coordsChunkPair.end() && found->second != nullptr)
	{
		out_foundChunk = found->second;
		return true;
	}

	out_foundChunk = nullptr;
	return false;
}

bool World::AreChunkCoordsInViewFrustum(IntVec3 const& chunkCoords, Frustum const& frustum) const
{
	Vec3 mins((chunkCoords.x * m_chunkSize.x), (chunkCoords.y * m_chunkSize.y), (chunkCoords.z * m_chunkSize.z));
	Vec3 maxs((int)mins.x + m_chunkSize.x, (int)mins.y + m_chunkSize.y, (int)mins.z + m_chunkSize.z);

	AABB3 chunkBounds = AABB3(mins, maxs);
	return IsAABB3inViewFrustum(chunkBounds, frustum);
}

//Density and Terrain Helpers
//----------------------------------------------------------------------------------------------------------------------------------------
float World::GetDensityAtPosition(Vec3 const& pos) const
{
	IntVec3 chunkCoords = GetChunkCoordsFromPosition(pos);
	Chunk* chunk = nullptr;
	if (!DoesChunkExist(chunkCoords, chunk))
	{
		return m_definition->GetCumulativeDensityAtPosition(pos);
	}

	if (chunk)
	{
		return chunk->GetDensityAtPosition(pos);
	}

	return m_definition->GetCumulativeDensityAtPosition(pos);
}

Vec3 World::GetNormalAtPosition(Vec3 const& pos) const
{
	IntVec3 chunkCoords = GetChunkCoordsFromPosition(pos);
	Chunk* chunk = nullptr;
	if (!DoesChunkExist(chunkCoords, chunk))
	{
		return GetBaseTerrainNormalAtPosition(pos);
	}

	if (chunk)
	{
		return chunk->GetNormalAtPosition(pos);
	}

	return GetBaseTerrainNormalAtPosition(pos);
}

Vec3 World::GetBaseTerrainNormalAtPosition(Vec3 const& pos, float epsilon) const
{
	Vec3 negativePos(pos - Vec3(epsilon, 0.f, 0.f));
	Vec3 positivePos(pos + Vec3(epsilon, 0.f, 0.f));
	float negativeDensity = GetDensityAtPosition(negativePos);
	float positiveDensity = GetDensityAtPosition(positivePos);
	float dx = negativeDensity - positiveDensity;

	negativePos = pos - Vec3(0.f, epsilon, 0.f);
	positivePos = pos + Vec3(0.f, epsilon, 0.f);
	negativeDensity = GetDensityAtPosition(negativePos);
	positiveDensity = GetDensityAtPosition(positivePos);
	float dy = negativeDensity - positiveDensity;

	negativePos = pos - Vec3(0.f, 0.f, epsilon);
	positivePos = pos + Vec3(0.f, 0.f, epsilon);
	negativeDensity = GetDensityAtPosition(negativePos);
	positiveDensity = GetDensityAtPosition(positivePos);
	float dz = negativeDensity - positiveDensity;

	Vec3 normal = Vec3(dx, dy, dz).GetNormalized();
	return normal;
}

void World::PushSphereOutOfTerrain(Vec3& sphereCenter, float radius)
{
	float density = GetDensityAtPosition(sphereCenter);
	density *= 100.f;
	if (density > -radius)
	{
		Vec3 normal = GetNormalAtPosition(sphereCenter);
		float penetration = radius + density;

		sphereCenter += normal * penetration;
	}
}

void World::BounceSphereOffTerrain(Vec3& out_sphereCenter, Vec3& out_velocity, float radius, float combinedElasticity)
{
	float density = GetDensityAtPosition(out_sphereCenter);
	density *= 100.f;
	if (density > -radius)
	{
		Vec3 terrainNormal = GetNormalAtPosition(out_sphereCenter);
		float projectedLength = DotProduct3D(out_velocity, terrainNormal);
		Vec3 vectorAlongNormal = terrainNormal * projectedLength;
		Vec3 vectorAlongPerpendicular = out_velocity - vectorAlongNormal;
		out_velocity = vectorAlongPerpendicular - (vectorAlongNormal * combinedElasticity);
	}
}

bool World::IsPlayerUnderwater() const
{
	float playerDepth = g_game->m_player->m_position.z;
	Vec2 playerPosXY = g_game->m_player->m_position.GetXY();

	WaterConstants waterConst = m_unsavedDef.m_waterConstants;
	float globalWaveTime = g_game->m_gameClock->GetTotalSeconds() * waterConst.m_globalWaveSpeedScale;

	constexpr float PI = 3.14159f;

	//Same wave logic as in WaterSurface.hlsl shader
	float dispZ = 0.f;
	for (int i = 0; i < waterConst.m_numWaves; ++i)
	{
		float waveSpeed = waterConst.m_waveDataExtra[i].m_waveSpeed;
		float waveTime = globalWaveTime * waveSpeed;
		Vec2 waveDir = waterConst.m_waveData[i].m_directionXY;
		float waveLength = waterConst.m_waveData[i].m_waveLength;

		float piOverWavelength = PI / waveLength;
		float waveNumber = 2.0f * piOverWavelength;
		float angularFrequency = 0.2f * sqrtf(9.81f * waveNumber);
		float phasePos = DotProduct2D(waveDir, playerPosXY);
		float phaseOffset = waterConst.m_waveDataExtra[i].m_waveOffset;
		float wavePhase = ((phasePos * waveNumber) - (angularFrequency * waveTime)) + phaseOffset;

		float amplitude = waterConst.m_waveData[i].m_amplitude;

		float sinPhase = sinf(wavePhase);
		dispZ += amplitude * sinPhase;
	}

	float seaHeight = m_unsavedDef.GetSeaLevel() + dispZ;
	return playerDepth < seaHeight;
}


//World Definition
//----------------------------------------------------------------------------------------------------------------------------------------

void World::LoadChangedWorld()
{
	m_unsavedDef.SetUsableName();
	m_unsavedDef.UpdateVegetationSpawnRadiusInfo();
	WorldDefinition::SaveDefinition(m_unsavedDef);
	m_shouldRebuild = true;
}

bool World::TryLoadFromXML(std::string const& filePath)
{
	if (!FileExists(filePath))
	{
		DebugAddMessage("could not find xml file:",5.f, Rgba8::RED, Rgba8::RED);
		return false;
	}

	WorldDefinition::InitWorldDefinitionsFromFile(filePath);
	std::string worldName = WorldDefinition::s_worldDefinitions[WorldDefinition::s_worldDefinitions.size() - 1].m_worldName;
	WorldDefinition const* worldDef = WorldDefinition::GetWorldDefinitionFromName(worldName);
	m_unsavedDef = WorldDefinition(worldDef);
	m_shouldRebuild = true;
	
	return true;
}

void World::ResetWorldToDefault()
{
	m_unsavedDef.SetUsableName();
	WorldDefinition::SaveDefinition(m_unsavedDef);

	WorldDefinition const* defaultWorldDef = WorldDefinition::GetWorldDefinitionFromName(g_gameConfigBlackboard.GetValue("DefaultWorldName", "DefaultWorld"));
	g_game->QueueLoadWorldFromWorldName(g_gameConfigBlackboard.GetValue("DefaultWorldName", "DefaultWorld"));
	m_unsavedDef = WorldDefinition(defaultWorldDef);
	m_shouldRebuild = true;
}

void World::SelectMostRecentSaves(int saveIndex)
{
	if (saveIndex >= 0 && saveIndex < WorldDefinition::s_worldDefinitions.size())
	{
		m_unsavedDef = WorldDefinition(WorldDefinition::GetWorldDefinitionFromName(WorldDefinition::s_worldDefinitions[saveIndex].m_worldName));
	}
}


//Lighting
//----------------------------------------------------------------------------------------------------------------------------------------
bool World::Event_SunSettings(EventArgs& args)
{
	if (g_game)
	{
		g_game->m_world->AdjustLightSettings(args);
		return true;
	}
	return false;
}

void World::AdjustLightSettings(EventArgs& args)
{
	if (args.HasKey("Reset"))
	{
		ResetSun();
		return;
	}

	SunConstants& sunInfo = m_unsavedDef.m_sunInfo;
	m_lightSettings->m_sunIntensity = args.GetValue("Intensity", m_lightSettings->m_sunIntensity);
	sunInfo.m_intensity = m_lightSettings->m_sunIntensity;

	m_lightSettings->m_ambientIntensity = args.GetValue("AmbientIntensity", m_lightSettings->m_ambientIntensity);
	sunInfo.m_ambientIntensity = m_lightSettings->m_ambientIntensity;

	sunInfo.m_orientation = args.GetValue("SunAngles", sunInfo.m_orientation);
	m_lightSettings->m_sunDirection = m_unsavedDef.m_sunInfo.m_orientation.Get_IFwd();

}

void World::ResetSun()
{
	SunConstants& sunInfo = m_unsavedDef.m_sunInfo;
	sunInfo.m_orientation = EulerAngles(45.f, 45.f, 0.f);
	m_lightSettings->m_sunDirection = sunInfo.m_orientation.Get_IFwd();
	m_lightSettings->m_sunIntensity = 1.f;
	sunInfo.m_intensity = 1.f;
	m_lightSettings->m_ambientIntensity = 0.5f;
	sunInfo.m_ambientIntensity = 0.5f;
}


int World::TryToDeleteFish(int numFishToDelete)
{
	int numFishDeleted = 0;
	for (int i = 0; i < (int)m_activeChunks.size(); ++i)
	{
		if (m_activeChunks[i])
		{
			numFishDeleted += m_activeChunks[i]->TryToDeleteFish(numFishToDelete - numFishDeleted);
			if (numFishDeleted >= numFishToDelete)
			{
				break;
			}
		}
	}

	return numFishDeleted;
}

int World::GetTotalNumFish() const
{
	int count = 0;
	for (int i = 0; i < (int)FishType::COUNT; ++i)
	{
		count += m_totalNumFishByType[i];
	}
	return count;
}


//Job System
//----------------------------------------------------------------------------------------------------------------------------------------

float World::GetLoadingPercent() const
{
	if (m_state != WorldState::LOADING_RESOURCES)
		return 1.0f;

	float vegetationWeightMultiplier = 1.5f;

	float weightedTotalNeeded =
		(float)m_neededFishLoaded +
		(float)m_neededTexturesLoaded +
		((float)m_neededVegetationLoaded * vegetationWeightMultiplier);

	float weightedTotalLoaded =
		(float)m_numFishLoaded +
		(float)m_numTexturesLoaded +
		((float)m_numVegetationLoaded * vegetationWeightMultiplier);

	return GetClampedFractionWithinRange(weightedTotalLoaded, 0.0f, weightedTotalNeeded);
}

void World::ExecuteChunkGenerateJob(Chunk* chunk)
{
	ChunkGenerateJob* generateJob = new ChunkGenerateJob(chunk);
	chunk->m_state = ChunkState::PENDING_GENERATE;
	g_jobSystem->SubmitJob(generateJob);
	m_activeJobs.insert(generateJob);
}

void World::ExecuteImageLoadJob(std::string const& filePath, Biome biome, bool floorFile, int textureIndex)
{
	TerrainImageLoadJob* loadJob = new TerrainImageLoadJob(filePath, biome, floorFile, textureIndex);
	g_jobSystem->SubmitJob(loadJob);
	m_activeJobs.insert(loadJob);
}

void World::ExecuteFishLoadJob(std::string const& filePath, FishType fishType)
{
	FishMeshLoadJob* fishLoadJob = new FishMeshLoadJob(filePath, fishType);
	g_jobSystem->SubmitJob(fishLoadJob);
	m_activeJobs.insert(fishLoadJob);
}

void World::ExecuteVegetationLoadJob(std::string const& filePath, VegetationType vegType)
{
	VegetationMeshLoadJob* vegLoadJob = new VegetationMeshLoadJob(filePath, m_definition->m_vegetationInfos[(int)vegType].m_diffuseTexName, m_definition->m_vegetationInfos[(int)vegType].m_normalTexName, vegType);
	g_jobSystem->SubmitJob(vegLoadJob);
	m_activeJobs.insert(vegLoadJob);
}

void World::LoadFishMeshInfo()
{
	for (int i = 0; i < (int)FishType::COUNT; ++i)
	{
		FishInfo const& fishInfo = m_definition->m_fishInfos[i];
		if(fishInfo.m_type == FishType::UN_INITIALIZED)
			continue;

		ExecuteFishLoadJob(fishInfo.m_meshInfo, (FishType)i);
		m_neededFishLoaded++;
	}
}

void World::LoadTerrainImages(TextureFolderPaths const& folderPaths, Biome biome, bool floorFile)
{
	Strings folderPathStrings;
	folderPathStrings.push_back(folderPaths.m_albedoFile);
	folderPathStrings.push_back(folderPaths.m_normalFile);
	folderPathStrings.push_back(folderPaths.m_aoFile);

	for (int i = 0; i < (int)folderPathStrings.size(); ++i)
	{
		if (folderPathStrings[i] == UN_INITIALIZED_WORLD_DATA_NAME)
		{
			m_neededTexturesLoaded--;
			continue;
		}

		//If texture already exist do not make a thread for loading image
		TextureDX12* texture = nullptr;
		if (g_renderer->IsTextureLoaded(folderPathStrings[i], texture, NUM_TERRAIN_MIPS))
		{
			unsigned int bindlessIndex = texture->GetBindlessIndex();
			WorldDefinition::LoadBiomeInfoTextureFromBindlessTextureIndex(m_definition->m_worldName, biome, floorFile, i, bindlessIndex);
			m_numTexturesLoaded++;
		}

		else // create the image on a worker thread and then load the texture
		{
			ExecuteImageLoadJob(folderPathStrings[i], biome , floorFile, i);
		}
	}
}

void World::LoadVegetationMeshInfo()
{
	for (int i = 0; i < (int)VegetationType::COUNT; ++i)
	{
		VegetationInfo const& vegInfo = m_definition->m_vegetationInfos[i];
		if(vegInfo.m_type == VegetationType::UN_INITIALIZED)
			continue;

		ExecuteVegetationLoadJob(vegInfo.m_meshInfo, (VegetationType)i);
		m_neededVegetationLoaded++;
	}
}


void World::ProcessFinishedJobs()
{
	std::vector<Job*> finishedJobs = g_jobSystem->RetrieveCompletedJobs();

	for (int i = 0; i < (int)finishedJobs.size(); ++i)
	{
		ChunkGenerateJob* generateJob = dynamic_cast<ChunkGenerateJob*>(finishedJobs[i]);
		if (generateJob)
		{
			if (generateJob->m_chunk->m_state == ChunkState::READY_TO_ACTIVATE)
			{
				ActivateChunk(generateJob->m_chunk);
			}

			else if (generateJob->m_chunk->m_state == ChunkState::PENDING_MESH_BUFFERS)
			{
				m_pendingMeshBufferChunks.insert({ generateJob->m_chunk->m_chunkCoords, generateJob->m_chunk });
			}

			if (generateJob->m_chunk->m_isEmptyChunk)
			{
				m_totalMeshlessGenerateSpeed += (GetCurrentTimeSeconds() - generateJob->m_timeStarted) * 1000.0;
				m_numCompletedMeshlessGenerateJobs++;
				m_averageMeshlessGenerateSpeed = m_totalMeshlessGenerateSpeed / m_numCompletedMeshlessGenerateJobs;
			}

			else
			{
				m_totalMeshGenerateSpeed += (GetCurrentTimeSeconds() - generateJob->m_timeStarted) * 1000.0;
				m_numCompletedMeshGenerateJobs++;
				m_averageMeshGenerateSpeed = m_totalMeshGenerateSpeed / m_numCompletedMeshGenerateJobs;
			}
		}

		TerrainImageLoadJob* imageLoadJob = dynamic_cast<TerrainImageLoadJob*>(finishedJobs[i]);
		if (imageLoadJob)
		{
			Biome biome = imageLoadJob->m_biome;
			bool isFloorFile = imageLoadJob->m_isFloorFile;
			int textureIndex = imageLoadJob->m_textureIndex;
			Image* image = imageLoadJob->m_image;
			WorldDefinition::LoadBiomeInfoTextureFromImage(m_definition->m_worldName, biome, isFloorFile, textureIndex, image);
			m_numTexturesLoaded++;
		}

		FishMeshLoadJob* fishMeshLoadJob = dynamic_cast<FishMeshLoadJob*>(finishedJobs[i]);
		if (fishMeshLoadJob)
		{
			FishType fishType = fishMeshLoadJob->m_fishType;
			WorldDefinition::LoadFishMeshInfo(m_definition->m_worldName, fishType, fishMeshLoadJob->m_outImages, fishMeshLoadJob->m_outVerts, fishMeshLoadJob->m_outIndexes, fishMeshLoadJob->m_outShaderPath);
			m_numFishLoaded++;

		}

		VegetationMeshLoadJob* vegMeshLoadJob = dynamic_cast<VegetationMeshLoadJob*>(finishedJobs[i]);
		if (vegMeshLoadJob)
		{
			VegetationType vegType = vegMeshLoadJob->m_vegetationType;
			WorldDefinition::LoadVegetationMeshInfo(m_definition->m_worldName, vegType, vegMeshLoadJob->m_outImages, vegMeshLoadJob->m_outVerts, vegMeshLoadJob->m_outIndexes, vegMeshLoadJob->m_outShaderPath, vegMeshLoadJob->m_meshRadius);
			m_numVegetationLoaded++;
		}

		else if(!imageLoadJob && !generateJob && !fishMeshLoadJob && !vegMeshLoadJob)
		{
			ERROR_AND_DIE("Received invalid world job");
		}
	}

	for (int i = 0; i < (int)finishedJobs.size(); ++i)
	{
		m_activeJobs.erase(finishedJobs[i]);
		delete finishedJobs[i];
		finishedJobs[i] = nullptr;
	}
}

ChunkGenerateJob::ChunkGenerateJob(Chunk* chunk)
	:m_chunk(chunk)
	, m_timeStarted(GetCurrentTimeSeconds())
{
}

void ChunkGenerateJob::Execute()
{
	m_chunk->m_state = ChunkState::GENERATING;
	std::vector<TerrainNoiseValues2D> terrainNoiseValuesXY;
	std::vector<VegetationNoiseValues2D> vegetationNoiseValuesXY;
	m_chunk->CreateVoxels(terrainNoiseValuesXY, vegetationNoiseValuesXY);
	if (m_chunk->IsMeshChunk())
	{
		std::vector<int> highestCrossingVoxelIndexXY;
		std::vector<BiomeSplat> highestCrossingBiomeSplatXY;
		m_chunk->CreateTerrain(highestCrossingVoxelIndexXY, highestCrossingBiomeSplatXY, terrainNoiseValuesXY, vegetationNoiseValuesXY);
		m_chunk->CreateVegetation(terrainNoiseValuesXY, vegetationNoiseValuesXY, highestCrossingVoxelIndexXY, highestCrossingBiomeSplatXY);
		m_chunk->m_state = ChunkState::PENDING_MESH_BUFFERS;
	}

	else
	{
		m_chunk->m_state = ChunkState::READY_TO_ACTIVATE;
	}

}

TerrainImageLoadJob::TerrainImageLoadJob(std::string const& imageFilePath, Biome biome, bool isFloorFile, int textureIndex)
	:m_imageFilePath(imageFilePath)
	,m_biome(biome)
	,m_isFloorFile(isFloorFile)
	,m_textureIndex(textureIndex)
{
}

void TerrainImageLoadJob::Execute()
{
	m_image = new Image(m_imageFilePath.c_str());
}

FishMeshLoadJob::FishMeshLoadJob(std::string const& meshFolderPath, FishType fishType)
	:m_meshFolderPath(meshFolderPath)
	,m_fishType(fishType)
	,m_outShaderPath(UN_INITIALIZED_WORLD_DATA_NAME)
{
}

FishMeshLoadJob::~FishMeshLoadJob()
{
	delete m_outImages.m_diffuseImage;
	m_outImages.m_diffuseImage = nullptr;
	delete m_outImages.m_normalImage;
	m_outImages.m_normalImage = nullptr;
	delete m_outImages.m_specularImage;
	m_outImages.m_specularImage = nullptr;
	delete m_outImages.m_aoImage;
	m_outImages.m_aoImage = nullptr;
	delete m_outImages.m_emissiveImage;
	m_outImages.m_emissiveImage = nullptr;
}

void FishMeshLoadJob::Execute()
{
	if (!LoadOBJMeshFile(m_outVerts, m_outIndexes, m_meshFolderPath, true))
	{
		std::string fishTypeName = GetFishTypeNameFromType(m_fishType);
		DebugAddMessage(Stringf("Mesh Load for: %s (%s) failed", fishTypeName.c_str(), m_meshFolderPath.c_str()), 30.f, Rgba8::RED, Rgba8::RED);
		return;
	}

	if (!WorldDefinition::TryToGetFishInfoImagesAndShaderFile(m_meshFolderPath, m_outImages, m_outShaderPath))
	{
		std::string fishTypeName = GetFishTypeNameFromType(m_fishType);
		DebugAddMessage(Stringf("Image Load for: %s (%s) failed", fishTypeName.c_str(), m_meshFolderPath.c_str()), 30.f, Rgba8::RED, Rgba8::RED);
		return;
	}


}

VegetationMeshLoadJob::VegetationMeshLoadJob(std::string const& meshFolderPath, std::string const& diffuseTextureName, std::string const& normalTextureName, VegetationType vegType)
	:m_meshFolderPath(meshFolderPath)
	,m_vegetationType(vegType)
	,m_diffuseTextureName(diffuseTextureName)
	,m_normalTextureName(normalTextureName)
	,m_outShaderPath(UN_INITIALIZED_WORLD_DATA_NAME)
{

}

VegetationMeshLoadJob::~VegetationMeshLoadJob()
{
	delete m_outImages.m_diffuseImage;
	m_outImages.m_diffuseImage = nullptr;
	delete m_outImages.m_normalImage;
	m_outImages.m_normalImage = nullptr;
	delete m_outImages.m_specularImage;
	m_outImages.m_specularImage = nullptr;
	delete m_outImages.m_aoImage;
	m_outImages.m_aoImage = nullptr;
	delete m_outImages.m_emissiveImage;
	m_outImages.m_emissiveImage = nullptr;
}

void VegetationMeshLoadJob::Execute()
{
	m_meshRadius = 0.f;

	switch (m_vegetationType)
	{
	case VegetationType::SEA_GRASS:
		AddVertsForGrassBlade(m_outVerts, m_outIndexes, Vec3::ZERO, 1.f, 1.f, 20, Rgba8::WHITE);
		break;
	case VegetationType::SEA_ANEMONE:
		AddVertsForIndexedZCylinder3D(m_outVerts, m_outIndexes, Vec3::ZERO, 1.f, 0.5f, 32, 32);
		break;
	case VegetationType::GRASS:
		AddVertsForGrassBlade(m_outVerts, m_outIndexes, Vec3::ZERO, 1.f, 1.f, 20, Rgba8::WHITE);
		break;
	case VegetationType::KELP:
		if (!LoadOBJMeshFile(m_outVerts, m_outIndexes, m_meshFolderPath, true))
		{
			std::string vegName = GetVegetationTypeNameFromType(m_vegetationType);
			DebugAddMessage(Stringf("Mesh Load for: %s (%s) failed", vegName.c_str(), m_meshFolderPath.c_str()), 30.f, Rgba8::RED, Rgba8::RED);
		}

		if (!WorldDefinition::TryToGetVegInfoImagesAndShaderFile(m_meshFolderPath, m_outImages, m_outShaderPath))
		{
			std::string vegName = GetVegetationTypeNameFromType(m_vegetationType);
			DebugAddMessage(Stringf("Image Load for: %s (%s) failed", vegName.c_str(), m_meshFolderPath.c_str()), 30.f, Rgba8::RED, Rgba8::RED);
		}
		break;
	default:
		if (!LoadOBJMeshFile(m_outVerts, m_outIndexes, m_meshFolderPath, true))
		{
			std::string vegName = GetVegetationTypeNameFromType(m_vegetationType);
			DebugAddMessage(Stringf("Mesh Load for: %s (%s) failed", vegName.c_str(), m_meshFolderPath.c_str()), 30.f, Rgba8::RED, Rgba8::RED);
		}

		if (!WorldDefinition::TryToGetVegInfoImagesAndShaderFile(m_meshFolderPath, m_outImages, m_outShaderPath))
		{
			std::string vegName = GetVegetationTypeNameFromType(m_vegetationType);
			DebugAddMessage(Stringf("Image Load for: %s (%s) failed", vegName.c_str(), m_meshFolderPath.c_str()), 30.f, Rgba8::RED, Rgba8::RED);
		}

		float maxRadiusSqrd = 1.f;
		for (int i = 0; i < (int)m_outVerts.size(); ++i)
		{
			float distSqrd = m_outVerts[i].m_position.GetXY().GetLengthSquared();
			maxRadiusSqrd = GetMax(maxRadiusSqrd, distSqrd);
		}

		m_meshRadius = sqrtf(maxRadiusSqrd);

		break;
	}

	if (m_diffuseTextureName != UN_INITIALIZED_WORLD_DATA_NAME)
	{
		m_outImages.m_diffuseImage = new Image(m_diffuseTextureName.c_str());
	}

	if (m_normalTextureName != UN_INITIALIZED_WORLD_DATA_NAME)
	{
		m_outImages.m_normalImage = new Image(m_normalTextureName.c_str());
	}
}
