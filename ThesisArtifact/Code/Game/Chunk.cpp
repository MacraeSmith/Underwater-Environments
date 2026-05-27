#include "Game/Chunk.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Game.hpp"
#include "Game/Player.hpp"
#include "Game/TerrainUtils.hpp"
#include "Game/World.hpp"
#include "Game/WorldDefinition.hpp"
#include "Game/Fish.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Renderer/RendererDX12.hpp"
#include "Engine/Renderer/CommandQueue.hpp"
#include "Engine/Renderer/VertexBufferDX12.hpp"
#include "Engine/Renderer/IndexBufferDX12.hpp"
#include "Engine/Renderer/StructuredBufferDX12.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/RawNoise.hpp"
#include "Engine/Math/SmoothNoise.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Timer.hpp"
#include "Engine/Math/HashUtils.hpp"
#include <thread>

Chunk::Chunk(World* world, IntVec3 const& chunkCoords)
:m_world(world)
,m_chunkCoords(chunkCoords)
{

	const IntVec3 CHUNK_SIZE = m_world->m_chunkSize;
	const int VOXEL_SCALE = m_world->m_definition->m_voxelScale;

	Vec3 mins((m_chunkCoords.x * CHUNK_SIZE.x), (m_chunkCoords.y * CHUNK_SIZE.y), (m_chunkCoords.z * CHUNK_SIZE.z));
	Vec3 maxs((int)mins.x + CHUNK_SIZE.x, (int)mins.y + CHUNK_SIZE.y, (int)mins.z + CHUNK_SIZE.z);

	FloatRange zRange(mins.z, maxs.z);
	float seaLevel = m_world->m_definition->GetSeaLevel();
	std::string chunkLabel = Stringf(" Chunk: (%i, %i, %i)", m_chunkCoords.x, m_chunkCoords.y, m_chunkCoords.z);
	if (zRange.IsOnRange(seaLevel))
	{
		VertTBNs verts;
		IndexList indexes;
		AddVertsForIndexedGerstnerQuadMesh(verts, indexes, IntVec2(20 * VOXEL_SCALE, 20 * VOXEL_SCALE), Vec3(mins.x, mins.y, seaLevel), Vec3(maxs.x, mins.y, seaLevel),
			Vec3(maxs.x, maxs.y, seaLevel), Vec3(mins.x, maxs.y, seaLevel));

		m_waterSurfaceVbo = g_renderer->CreateVertexBuffer(verts, "WaterSurface_VBO" + chunkLabel);
		m_waterSurfaceIbo = g_renderer->CreateIndexBuffer(indexes, "WaterSurface_IBO" + chunkLabel);
		m_isSeaSurfaceChunk = true;
	}

	m_chunkBounds = AABB3(mins, maxs);

	Verts verts;
	IndexList indexes;
	AddVertsForIndexedWireFrameAABB3D(verts, indexes, m_chunkBounds, 0.5f);
	m_debugVbo = g_renderer->CreateVertexBuffer(verts, "ChunkDebug_VBO" + chunkLabel);
	m_debugIbo = g_renderer->CreateIndexBuffer(indexes, "ChunkDebug_IBO" + chunkLabel);

}

Chunk::~Chunk()
{
	m_world = nullptr;

	delete m_waterSurfaceVbo;
	m_waterSurfaceVbo = nullptr;

	delete m_waterSurfaceIbo;
	m_waterSurfaceIbo = nullptr;

	delete m_terrainVbo;
	m_terrainVbo = nullptr;

	delete m_terrainIbo;
	m_terrainIbo = nullptr;

	delete m_debugVbo;
	m_debugVbo = nullptr;

	delete m_debugIbo;
	m_debugIbo = nullptr;

	for (int i = 0; i < (int)VegetationType::COUNT; ++i)
	{
		VegetationBufferInfo& vegInfo = m_vegetationBufferInfos[i];
		delete vegInfo.m_terrainSBO;
		vegInfo.m_terrainSBO = nullptr;
		delete vegInfo.m_instanceSBO;
		vegInfo.m_instanceSBO = nullptr;
	}


	for (int i = 0; i < (int)m_fish.size(); ++i)
	{
		delete m_fish[i];
		m_fish[i] = nullptr;
	}
}

void Chunk::Update(Frustum const& cameraFrustum)
{
	if(m_state != ChunkState::ACTIVE)
		return;	

	UpdateFish();

	if (m_dispatchedVegetation)
	{
		for (int i = 0; i < (int)VegetationType::COUNT; ++i)
		{
			VegetationBufferInfo& vegInfo = m_vegetationBufferInfos[i];
			if (!vegInfo.m_shouldRender)
			{
				vegInfo.m_shouldRender = true;
			}
		}
	}

	m_inFrustum = IsChunkInFrustrum(cameraFrustum);
}


void Chunk::UpdateFish()
{
	WorldDefinition const& worldDef = m_world->m_unsavedDef;
	if(!worldDef.m_activeFish || m_state != ChunkState::ACTIVE)
		return;


	const float deltaSeconds = g_game->m_gameClock->GetDeltaSeconds();
	const Vec3 PLAYER_POS = g_game->m_player->m_position;
	const int MAX_NUM_FISH_NEIGHBORS_TO_CHECK = worldDef.m_maxNumFishNeighborsToCheck;

	const uint32_t FRAME_COUNT = m_world->m_gameplayFrameCount;

	for (int i = 0; i < (int)m_fish.size(); ++i)
	{
		Fish* fish = m_fish[i];
		if(!fish || fish->m_isGarbage)
			continue;

		FishInfo fishInfo = fish->m_fishInfo;
		FishType fishType = fishInfo.m_type;
		if(!fishInfo.m_isActive)
			continue;

		Vec3 velocitySum = Vec3::ZERO;
		Vec3 positionSum = Vec3::ZERO;
		Vec3 separationSum = Vec3::ZERO;
		int neighborCount = 0;


		int fishCount = (int)m_fish.size();
		if (fishCount == 0)
			continue;

		//allows fish to find new neighbors each frame
		uint32_t baseSeed = (fish->m_id * 2654435761u) ^ (FRAME_COUNT * 1013904223u);
		uint32_t start = (uint32_t)(((uint64_t)baseSeed * (uint64_t)fishCount) >> 32);
		baseSeed = (baseSeed * 1664525u) + 1013904223u;

		for (int step = 0; step < fishCount; ++step)
		{
			if(neighborCount >= MAX_NUM_FISH_NEIGHBORS_TO_CHECK)
				continue;

			int idx = (start + step);
			if (idx >= fishCount)
			{
				idx -= fishCount;
			}

			Fish* otherFish = m_fish[idx];
			if(!otherFish || otherFish == fish || otherFish->m_isGarbage || !otherFish->m_fishInfo.m_isActive)
				continue;


			Vec3 diff = (fish->m_position - otherFish->m_position);
			float distSqrd = diff.GetLengthSquared();
			if(distSqrd > fishInfo.m_flockingRadiusSqrd)
				continue;

			if (fishType == FishType::SHARK) // sharks chase other fish but do not school with each other
			{
				if (otherFish->m_fishInfo.m_type == fishType)
				{
					if (distSqrd < fishInfo.m_separationRadiusSqrd)
					{
						separationSum += (diff / (distSqrd + 0.0001f));
						neighborCount++;
					}
				}

				else
				{
					velocitySum += otherFish->m_velocity;
					positionSum += otherFish->m_position;
					neighborCount++;

					if (distSqrd < fishInfo.m_separationRadiusSqrd)
					{
						separationSum += (diff / (distSqrd + 0.0001f));
						neighborCount++;
					}
				}
			}

			else
			{
				if (otherFish->m_fishInfo.m_type == fishType)
				{
					velocitySum += otherFish->m_velocity;
					positionSum += otherFish->m_position;
					neighborCount++;

					if (distSqrd < fishInfo.m_separationRadiusSqrd)
					{
						separationSum += (diff / (distSqrd + 0.0001f));
					}
				}

				else if (distSqrd < fishInfo.m_separationRadiusSqrd)
				{
					separationSum += (diff / (distSqrd + 0.0001f));
					neighborCount++;
				}
			}

		}

		
		Vec3 alignment = Vec3::ZERO;
		Vec3 cohesion = Vec3::ZERO;
		Vec3 separation = Vec3::ZERO;

		if (neighborCount > 0)
		{
			float invCount = (1.f / (float)neighborCount);

			Vec3 avgVelocity = velocitySum * invCount;
			Vec3 avgPos = positionSum * invCount;

			if (avgVelocity.GetLengthSquared() > 0.000001f)
			{
				alignment = avgVelocity.GetNormalized() * fishInfo.m_alignment * 100.f;
			}

			Vec3 toCenter = (avgPos - fish->m_position);
			if (toCenter.GetLengthSquared() > 0.000001f)
			{
				cohesion = toCenter.GetNormalized() * fishInfo.m_cohesion * 100.f;
			}

			if (separationSum.GetLengthSquared() > 0.000001f)
			{
				separation = separationSum * fishInfo.m_separation * 100.f;
			}
		}

		fish->UpdateFishBehavior(alignment, cohesion, separation, PLAYER_POS, deltaSeconds);
	};

	for (int i = 0; i < (int)m_fish.size(); ++i)
	{
		Fish* fish = m_fish[i];
		if(!fish || fish->m_isGarbage)
			continue;

		Chunk* newChunk = m_world->GetActiveChunkFromPosition(fish->m_position);

		if (newChunk != this)
		{
			fish->MigrateChunk(newChunk);
		}
	}

	for (int i = 0; i < (int)m_fish.size(); ++i)
	{
		Fish* fish = m_fish[i];
		if (!fish || fish->m_isGarbage)
			continue;

		fish->Update();
	}

}

void Chunk::UpdateGarbageFish()
{
	for (int i = 0; i < (int)m_fish.size(); ++i)
	{
		Fish* fish = m_fish[i];

		if (fish && fish->m_isGarbage)
		{
			delete m_fish[i];
			m_fish[i] = nullptr;
			m_world->m_totalNumFishByType[(int)fish->m_fishInfo.m_type]--;
		}
	}
}

void Chunk::RenderDebug() const
{
	if (!m_debugVbo)
		return;


	Rgba8 color = Rgba8(100, 255, 255);

	if (IsMeshChunk())
	{
		color = Rgba8(255,147,0);
	}


	else if (m_world->m_debugView == WorldDebugView::MESH_CHUNK_BOUNDS)
		return;

	if (g_game->m_player->GetCameraMode() == CameraMode::FRUSTUM_CULL && m_inFrustum)
	{
		color = Rgba8::MAGENTA;
	}

	g_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_renderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_renderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP);
	g_renderer->BindRootSignature(nullptr);
	g_renderer->BindTexture(nullptr);
	g_renderer->BindShader(nullptr);
	g_renderer->SetModelConstants(Mat44(), color);
	g_renderer->DrawIndexedVertexBuffer(m_debugVbo, m_debugIbo, m_debugIbo->GetNumIndexes());
	
}

void Chunk::RenderTerrain() const
{
	if(m_state != ChunkState::ACTIVE || !IsMeshChunk() || !m_inFrustum)
		return;
	
	if(!m_terrainVbo || !m_terrainIbo)
		return;

	g_renderer->DrawIndexedVertexBuffer(m_terrainVbo, m_terrainIbo, m_terrainIbo->GetNumIndexes());
	
}

void Chunk::RenderVegetation(VegetationType vegetationType) const
{
	if (m_state != ChunkState::ACTIVE || !IsMeshChunk() || !m_inFrustum)
		return;

	VegetationBufferInfo bufferInfo = m_vegetationBufferInfos[(int)vegetationType];
	if(!bufferInfo.m_shouldRender || bufferInfo.m_instanceCount <= 0)
		return;

	WorldDefinition const* worldDef = m_world->m_definition;
	VegetationInfo vegInfo = worldDef->m_vegetationInfos[(int)vegetationType];
	if(!worldDef->m_activeVegetation || !vegInfo.m_isActive)
		return;
	
	VertexBufferDX12* vbo = vegInfo.m_instanceVbo;
	IndexBufferDX12* ibo = vegInfo.m_instanceIBO;
	g_renderer->BindStructuredBuffer(bufferInfo.m_instanceSBO, 0);
	g_renderer->DrawInstancedIndexedVertexBufer(vbo, ibo, ibo->GetNumIndexes(), bufferInfo.m_instanceCount);
}

void Chunk::RenderWaterSurface() const
{
	if (m_state != ChunkState::ACTIVE || !m_inFrustum)
		return;

	if (!m_isSeaSurfaceChunk || !m_waterSurfaceVbo || !m_waterSurfaceIbo)
		return;

	Rgba8 color = Rgba8(0, 0, 255, 255);
	g_renderer->SetModelConstants(Mat44(), color);
	g_renderer->DrawIndexedVertexBuffer(m_waterSurfaceVbo, m_waterSurfaceIbo, m_waterSurfaceIbo->GetNumIndexes());
}

void Chunk::GatherFishRenderData(std::vector<std::vector<FishInstanceData>>& fishDataGroups)
{
	if (m_fish.size() <= 0 || !m_inFrustum)
		return;


	WorldDefinition const& worldDef = m_world->m_unsavedDef;
	const int maxInstanceCount = m_world->m_definition->m_maxNumFish;

	bool activeFishTypes[(int)FishType::COUNT] = {};
	for (int i = 0; i < (int)FishType::COUNT; ++i)
	{
		activeFishTypes[i] = worldDef.m_fishInfos[i].m_isActive;
		if (worldDef.m_fishInfos[i].m_type == FishType::UN_INITIALIZED)
		{
			activeFishTypes[i] = false;
		}
	}

	for (int i = 0; i < (int)m_fish.size(); ++i)
	{
		if(!m_fish[i])
			continue;

		FishType fishType = m_fish[i]->m_type;
		if (!activeFishTypes[(int)fishType])
			continue;

		if(fishDataGroups[(int)fishType].size() >= maxInstanceCount)
			continue;

		FishInstanceData fishData = m_fish[i]->GetInstanceData();
		fishDataGroups[(int)fishType].push_back(fishData);
	}

}


void Chunk::RenderDebugFish(VertexBufferDX12* sphereVBO) const
{
	if (m_fish.size() <= 0 || !m_inFrustum)
		return;

	for (int i = 0; i < (int)m_fish.size(); ++i)
	{
		if (m_fish[i])
		{
			m_fish[i]->RenderDebug(sphereVBO);
		}
	}
}


//World Creation
//--------------------------------------------------------------------------------------------


void Chunk::CreateVoxels(std::vector<TerrainNoiseValues2D>& terrainNoiseValuesXY, std::vector<VegetationNoiseValues2D>& vegetationNoiseValuesXY)
{
	WorldDefinition const* worldDef = m_world->m_definition;
	const float VOXEL_SCALE = (float)worldDef->m_voxelScale;

	const float WORLD_HEIGHT = worldDef->m_worldHeight;
	const float WORLD_HALF_HEIGHT = WORLD_HEIGHT * 0.5f;
	const float BASE_TERRAIN_BIAS = 1.f - worldDef->m_baseTerrainBiasFactor;
	const float FINAL_TERRAIN_BIAS = 1.f - worldDef->m_finalTerrainBiasFactor;
	const float DEFAULT_TERRAIN_HEIGHT = worldDef->GetSeaLevel() - worldDef->m_baseTerrainDepthBelowSeaLevel;
	const float MIN_TERRAIN_HEIGHT = worldDef->GetSeaLevel() - worldDef->m_waterConstants.m_maxTerrainDepth;

	//Save 2D noise info
	//-----------------------------------------------------------------------------------------
	std::vector<float>heightOffsets;
	std::vector<float>baseTerrainHeightsXY;
	std::vector<float>squashFactors;
	baseTerrainHeightsXY.reserve(NUM_NOISE_VOXELS_2D);
	heightOffsets.reserve(NUM_NOISE_VOXELS_2D);
	squashFactors.reserve(NUM_NOISE_VOXELS_2D);
	vegetationNoiseValuesXY.reserve(NUM_NOISE_VOXELS_2D);
	terrainNoiseValuesXY.reserve(NUM_NOISE_VOXELS_2D);

	Vec2 noiseStartPos2D = m_chunkBounds.m_mins.GetXY() - (Vec2::ONE * VOXEL_SCALE);

	for (int rowNum = 0; rowNum < NUM_NOISE_VOXELS_Y; ++rowNum)
	{
		for (int columNum = 0; columNum < NUM_NOISE_VOXELS_X; ++columNum)
		{
			Vec2 pos = noiseStartPos2D + (Vec2((float)columNum, (float)rowNum) * VOXEL_SCALE);
			TerrainNoiseValues2D noiseValues = worldDef->GetTerrainNoiseValuesAtPos2D(pos);
			terrainNoiseValuesXY.push_back(noiseValues);

			float heightOffset = worldDef->GetTerrainHeightOffsetAtPos2D(noiseValues);
			heightOffsets.push_back(heightOffset);
			squashFactors.push_back(worldDef->GetSquashFactorAtPos2D(noiseValues));

			float baseTerrainHeightXY = DEFAULT_TERRAIN_HEIGHT + (heightOffset * WORLD_HALF_HEIGHT);
			baseTerrainHeightsXY.push_back(baseTerrainHeightXY);

			VegetationNoiseValues2D vegNoiseValues = worldDef->GetVegetationNoiseValues2DAtPos2D(pos);
			vegetationNoiseValuesXY.push_back(vegNoiseValues);
		}
	}


	//Chunk 3D Density field including neighbor voxels
	//----------------------------------------------------------------------------------------
	std::vector<float>densityField;
	densityField.reserve(NUM_NOISE_VOXELS_3D);

	bool hasEmptyVoxel = false;
	bool hasSolidVoxel = false;

	Vec3 noiseStartPos3D = m_chunkBounds.m_mins - (Vec3::ONE * VOXEL_SCALE);
	for (int layerNum = 0; layerNum < NUM_NOISE_VOXELS_Z; ++layerNum)
	{
		if (g_jobSystem->IsAborting())
			return;

		int idXY = 0;
		for (int rowNum = 0; rowNum < NUM_NOISE_VOXELS_Y; ++rowNum)
		{
			for (int columNum = 0; columNum < NUM_NOISE_VOXELS_X; ++columNum)
			{
				Vec3 globalPos = noiseStartPos3D + (Vec3(columNum, rowNum, layerNum) * VOXEL_SCALE);

				//Global Squash Bias
				float normalizedZ = GetClampedFractionWithinRange(globalPos.z, MIN_TERRAIN_HEIGHT, WORLD_HEIGHT);
				float squashCurve = 4.f * normalizedZ * (1.f - normalizedZ);
				float verticalBias = Lerp(1.f, -1.f, SmoothStep3(normalizedZ));

				//Base 3D density
				float baseTerrainNoise = worldDef->GetBaseTerrainNoiseAtPos(globalPos);
				float density = (verticalBias * (1.f - BASE_TERRAIN_BIAS)) + (baseTerrainNoise * squashCurve * BASE_TERRAIN_BIAS);

				//Terrain Shaping
				float heightOffset = heightOffsets[idXY];
				float squashFactor = squashFactors[idXY];
				
				float baseTerrainHeightXY = baseTerrainHeightsXY[idXY];
				float terrainOffsetFromBaseHeight = (globalPos.z - baseTerrainHeightXY) / GetMax(baseTerrainHeightXY, 0.1f);
				density += heightOffset;
				density -= squashFactor * terrainOffsetFromBaseHeight;


				//final squash
				density = (verticalBias * (1.f - FINAL_TERRAIN_BIAS)) + (density * squashCurve * FINAL_TERRAIN_BIAS);
				
				//Add to densityField
				densityField.push_back(density);

				//flag solid or empty
				if (density > 0.0f)
				{
					hasSolidVoxel = true;
				}
				else if (density < 0.0f)
				{
					hasEmptyVoxel = true;
				}

				idXY++;
			}
		}
	}


	//Exit if fully solid or empty chunk
	//----------------------------------------------------------------------------------------
	if (hasEmptyVoxel && !hasSolidVoxel)
	{
		m_isEmptyChunk = true;
		return;
	}

	
	else if (hasSolidVoxel && !hasEmptyVoxel)
	{
		m_isSolidChunk = true;
		return;
	}
	
	
	//Fill Voxels
	//--------------------------------------------------------------------------------------
	m_voxels.reserve(NUM_BLOCKS_IN_CHUNK);
	for (int layerNum = 0; layerNum < CHUNK_SIZE_Z; ++layerNum)
	{
		if (g_jobSystem->IsAborting())
			return;

		int zOffset = (layerNum + 1) * NUM_NOISE_VOXELS_Y * NUM_NOISE_VOXELS_X;
		for (int rowNum = 0; rowNum < CHUNK_SIZE_Y; ++rowNum)
		{
			int baseIndex3D = zOffset + ((rowNum + 1) * NUM_NOISE_VOXELS_X) + 1;

			for (int columnNum = 0; columnNum < CHUNK_SIZE_X; ++columnNum)
			{
				int densityIndex = baseIndex3D + columnNum;

				Voxel newVoxel;

				Vec3 globalPos = GetGlobalPosFromLocalCoords(IntVec3(columnNum, rowNum, layerNum));
				newVoxel.SetDensity(densityField[densityIndex]);
				m_voxels.push_back(newVoxel);

			}
		}
	}
}

void Chunk::TrySpawnFish()
{
	if(m_isSolidChunk)
		return;

	WorldDefinition const* worldDef = m_world->m_definition;
	WorldDefinition const& unsavedDef = m_world->m_unsavedDef;

	const int MAX_FISH = unsavedDef.m_maxNumFish;

	if(m_world->GetTotalNumFish() >= MAX_FISH)
		return;

	const float VOXEL_SCALE = (float)worldDef->m_voxelScale;

	int numFishOfEachTypeToSpawn[(int)FishType::COUNT] = {};

	float minSeaSwimDepth = 0;

	for (int i = 0; i < (int)FishType::COUNT; ++i)
	{
		numFishOfEachTypeToSpawn[i] = 0;
		FishInfo fishInfo = unsavedDef.m_fishInfos[i];
		if (fishInfo.m_minOceanDepth > minSeaSwimDepth)
		{
			minSeaSwimDepth = fishInfo.m_minOceanDepth;
		}
		int numFishOfType = m_world->m_totalNumFishByType[i];
		if(numFishOfType >= fishInfo.m_maxNumber)
			continue;

		int totalNumFish = m_world->GetTotalNumFish();
		if(totalNumFish > MAX_FISH)
			continue;

		int numFishToSpawn = fishInfo.m_spawnThickness;
		if (numFishOfType + numFishToSpawn >= fishInfo.m_maxNumber)
		{
			numFishToSpawn = GetClampedInt(fishInfo.m_maxNumber - numFishOfType, 0, fishInfo.m_spawnThickness);
		}

		if (totalNumFish + numFishToSpawn >= MAX_FISH)
		{
			numFishToSpawn = GetClampedInt(MAX_FISH - totalNumFish, 0, fishInfo.m_spawnThickness);

		}

		numFishOfEachTypeToSpawn[i] = numFishToSpawn;
	}

	int fishTypeIndex = 0;
	const float SEA_LEVEL_CHECK = unsavedDef.GetSeaLevel() - minSeaSwimDepth;

	for (int layerNum = 0; layerNum < CHUNK_SIZE_Z; layerNum += 4)
	{
		float globalZ = layerNum * VOXEL_SCALE;
		if (globalZ >= SEA_LEVEL_CHECK)
			return;

		for (int rowNum = 0; rowNum < CHUNK_SIZE_Y; rowNum += 4 )
		{
			float globalY = rowNum * VOXEL_SCALE;
			for (int columnNum = 0; columnNum < CHUNK_SIZE_X; columnNum += 4)
			{
				if (numFishOfEachTypeToSpawn[fishTypeIndex] <= 0)
				{
					fishTypeIndex++;
					if (fishTypeIndex >= (int)FishType::COUNT)
					{
						return;
					}

					continue;
				}

				float globalX = columnNum * VOXEL_SCALE;
				Vec3 spawnPos = m_chunkBounds.m_mins + Vec3(globalX, globalY, globalZ);

				if (numFishOfEachTypeToSpawn[fishTypeIndex] > 0 && GetDensityAtPosition(spawnPos) < -0.25f)
				{
					Fish* newFish = new Fish(m_world, this, spawnPos, FishType(fishTypeIndex));
					m_world->m_totalNumFishByType[fishTypeIndex]++;
					AddFish(newFish);
					numFishOfEachTypeToSpawn[fishTypeIndex]--;
				}

			}
		}
	}
}

void Chunk::CreateTerrain(std::vector<int>& highestCrossingVoxelIndexXY, std::vector<BiomeSplat>& highestCrossingBiomeSplatXY, std::vector<TerrainNoiseValues2D> const& terrainNoiseValuesXY, std::vector<VegetationNoiseValues2D> const& vegetationNoiseValuesXY)
{
	Vec3 chunkStartPos = m_chunkBounds.m_mins;
	IntVec3 localCubePos;
	constexpr int NUM_CORNERS = 8;

	WorldDefinition const* worldDef = m_world->m_definition;
	float const VOXEL_SCALE = (float)worldDef->m_voxelScale;

	m_terrainVerts.reserve(10000);
	m_terrainIndexes.reserve(50000);

	std::vector<IntVec3> edgeXYZVertIndexes;
	edgeXYZVertIndexes.resize(NUM_BLOCKS_IN_CHUNK, IntVec3(-1, -1, -1));

	highestCrossingVoxelIndexXY.resize(NUM_BLOCKS_IN_CHUNK_2D, -1);
	highestCrossingBiomeSplatXY.resize(NUM_BLOCKS_IN_CHUNK_2D);

	for (int levelNum = 0; levelNum < CHUNK_SIZE_Z; levelNum += MARCHING_CUBE_SCALE)
	{
		localCubePos.z = levelNum;
		Vec3 globalPointPos(0.f, 0.f, chunkStartPos.z + ((float)levelNum * VOXEL_SCALE));
		int idXY = 0;

		for (int rowNum = 0; rowNum < CHUNK_SIZE_Y; rowNum += MARCHING_CUBE_SCALE)
		{
			localCubePos.y = rowNum;
			globalPointPos.y = chunkStartPos.y + ((float)rowNum * VOXEL_SCALE);

			if (g_jobSystem->IsAborting())
			{
				return;
			}

			for (int columnNum = 0; columnNum < CHUNK_SIZE_X; columnNum += MARCHING_CUBE_SCALE)
			{
				localCubePos.x = columnNum;
				globalPointPos.x = chunkStartPos.x + ((float)columnNum * VOXEL_SCALE);
				int noiseIndex2D = ((rowNum + 1) * NUM_NOISE_VOXELS_X) + (columnNum + 1);

				Vec3 globalCorners[NUM_CORNERS] =
				{
					globalPointPos + (cornerOffsetsF[0] * VOXEL_SCALE),
					globalPointPos + (cornerOffsetsF[1] * VOXEL_SCALE),
					globalPointPos + (cornerOffsetsF[2] * VOXEL_SCALE),
					globalPointPos,
					globalPointPos + (cornerOffsetsF[4] * VOXEL_SCALE),
					globalPointPos + (cornerOffsetsF[5] * VOXEL_SCALE),
					globalPointPos + (cornerOffsetsF[6] * VOXEL_SCALE),
					globalPointPos + (cornerOffsetsF[7] * VOXEL_SCALE),
				};

				IntVec3 localCorners[NUM_CORNERS] =
				{
					localCubePos + cornerOffsetsI[0],
					localCubePos + cornerOffsetsI[1],
					localCubePos + cornerOffsetsI[2],
					localCubePos,
					localCubePos + cornerOffsetsI[4],
					localCubePos + cornerOffsetsI[5],
					localCubePos + cornerOffsetsI[6],
					localCubePos + cornerOffsetsI[7],
				};

				float cubeDensityValues[NUM_CORNERS] = {};
				int cornerIndexes[NUM_CORNERS] = {};

				for (int i = 0; i < NUM_CORNERS; ++i)
				{
					int cornerIndex = IsLocalCoordsInChunk(localCorners[i]) ? GetVoxelIndexFromLocalCoords(localCorners[i]) : -1;
					float density = 0.f;

					if (cornerIndex >= 0 && cornerIndex < NUM_BLOCKS_IN_CHUNK)
					{
						density = m_voxels[cornerIndex].GetDensity();
					}
					else
					{
						density = GetDensityAtPosition(globalCorners[i]);
					}

					cubeDensityValues[i] = density;
					cornerIndexes[i] = cornerIndex;
				}

				int voxelIndex = cornerIndexes[3];

				uint8_t lookupKey = 0;
				for (int i = 0; i < NUM_CORNERS; ++i)
				{
					lookupKey |= (cubeDensityValues[i] > 0.0f) << i;
				}

				m_voxels[voxelIndex].SetBlockType(lookupKey);

				unsigned int edgeTableIndex = EDGES_KEY_TABLE[lookupKey];
				if (edgeTableIndex == 0)
				{
					idXY++;
					continue;
				}

				highestCrossingVoxelIndexXY[idXY] = voxelIndex;

				TerrainNoiseValues2D const& crossingTerrainNoiseValues = terrainNoiseValuesXY[noiseIndex2D];
				VegetationNoiseValues2D const& crossingVegetationNoiseValues = vegetationNoiseValuesXY[noiseIndex2D];
				Vec3 crossingNormal = GetNormalAtPosition(globalPointPos);
				highestCrossingBiomeSplatXY[idXY] = worldDef->GetBiomeSplatFromNoiseValues(crossingTerrainNoiseValues, crossingVegetationNoiseValues, globalPointPos, crossingNormal);

				float cornerBiomeScores[NUM_CORNERS][(int)Biome::NUM_BIOMES] = {};
				bool cornerBiomeScoresValid[NUM_CORNERS] = {};

				for (int cornerIndex = 0; cornerIndex < NUM_CORNERS; ++cornerIndex)
				{
					TerrainNoiseValues2D cornerTerrainNoiseValues = GetInterpolatedTerrainNoiseValuesAtPosition(globalCorners[cornerIndex], chunkStartPos, VOXEL_SCALE, terrainNoiseValuesXY);
					VegetationNoiseValues2D cornerVegetationNoiseValues = GetInterpolatedVegetationNoiseValuesAtPosition(globalCorners[cornerIndex], chunkStartPos, VOXEL_SCALE, vegetationNoiseValuesXY);
					Vec3 cornerNormal = GetNormalAtPosition(globalCorners[cornerIndex]);
					worldDef->GetBiomeScoresFromNoiseValues(cornerBiomeScores[cornerIndex], cornerTerrainNoiseValues, cornerVegetationNoiseValues, globalCorners[cornerIndex], cornerNormal);
					cornerBiomeScoresValid[cornerIndex] = true;
				}

				for (int i = 0; TRIS_TABLE[lookupKey][i] != -1; i += 3)
				{
					int edges[3] =
					{
						TRIS_TABLE[lookupKey][i],
						TRIS_TABLE[lookupKey][i + 1],
						TRIS_TABLE[lookupKey][i + 2],
					};

					for (int j = 0; j < 3; ++j)
					{
						int edge = edges[j];
						int vertIndex = -1;
						int edgeLookupIndex = -1;
						int localEdgeLookupIndex = EDGE_CORNER_TO_INDEX[edge];

						if (IsLocalCoordsInChunk(localCorners[localEdgeLookupIndex]))
						{
							edgeLookupIndex = cornerIndexes[localEdgeLookupIndex];
							IntVec3 vertIndexes = edgeXYZVertIndexes[edgeLookupIndex];

							if (edge == 0 || edge == 2 || edge == 4 || edge == 6)
							{
								vertIndex = vertIndexes.x;
							}
							else if (edge < 8)
							{
								vertIndex = vertIndexes.y;
							}
							else if (edge < 12)
							{
								vertIndex = vertIndexes.z;
							}
						}

						IntVec2 edgeCornerPairIndexes = EDGE_CORNERS_TABLE[edge];

						if (vertIndex >= 0)
						{
							m_terrainIndexes.push_back((unsigned int)vertIndex);
							continue;
						}

						int cornerAIndex = edgeCornerPairIndexes.x;
						int cornerBIndex = edgeCornerPairIndexes.y;

						float t = GetFractionWithinRange(0.f, cubeDensityValues[cornerAIndex], cubeDensityValues[cornerBIndex]);
						Vec3 vertexPos = Lerp(globalCorners[cornerAIndex], globalCorners[cornerBIndex], t);
						Vec3 vertexNormal = GetNormalAtPosition(vertexPos);

						float interpolatedBiomeScores[(int)Biome::NUM_BIOMES] = {};
						for (int biomeIndex = 0; biomeIndex < (int)Biome::NUM_BIOMES; ++biomeIndex)
						{
							interpolatedBiomeScores[biomeIndex] = Lerp(cornerBiomeScores[cornerAIndex][biomeIndex], cornerBiomeScores[cornerBIndex][biomeIndex], t);
						}

						BiomeSplat vertexBiomeSplat = BuildBiomeSplatFromScores(interpolatedBiomeScores);
						Vertex_Terrain terrainVert = GetTerrainVertFromBiomeSplat(vertexPos, vertexNormal, vertexBiomeSplat);

						vertIndex = (int)m_terrainVerts.size();
						m_terrainIndexes.push_back((unsigned int)vertIndex);
						m_terrainVerts.push_back(terrainVert);

						if (IsLocalCoordsInChunk(localCorners[localEdgeLookupIndex]))
						{
							IntVec3& vertIndexes = edgeXYZVertIndexes[edgeLookupIndex];

							if (edge == 0 || edge == 2 || edge == 4 || edge == 6)
							{
								vertIndexes.x = vertIndex;
							}
							else if (edge < 8)
							{
								vertIndexes.y = vertIndex;
							}
							else if (edge < 12)
							{
								vertIndexes.z = vertIndex;
							}
						}
					}
				}

				idXY++;
			}
		}
	}
}

void Chunk::CreateVegetation(std::vector<TerrainNoiseValues2D> const& terrainNoiseValuesXY, std::vector<VegetationNoiseValues2D> const& vegetationNoiseValuesXY,std::vector<int> const& highestCrossingVoxelIndexXY, std::vector<BiomeSplat> const& highestCrossingBiomeSplatXY)
{
	WorldDefinition const* worldDef = m_world->m_definition;
	if (worldDef->m_activeVegetation)
	{
		const int VOXEL_SCALE = worldDef->m_voxelScale;
		const int VOXEL_AREA = VOXEL_SCALE * VOXEL_SCALE;
		constexpr float MIN_VEGETATION_WEIGHT = 0.1f;

		std::vector<PackedVegetationDisc> packedDiscs;
		packedDiscs.reserve(5000);

		int idXY = 0;
		for (int rowNum = 0; rowNum < CHUNK_SIZE_Y; ++rowNum)
		{
			if(g_jobSystem->IsAborting())
				return;

			int baseIndex2D = ((rowNum + 1) * NUM_NOISE_VOXELS_X) + 1;
			for (int columnNum = 0; columnNum < CHUNK_SIZE_X; ++columnNum)
			{
				int noiseIndex2D = baseIndex2D + columnNum;
				//Determine if there is even a density crossing on this vertical column
				//----------------------------------------------------------------------------------
				int crossingVoxelIndex = highestCrossingVoxelIndexXY[idXY];
				if (crossingVoxelIndex < 0)
				{
					idXY++;
					continue;
				}

				//Determine if vegetation can grow here with temperature
				//----------------------------------------------------------------------------------
				float temperature = terrainNoiseValuesXY[noiseIndex2D].m_values[(int)TerrainNoiseType::TEMPERATURE];
				float vegetationPotential = GetVegetationPotential(temperature);
				if (vegetationPotential <= 0)
				{
					idXY++;
					continue;
				}


				//Accumulate vegetation weights for this biome and continue if no weights > 0
				//----------------------------------------------------------------------------------
				BiomeSplat const& biomeSplat = highestCrossingBiomeSplatXY[idXY];
				
				float vegetationWeights[(int)VegetationType::COUNT] = {};
				float totalVegetationWeight = GetVegetationWeightsFromBiomeSplat(biomeSplat, vegetationWeights);
				if (totalVegetationWeight <= MIN_VEGETATION_WEIGHT)
				{
					idXY++;
					continue;
				}

				//Scale vegetation weights by cluster mask and vegetation potential
				//----------------------------------------------------------------------------------
				IntVec3 localCoords = GetLocalCoordsFromIndex(crossingVoxelIndex);
				Vec3 voxelPos = GetGlobalPosFromLocalCoords(localCoords);
				totalVegetationWeight = 0.f;
				for (int vegetationTypeIndex = 0; vegetationTypeIndex < (int)VegetationType::COUNT; ++vegetationTypeIndex)
				{
					if (vegetationWeights[vegetationTypeIndex] <= 0.0f)
					{
						continue;
					}

					if (!worldDef->m_vegetationInfos[vegetationTypeIndex].m_isActive)
					{
						vegetationWeights[vegetationTypeIndex] = 0.f;
						continue;
					}

					VegetationType vegetationType = (VegetationType)vegetationTypeIndex;
					VegetationNoiseType vegNoiseType = GetVegetationNoiseTypeFromVegetationType(vegetationType);
					float vegetationNoise = vegetationNoiseValuesXY[noiseIndex2D].m_values[(int)vegNoiseType];
					float vegetationMask = GetVegetationClusterMask(vegetationNoise, vegNoiseType);

					vegetationWeights[vegetationTypeIndex] *= vegetationMask;
					vegetationWeights[vegetationTypeIndex] *= vegetationPotential; //#TODO may want to remove this
					totalVegetationWeight += vegetationWeights[vegetationTypeIndex];
				}

				//Normalize vegetation weights
				//----------------------------------------------------------------------------------
				float localCoverage = totalVegetationWeight;
				if (totalVegetationWeight > MIN_VEGETATION_WEIGHT)
				{
					for (int vegetationTypeIndex = 0; vegetationTypeIndex < (int)VegetationType::COUNT; ++vegetationTypeIndex)
					{
						vegetationWeights[vegetationTypeIndex] /= totalVegetationWeight;
					}
				}

				if (totalVegetationWeight <= MIN_VEGETATION_WEIGHT)
				{
					idXY++;
					continue;
				}


				//Determine spawn attempts from normalized weights
				//----------------------------------------------------------------------------------
				float blendedMaxInstanceCount = 0.0f;
				for (int vegetationTypeIndex = 0; vegetationTypeIndex < (int)VegetationType::COUNT; ++vegetationTypeIndex)
				{
					float normalizedWeight = vegetationWeights[vegetationTypeIndex];
					if (normalizedWeight <= MIN_VEGETATION_WEIGHT)
					{
						continue;
					}

					VegetationInfo const& vegetationInfo = worldDef->m_vegetationInfos[(int)vegetationTypeIndex];
					float instanceRadius = vegetationInfo.m_instanceRadius;
					if (instanceRadius <= 0.0f)
					{
						continue;
					}

					float instanceArea = (instanceRadius * instanceRadius);
					float maxCountForThisType = (VOXEL_AREA / instanceArea);
					blendedMaxInstanceCount += (normalizedWeight * maxCountForThisType);
				}

				float spawnAttemptsFloat = (localCoverage * blendedMaxInstanceCount);
				int spawnAttempts = RoundDownToInt(spawnAttemptsFloat);
				float fractionalSpawnAttempt = (spawnAttemptsFloat - (float)spawnAttempts);

				float randomValue = GetDeterministicRandom01((uint32_t)crossingVoxelIndex);
				if (randomValue < fractionalSpawnAttempt)
				{
					spawnAttempts++;
				}

				if (spawnAttempts <= 0)
				{
					idXY++;
					continue;
				}


				//Loop through spawn attempts
				//----------------------------------------------------------------------------------
				for (int spawnAttemptIndex = 0; spawnAttemptIndex < spawnAttempts; ++spawnAttemptIndex)
				{
					uint32_t seed = (uint32_t)crossingVoxelIndex;
					seed ^= (uint32_t)(spawnAttemptIndex * 73856093);

					float typeChoiceRandom = GetDeterministicRandom01(seed);

					VegetationType chosenVegetationType = VegetationType::UN_INITIALIZED;
					float runningWeight = 0.0f;
					for (int vegetationTypeIndex = 0; vegetationTypeIndex < (int)VegetationType::COUNT; ++vegetationTypeIndex)
					{
						float normalizedWeight = vegetationWeights[vegetationTypeIndex];
						if (normalizedWeight <= 0.0f)
						{
							continue;
						}

						runningWeight += normalizedWeight;
						if (typeChoiceRandom <= runningWeight)
						{
							chosenVegetationType = (VegetationType)vegetationTypeIndex;
							break;
						}
					}

					if (chosenVegetationType == VegetationType::UN_INITIALIZED)
					{
						continue;
					}

					VegetationInfo const& vegetationInfo = worldDef->m_vegetationInfos[(int)chosenVegetationType];
					if (!vegetationInfo.m_isActive)
					{
						continue;
					}

					float candidateRadius = vegetationInfo.m_instanceRadius;
					if (candidateRadius <= 0.0f)
					{
						continue;
					}

					uint32_t offsetSeedX = seed ^ 0x68bc21ebu;
					uint32_t offsetSeedY = seed ^ 0x02e5be93u;

					float randomOffsetX01 = GetDeterministicRandom01(offsetSeedX);
					float randomOffsetY01 = GetDeterministicRandom01(offsetSeedY);

					float offsetX = ((randomOffsetX01 - 0.5f) * VOXEL_SCALE);
					float offsetY = ((randomOffsetY01 - 0.5f) * VOXEL_SCALE);

					Vec2 candidateCenter = voxelPos.GetXY();
					candidateCenter.x += offsetX;
					candidateCenter.y += offsetY;

					Vec2 voxelCenter = voxelPos.GetXY();
					float voxelHalfExtent = ((float)VOXEL_SCALE * 0.5f);

					if (candidateCenter.x < (voxelCenter.x - voxelHalfExtent) || candidateCenter.x >(voxelCenter.x + voxelHalfExtent))
					{
						continue;
					}

					if (candidateCenter.y < (voxelCenter.y - voxelHalfExtent) || candidateCenter.y >(voxelCenter.y + voxelHalfExtent))
					{
						continue;
					}

					bool overlapsExistingDisc = false;
					for (int packedDiscIndex = 0; packedDiscIndex < (int)packedDiscs.size(); ++packedDiscIndex)
					{
						Vec2 displacementToPackedDisc = (candidateCenter - packedDiscs[packedDiscIndex].m_center);
						float distanceSquared = displacementToPackedDisc.GetLengthSquared();
						float requiredSeparation = (candidateRadius + packedDiscs[packedDiscIndex].m_radius);
						if (distanceSquared < (requiredSeparation * requiredSeparation))
						{
							overlapsExistingDisc = true;
							break;
						}
					}

					if (overlapsExistingDisc)
					{
						continue;
					}

					Vec3 surfacePosition;
					Vec3 surfaceNormal;
					bool foundSurface = GetSurfacePosAndNormalAtXY(candidateCenter, crossingVoxelIndex, surfacePosition, surfaceNormal);
					if (!foundSurface)
					{
						continue;
					}

					bool didAddVegetation = TryAddVegetationAtSurfacePoint(surfacePosition, surfaceNormal, chosenVegetationType);
					if (!didAddVegetation)
					{
						continue;
					}

					PackedVegetationDisc packedDisc;
					packedDisc.m_center = surfacePosition.GetXY();
					packedDisc.m_radius = candidateRadius;
					packedDisc.m_type = chosenVegetationType;
					packedDiscs.push_back(packedDisc);
				}

				idXY++;
			}
		}
	}
}

bool Chunk::TryGenerateMeshBuffers()
{
	if (m_state != ChunkState::PENDING_MESH_BUFFERS)
		return false;

	if (m_terrainVerts.size() > 0 && m_terrainIndexes.size() > 0)
	{
		std::string chunkLabel = Stringf(" Chunk: (%i, %i, %i)", m_chunkCoords.x, m_chunkCoords.y, m_chunkCoords.z);
		m_terrainVbo = g_renderer->CreateVertexBuffer(m_terrainVerts, "Terrain_VBO" + chunkLabel);
		m_terrainIbo = g_renderer->CreateIndexBuffer(m_terrainIndexes, "Terrain_IBO" + chunkLabel);
		m_numVerts = (int)m_terrainVerts.size();
		m_numIndexes = (int)m_terrainIndexes.size();
		m_terrainVerts.clear();
		m_terrainVerts.shrink_to_fit();
		m_terrainIndexes.clear();
		m_terrainIndexes.shrink_to_fit();
		m_state = ChunkState::READY_TO_ACTIVATE;
		return true;
	}

	else
	{
		m_terrainVerts.clear();
		m_terrainVerts.shrink_to_fit();
		m_terrainIndexes.clear();
		m_terrainIndexes.shrink_to_fit();
		m_state = ChunkState::READY_TO_ACTIVATE;
	}

	return false;
	
}

bool Chunk::TryGenerateVegetationBuffers()
{
	if(!IsMeshChunk() || m_dispatchedVegetation)
		return false;

	WorldDefinition const* worldDef = m_world->m_definition;
	if(!worldDef->m_activeVegetation)
		return false;

	std::string chunkLabel = Stringf(" Chunk: (%i, %i, %i)", m_chunkCoords.x, m_chunkCoords.y, m_chunkCoords.z);

	for (int i = 0; i < (int)VegetationType::COUNT; ++i)
	{
		VegetationType vegType = (VegetationType)i;
		VegetationInfo const& vegInfo = worldDef->m_vegetationInfos[i];
		if(!vegInfo.m_isActive || vegInfo.m_type == VegetationType::UN_INITIALIZED)
			continue;

		VegetationBufferInfo& bufferInfo = m_vegetationBufferInfos[i];
		if(bufferInfo.m_terrainVerts.size() <= 0)
			continue;

		bufferInfo.m_instanceCount = (unsigned int)bufferInfo.m_terrainVerts.size();

		std::string vegetationLabel = GetVegetationTypeNameFromType(vegType);

		size_t stride = (IsVegetationTypeCoral(vegType) || vegType == VegetationType::KELP) ? sizeof(CoralInstance) : sizeof(GrassInstance);

		bufferInfo.m_instanceSBO = g_renderer->CreateEmptyStructuredBuffer(bufferInfo.m_instanceCount, stride, vegetationLabel + "Instance_SBO" + chunkLabel, true);
		bufferInfo.m_terrainSBO = g_renderer->CreateStructuredBuffer(bufferInfo.m_terrainVerts, true, vegetationLabel + "Terrain_SBO" + chunkLabel);

		uint32_t groupsX = (bufferInfo.m_instanceCount + 63u) / 64u;

		GrassConstants cb = {};
		cb.vertexCount = bufferInfo.m_instanceCount;

		g_renderer->BindComputeShader(vegInfo.m_computeShader);
		g_renderer->SetComputeConstantBufferData((uint32_t)ComputeRootParameters::CBV, cb);

		g_renderer->SetComputeSRV((uint32_t)ComputeRootParameters::SRV, 0, bufferInfo.m_terrainSBO, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, nullptr);

		D3D12_UNORDERED_ACCESS_VIEW_DESC instancesUavDesc = {};
		instancesUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		instancesUavDesc.Format = DXGI_FORMAT_UNKNOWN;
		instancesUavDesc.Buffer.FirstElement = 0;
		instancesUavDesc.Buffer.NumElements = bufferInfo.m_instanceCount;
		instancesUavDesc.Buffer.StructureByteStride = (UINT)stride;
		instancesUavDesc.Buffer.CounterOffsetInBytes = 0;
		instancesUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		g_renderer->SetComputeUAV((uint32_t)ComputeRootParameters::UAV, 0, bufferInfo.m_instanceSBO, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &instancesUavDesc);

		g_renderer->Dispatch(groupsX, 1, 1);

		bufferInfo.m_terrainVerts.clear();
		bufferInfo.m_terrainVerts.shrink_to_fit();

	}

	m_dispatchedVegetation = true;
	return true;
}

void Chunk::InitNeighbors()
{
	
	//Assign all neighbors and make sure they assign me as a neighbor
	for (int i = 0; i < (int)TerrainDirection::NUM_DIRECTIONS; ++i)
	{
		m_neighbors[i] = nullptr;

		Chunk* neighbor = m_world->GetChunkFromChunkCoords(m_chunkCoords + CHUNK_NEIGHBOR_DIRECTION_OFFSETS[i]);

		// Only keep ACTIVE neighbors
		if (neighbor && neighbor->m_state == ChunkState::ACTIVE)
			m_neighbors[i] = neighbor;
	}

	for (int i = 0; i < (int)TerrainDirection::NUM_DIRECTIONS; ++i)
	{
		if (!m_neighbors[i] || m_neighbors[i]->m_state != ChunkState::ACTIVE)
			continue;

		TerrainDirection direction = (TerrainDirection)i;

		switch (direction)
		{
		case TerrainDirection::NORTH: m_neighbors[i]->AddNeighbor(this, TerrainDirection::SOUTH);
			break;
		case TerrainDirection::EAST: m_neighbors[i]->AddNeighbor(this, TerrainDirection::WEST);
			break;
		case TerrainDirection::SOUTH: m_neighbors[i]->AddNeighbor(this, TerrainDirection::NORTH);
			break;
		case TerrainDirection::WEST: m_neighbors[i]->AddNeighbor(this, TerrainDirection::EAST);
			break;
		case TerrainDirection::UP: m_neighbors[i]->AddNeighbor(this, TerrainDirection::DOWN);
			break;
		case TerrainDirection::DOWN: m_neighbors[i]->AddNeighbor(this, TerrainDirection::UP);
			break;
		default:
			break;
		}
	}
}

void Chunk::AddNeighbor(Chunk* neighborChunk, TerrainDirection direction)
{
	m_neighbors[(int)direction] = neighborChunk;
}

void Chunk::RemoveNeighbor(TerrainDirection direction)
{
	m_neighbors[(int)direction] = nullptr;
}

void Chunk::RemoveNeighborIfMatch(Chunk* neighbor)
{
	if(!neighbor)
		return;

	for (int i = 0; i < (int)TerrainDirection::NUM_DIRECTIONS; ++i)
	{
		if (neighbor == m_neighbors[i])
		{
			m_neighbors[i] = nullptr;
		}
	}
}

void Chunk::ClearNeighbors()
{
	for (int i = 0; i < (int)TerrainDirection::NUM_DIRECTIONS; ++i)
	{
		if (m_neighbors[i])// && m_neighbors[i]->m_state == ChunkState::ACTIVE)
		{
			m_neighbors[i]->RemoveNeighborIfMatch(this);
			/*
			TerrainDirection direction = (TerrainDirection)i;
			switch (direction)
			{
			case TerrainDirection::NORTH: m_neighbors[i]->RemoveNeighbor(TerrainDirection::SOUTH);
				break;
			case TerrainDirection::EAST: m_neighbors[i]->RemoveNeighbor(TerrainDirection::WEST);
				break;
			case TerrainDirection::SOUTH: m_neighbors[i]->RemoveNeighbor(TerrainDirection::NORTH);
				break;
			case TerrainDirection::WEST: m_neighbors[i]->RemoveNeighbor(TerrainDirection::EAST);
				break;
			case TerrainDirection::UP: m_neighbors[i]->RemoveNeighbor(TerrainDirection::DOWN);
				break;
			case TerrainDirection::DOWN: m_neighbors[i]->RemoveNeighbor(TerrainDirection::UP);
				break;
			default:
				break;
			}
			*/
		
		}

		m_neighbors[i] = nullptr;
	}
}

//Helpers
//--------------------------------------------------------------------------------------------
float Chunk::GetDensityAtPosition(Vec3 const& pos) const
{
	WorldDefinition const* worldDef = m_world->m_definition;

	// Your current behavior: fall back to procedural density
	if (m_isSolidChunk || m_isEmptyChunk)
	{
		return worldDef->GetCumulativeDensityAtPosition(pos);
	}

	float voxelScale = (float)worldDef->m_voxelScale;
	float invVoxelScale = worldDef->m_invVoxelScale;

	// Convert world pos -> local voxel coords (base corner)
	Vec3 rel = (pos - m_chunkBounds.m_mins);

	int baseX = (int)floorf(rel.x * invVoxelScale);
	int baseY = (int)floorf(rel.y * invVoxelScale);
	int baseZ = (int)floorf(rel.z * invVoxelScale);

	// Need 8 corners fully inside the chunk to sample from m_voxels
	if (baseX < 0 || baseX >= (CHUNK_SIZE_X - 1)
		|| baseY < 0 || baseY >= (CHUNK_SIZE_Y - 1)
		|| baseZ < 0 || baseZ >= (CHUNK_SIZE_Z - 1))
	{
		return worldDef->GetCumulativeDensityAtPosition(pos);
	}

	// If you want to avoid edge access and keep your old "interior only" behavior:
	if (baseX < 1 || baseX >(CHUNK_SIZE_X - 2)
		|| baseY < 1 || baseY >(CHUNK_SIZE_Y - 2)
		|| baseZ < 1 || baseZ >(CHUNK_SIZE_Z - 2))
	{
		return worldDef->GetCumulativeDensityAtPosition(pos);
	}

	// Base corner world position
	Vec3 basePos(
		(m_chunkBounds.m_mins.x + ((float)baseX * voxelScale)),
		(m_chunkBounds.m_mins.y + ((float)baseY * voxelScale)),
		(m_chunkBounds.m_mins.z + ((float)baseZ * voxelScale))
	);

	// Normalized interpolation weights inside the cell
	float tX = GetClamped((pos.x - basePos.x) * invVoxelScale, 0.f, 1.f);
	float tY = GetClamped((pos.y - basePos.y) * invVoxelScale, 0.f, 1.f);
	float tZ = GetClamped((pos.z - basePos.z) * invVoxelScale, 0.f, 1.f);

	// Sample 8 voxel corner densities using local coords offsets (0 or 1)
	IntVec3 c000(baseX, baseY, baseZ);
	IntVec3 c100(baseX + 1, baseY, baseZ);
	IntVec3 c010(baseX, baseY + 1, baseZ);
	IntVec3 c110(baseX + 1, baseY + 1, baseZ);
	IntVec3 c001(baseX, baseY, baseZ + 1);
	IntVec3 c101(baseX + 1, baseY, baseZ + 1);
	IntVec3 c011(baseX, baseY + 1, baseZ + 1);
	IntVec3 c111(baseX + 1, baseY + 1, baseZ + 1);

	int i000 = GetVoxelIndexFromLocalCoords(c000);
	int i100 = GetVoxelIndexFromLocalCoords(c100);
	int i010 = GetVoxelIndexFromLocalCoords(c010);
	int i110 = GetVoxelIndexFromLocalCoords(c110);
	int i001 = GetVoxelIndexFromLocalCoords(c001);
	int i101 = GetVoxelIndexFromLocalCoords(c101);
	int i011 = GetVoxelIndexFromLocalCoords(c011);
	int i111 = GetVoxelIndexFromLocalCoords(c111);

	// If any fail, fall back (should not happen due to bounds checks)
	if (i000 < 0 || i100 < 0 || i010 < 0 || i110 < 0 ||
		i001 < 0 || i101 < 0 || i011 < 0 || i111 < 0)
	{
		return worldDef->GetCumulativeDensityAtPosition(pos);
	}

	int n = (int)m_voxels.size();
	if (i000 >= n || i100 >= n || i010 >= n || i110 >= n ||
		i001 >= n || i101 >= n || i011 >= n || i111 >= n)
	{
		return worldDef->GetCumulativeDensityAtPosition(pos);
	}

	float d000 = m_voxels[i000].GetDensity();
	float d100 = m_voxels[i100].GetDensity();
	float d010 = m_voxels[i010].GetDensity();
	float d110 = m_voxels[i110].GetDensity();
	float d001 = m_voxels[i001].GetDensity();
	float d101 = m_voxels[i101].GetDensity();
	float d011 = m_voxels[i011].GetDensity();
	float d111 = m_voxels[i111].GetDensity();

	// Trilinear interpolation
	float x00 = Lerp(d000, d100, tX);
	float x10 = Lerp(d010, d110, tX);
	float x01 = Lerp(d001, d101, tX);
	float x11 = Lerp(d011, d111, tX);

	float y0 = Lerp(x00, x10, tY);
	float y1 = Lerp(x01, x11, tY);

 	return Lerp(y0, y1, tZ);
}

Vec3 Chunk::GetNormalAtPosition(Vec3 const& pos, float epsilon) const
{
	Vec3 negativePos(pos - Vec3(epsilon, 0.f, 0.f));
	Vec3 positivePos(pos + Vec3(epsilon, 0.f, 0.f));
	float negativeDensity = GetDensityAtPosition(negativePos);
	float positiveDensity = GetDensityAtPosition(positivePos);
	float dx = negativeDensity - positiveDensity;

	negativePos = pos - Vec3(0.f,epsilon, 0.f);
	positivePos = pos + Vec3(0.f,epsilon, 0.f);
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

void Chunk::AddFish(Fish* fishToAdd)
{
	for (int i = 0; i < (int)m_fish.size(); ++i)
	{
		if (m_fish[i] == nullptr)
		{
			m_fish[i] = fishToAdd;
			return;
		}
	}

	m_fish.push_back(fishToAdd);
}

void Chunk::RemoveFish(Fish* fishToRemove)
{
	for (int i = 0; i < (int)m_fish.size(); ++i)
	{
		if (m_fish[i] == fishToRemove)
		{
			m_fish[i] = nullptr;
			return;
		}
	}
}

Vec2 Chunk::GetCenterXYCoords() const
{
	return m_chunkBounds.GetCenterPos().GetXY();
}

Vec3 Chunk::GetCenterPos() const
{
	return  m_chunkBounds.GetCenterPos();
}

void Chunk::AddVertAndIndexCount(int& out_numVerts, int& out_numIndexes) const
{
	out_numVerts += m_numVerts;
	out_numIndexes += m_numIndexes;
}

int Chunk::GetVoxelIndexFromLocalCoords(IntVec3 const& localCoords) const
{
	if (!IsLocalCoordsInChunk(localCoords))
	{
		return -1;
	}

	int index = localCoords.x + ((localCoords.y << BITS_STRIDE_Y) + (localCoords.z << BITS_STRIDE_Z));
	return index;
}

int Chunk::GetVoxelIndexFromGlobalPos(Vec3 const& pos) const
{
	IntVec3 localCoords = GetLocalCoordsFromGlobalPos(pos);
	return GetVoxelIndexFromLocalCoords(localCoords);
}


bool Chunk::GetSurfacePosAndNormalAtXY(Vec2 const& xyPosition, int crossingVoxelIndex, Vec3& out_surfacePosition, Vec3& out_surfaceNormal) const
{
	IntVec3 crossingLocalCoords = GetLocalCoordsFromIndex(crossingVoxelIndex);
	float const surfaceThreshold = 0.0f;

	for (int zOffset = -1; zOffset <= 1; ++zOffset)
	{
		int testLocalZ = (crossingLocalCoords.z + zOffset);
		if (testLocalZ < 0 || testLocalZ >= CHUNK_SIZE_Z)
		{
			continue;
		}

		IntVec3 bottomLocalCoords = IntVec3(crossingLocalCoords.x, crossingLocalCoords.y, testLocalZ);
		IntVec3 topLocalCoords = IntVec3(crossingLocalCoords.x, crossingLocalCoords.y, (testLocalZ + 1));

		Vec3 bottomWorldPos = GetGlobalPosFromLocalCoords(bottomLocalCoords);
		Vec3 topWorldPos = GetGlobalPosFromLocalCoords(topLocalCoords);

		bottomWorldPos.x = xyPosition.x;
		bottomWorldPos.y = xyPosition.y;
		topWorldPos.x = xyPosition.x;
		topWorldPos.y = xyPosition.y;

		float densityAtBottom = GetDensityAtPosition(bottomWorldPos);
		float densityAtTop = GetDensityAtPosition(topWorldPos);

		bool const hasCrossing = ((densityAtBottom >= surfaceThreshold) != (densityAtTop >= surfaceThreshold));
		if (!hasCrossing)
		{
			continue;
		}

		float minZ = bottomWorldPos.z;
		float maxZ = topWorldPos.z;
		float minDensity = densityAtBottom;
		float maxDensity = densityAtTop;

		for (int iterationNum = 0; iterationNum < 8; ++iterationNum)
		{
			float midZ = ((minZ + maxZ) * 0.5f);
			Vec3 midWorldPos = Vec3(xyPosition.x, xyPosition.y, midZ);
			float midDensity = GetDensityAtPosition(midWorldPos);

			bool const midMatchesMinSide = ((midDensity >= surfaceThreshold) == (minDensity >= surfaceThreshold));
			if (midMatchesMinSide)
			{
				minZ = midZ;
				minDensity = midDensity;
			}
			else
			{
				maxZ = midZ;
				maxDensity = midDensity;
			}
		}

		float surfaceZ = ((minZ + maxZ) * 0.5f);
		out_surfacePosition = Vec3(xyPosition.x, xyPosition.y, surfaceZ);
		out_surfaceNormal = GetNormalAtPosition(out_surfacePosition);
		return true;
	}

	return false;
}

bool Chunk::TryAddVegetationAtSurfacePoint(Vec3 const& surfacePos, Vec3 const& surfaceNormal, VegetationType vegType)
{
	WorldDefinition const* worldDef = m_world->m_definition;
	float seaLevel = worldDef->GetSeaLevel();

	VegetationInfo const& vegInfo = worldDef->m_vegetationInfos[(int)vegType];
	if (!vegInfo.m_isActive)
	{
		return false;
	}

	if (vegInfo.m_type == VegetationType::GRASS && surfacePos.z < seaLevel + 2.f)
	{
		return false;
	}

	else if (vegInfo.m_type != VegetationType::GRASS && surfacePos.z > seaLevel - 15.f)
	{
		return false;
	}

	else if (vegInfo.m_type == VegetationType::KELP)
	{
		float kelpHeight = GetMax(vegInfo.m_vegConstants.m_heightRange.m_min, vegInfo.m_vegConstants.m_heightRange.m_max);
		if (surfacePos.z > seaLevel - kelpHeight)
		{
			return false;
		}
	}

	else if (vegInfo.m_type == VegetationType::CORAL_SEA_FAN_01 || vegInfo.m_type == VegetationType::CORAL_SEA_FAN_02)
	{
		float seaFanHeight = SEAFAN_MESH_HEIGHT * GetMax(vegInfo.m_vegConstants.m_heightRange.m_min, vegInfo.m_vegConstants.m_heightRange.m_max);
		if (surfacePos.z > seaLevel - seaFanHeight)
		{
			return false;
		}
	}

	float worldDotUp = DotProduct3D(surfaceNormal, Vec3::UP);
	if (worldDotUp < vegInfo.m_worldDotUpThreshold)
	{
		return false;
	}


	VegetationBufferInfo& bufferInfo = m_vegetationBufferInfos[(int)vegType];
	bufferInfo.m_terrainVerts.push_back({ surfacePos, 0, surfaceNormal });

	if (vegType == VegetationType::SEA_ANEMONE)
	{
		float circleRadius = (1.1f - vegInfo.m_thickness) * ANEMONE_CLUSTER_RADIUS;
		std::vector<Vec3> numCirclePositions = Vec3::GetPositionInCircleAround(surfacePos, surfaceNormal, circleRadius, NUM_ANENOME_IN_CLUSTER);
		for (int i = 0; i < NUM_ANENOME_IN_CLUSTER; ++i)
		{
			Vec3 const& position = numCirclePositions[i];
			Vec3 circleSurfaceNormal =GetNormalAtPosition(position);

			bufferInfo.m_terrainVerts.push_back({ position, 0, circleSurfaceNormal });
		}
	}

	else if (vegType == VegetationType::SEA_GRASS || vegType == VegetationType::GRASS)
	{
		float circleRadius = (1.1f - vegInfo.m_thickness) * GRASS_CLUSTER_RADIUS;
		std::vector<Vec3> numCirclePositions = Vec3::GetPositionInCircleAround(surfacePos, surfaceNormal, circleRadius, NUM_GRASS_IN_CLUSTER);
		for (int i = 0; i < NUM_GRASS_IN_CLUSTER; ++i)
		{
			Vec3 const& position = numCirclePositions[i];
			Vec3 circleSurfaceNormal = GetNormalAtPosition(position);

			bufferInfo.m_terrainVerts.push_back({ position, 0, circleSurfaceNormal });
		}
	}

	return true;
}

TerrainNoiseValues2D Chunk::GetInterpolatedTerrainNoiseValuesAtPosition(Vec3 const& vertexPos, Vec3 const& chunkStartPos, float voxelScale, std::vector<TerrainNoiseValues2D> const& terrainNoiseValuesXY) const
{
	TerrainNoiseValues2D result = {};

	float const noiseX = (((vertexPos.x - chunkStartPos.x) / voxelScale) + 1.f);
	float const noiseY = (((vertexPos.y - chunkStartPos.y) / voxelScale) + 1.f);

	int x0 = (int)floorf(noiseX);
	int y0 = (int)floorf(noiseY);

	x0 = GetClampedInt(x0, 0, (NUM_NOISE_VOXELS_X - 2));
	y0 = GetClampedInt(y0, 0, (NUM_NOISE_VOXELS_Y - 2));

	int const x1 = (x0 + 1);
	int const y1 = (y0 + 1);

	float const tx = (noiseX - (float)x0);
	float const ty = (noiseY - (float)y0);

	int const idx00 = ((y0 * NUM_NOISE_VOXELS_X) + x0);
	int const idx10 = ((y0 * NUM_NOISE_VOXELS_X) + x1);
	int const idx01 = ((y1 * NUM_NOISE_VOXELS_X) + x0);
	int const idx11 = ((y1 * NUM_NOISE_VOXELS_X) + x1);

	TerrainNoiseValues2D const& n00 = terrainNoiseValuesXY[idx00];
	TerrainNoiseValues2D const& n10 = terrainNoiseValuesXY[idx10];
	TerrainNoiseValues2D const& n01 = terrainNoiseValuesXY[idx01];
	TerrainNoiseValues2D const& n11 = terrainNoiseValuesXY[idx11];

	for (int noiseIndex = 0; noiseIndex < NUM_2D_TERRAIN_NOISE_VALUES; ++noiseIndex)
	{
		float const v00 = n00.m_values[noiseIndex];
		float const v10 = n10.m_values[noiseIndex];
		float const v01 = n01.m_values[noiseIndex];
		float const v11 = n11.m_values[noiseIndex];
		float const vx0 = Lerp(v00, v10, tx);
		float const vx1 = Lerp(v01, v11, tx);
		result.m_values[noiseIndex] = Lerp(vx0, vx1, ty);
	}

	return result;
}

VegetationNoiseValues2D Chunk::GetInterpolatedVegetationNoiseValuesAtPosition(Vec3 const& vertexPos, Vec3 const& chunkStartPos, float voxelScale, std::vector<VegetationNoiseValues2D> const& vegetationNoiseValuesXY) const
{
	VegetationNoiseValues2D result = {};

	float const noiseX = (((vertexPos.x - chunkStartPos.x) / voxelScale) + 1.f);
	float const noiseY = (((vertexPos.y - chunkStartPos.y) / voxelScale) + 1.f);

	int x0 = (int)floorf(noiseX);
	int y0 = (int)floorf(noiseY);

	x0 = GetClampedInt(x0, 0, (NUM_NOISE_VOXELS_X - 2));
	y0 = GetClampedInt(y0, 0, (NUM_NOISE_VOXELS_Y - 2));

	int const x1 = (x0 + 1);
	int const y1 = (y0 + 1);

	float const tx = (noiseX - (float)x0);
	float const ty = (noiseY - (float)y0);

	int const idx00 = ((y0 * NUM_NOISE_VOXELS_X) + x0);
	int const idx10 = ((y0 * NUM_NOISE_VOXELS_X) + x1);
	int const idx01 = ((y1 * NUM_NOISE_VOXELS_X) + x0);
	int const idx11 = ((y1 * NUM_NOISE_VOXELS_X) + x1);

	VegetationNoiseValues2D const& n00 = vegetationNoiseValuesXY[idx00];
	VegetationNoiseValues2D const& n10 = vegetationNoiseValuesXY[idx10];
	VegetationNoiseValues2D const& n01 = vegetationNoiseValuesXY[idx01];
	VegetationNoiseValues2D const& n11 = vegetationNoiseValuesXY[idx11];

	for (int noiseIndex = 0; noiseIndex < (int)VegetationNoiseType::COUNT; ++noiseIndex)
	{
		float const v00 = n00.m_values[noiseIndex];
		float const v10 = n10.m_values[noiseIndex];
		float const v01 = n01.m_values[noiseIndex];
		float const v11 = n11.m_values[noiseIndex];
		float const vx0 = Lerp(v00, v10, tx);
		float const vx1 = Lerp(v01, v11, tx);
		result.m_values[noiseIndex] = Lerp(vx0, vx1, ty);
	}

	return result;
}

IntVec3 Chunk::GetLocalCoordsFromIndex(int index) const
{
	IntVec3 localCoords;
	localCoords.x = index & CHUNK_MASK_X;
	localCoords.y = (index >> CHUNK_BITS_X) & (CHUNK_SIZE_Y - 1);
	localCoords.z = (index >> (CHUNK_BITS_X + CHUNK_BITS_Y)) & (CHUNK_SIZE_Z - 1);
	return localCoords;
}

Vec3 Chunk::GetGlobalPosFromLocalCoords(IntVec3 const& localCoords) const
{
	WorldDefinition const* worldDef = m_world->m_definition;
	float voxelScale = (float)worldDef->m_voxelScale;

	return (m_chunkBounds.m_mins + Vec3(((float)localCoords.x * voxelScale), ((float)localCoords.y * voxelScale), ((float)localCoords.z * voxelScale)));
}

IntVec3 Chunk::GetLocalCoordsFromGlobalPos(Vec3 const& pos) const
{
	WorldDefinition const* worldDef = m_world->m_definition;
	float invVoxelScale = worldDef->m_invVoxelScale;

	Vec3 rel = (pos - m_chunkBounds.m_mins);

	int x = (int)floorf(rel.x * invVoxelScale);
	int y = (int)floorf(rel.y * invVoxelScale);
	int z = (int)floorf(rel.z * invVoxelScale);

	return IntVec3(x, y, z);
}

bool Chunk::IsPositionInChunk(Vec3 const& pos) const
{
	IntVec3 local = GetLocalCoordsFromGlobalPos(pos);
	return IsLocalCoordsInChunk(local);
}

bool Chunk::IsLocalCoordsInChunk(IntVec3 const& localCoords) const
{
	return (localCoords.x >= 0 && localCoords.x < CHUNK_SIZE_X) &&
		(localCoords.y >= 0 && localCoords.y < CHUNK_SIZE_Y) &&
		(localCoords.z >= 0 && localCoords.z < CHUNK_SIZE_Z);
}

bool Chunk::IsChunkInFrustrum(Frustum const& frustrum) const
{
	return IsAABB3inViewFrustum(m_chunkBounds, frustrum);
}

void Chunk::GetApproximateMemoryUsage(size_t& out_voxelSize, size_t& out_cpuMemorySize, size_t& out_gpuMemorySize) const
{
	out_cpuMemorySize = sizeof(*this);

	out_voxelSize = (sizeof(Voxel) * m_voxels.capacity()) + sizeof(m_voxels);

	out_cpuMemorySize += out_voxelSize;
	out_cpuMemorySize += (sizeof(TerrainVerts::value_type) * m_terrainVerts.capacity() + sizeof(m_terrainVerts));
	out_cpuMemorySize += (sizeof(IndexList::value_type) * m_terrainIndexes.capacity() + sizeof(m_terrainIndexes));
	out_cpuMemorySize += sizeof(m_terrainVbo);
	out_cpuMemorySize += sizeof(m_terrainIbo);
	out_cpuMemorySize += sizeof(m_debugVbo);
	out_cpuMemorySize += sizeof(Fish) * (int)m_fish.size();

	out_gpuMemorySize = 0;
	if(m_terrainVbo)
		out_gpuMemorySize += m_terrainVbo->GetSizeInBytes();
	if(m_terrainIbo)
		out_gpuMemorySize += m_terrainIbo->GetSizeInBytes();
	if(m_debugVbo)
		out_gpuMemorySize += m_debugVbo->GetSizeInBytes();
	for (int i = 0; i < (int)VegetationType::COUNT; ++i)
	{
		VegetationBufferInfo const& bufferInfo = m_vegetationBufferInfos[i];
		if(bufferInfo.m_instanceSBO)
			out_gpuMemorySize += sizeof(GrassInstance) * bufferInfo.m_instanceCount;
		if(bufferInfo.m_terrainSBO)
			out_gpuMemorySize += sizeof(GrassInstance) * bufferInfo.m_instanceCount;
	}

}

bool Chunk::IsPendingVegetation()
{
	return m_state == ChunkState::ACTIVE && IsMeshChunk() && !m_dispatchedVegetation;
}

float Chunk::DeterministicRandom01_FromGlobalCoords(Vec3 globalPos)
{
	int gameSeed = m_world->m_definition->m_gameSeed;

	uint64_t a = (uint64_t)(uint32_t)globalPos.x;
	uint64_t b = (uint64_t)(uint32_t)globalPos.y;
	uint64_t c = (uint64_t)(uint32_t)globalPos.z;

	uint64_t d = (uint64_t)(uint32_t)globalPos.x;
	uint64_t e = (uint64_t)(uint32_t)globalPos.y;
	uint64_t f = (uint64_t)(uint32_t)globalPos.z;

	uint64_t state = 0;
	state ^= (a * 0xD6E8FEB86659FD93ULL);
	state ^= (b * 0xA5A3564E27F2D5B1ULL);
	state ^= (c * 0x9E3779B97F4A7C15ULL);
	state ^= (d * 0xBF58476D1CE4E5B9ULL);
	state ^= (e * 0x94D049BB133111EBULL);
	state ^= (f * 0x2545F4914F6CDD1DULL);
	state ^= (uint64_t)gameSeed * 0x369DEA0F31A53F85ULL;

	uint64_t r = SplitMix64_Next(state);
	return U64ToUnitFloat01(r);
}


int Chunk::TryToDeleteFish(int numFishToDelete)
{
	int numFishDeleted = 0;
	for (int i = 0; i < (int)m_fish.size(); ++i)
	{
		if (m_fish[i])
		{
			m_fish[i]->m_isGarbage = true;
			numFishDeleted++;
			if(numFishDeleted >= numFishToDelete)
				break;
		}
	}

	return numFishDeleted;
}


