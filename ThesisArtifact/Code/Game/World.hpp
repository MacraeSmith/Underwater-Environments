#pragma once
#include "Game/WorldDefinition.hpp" 
#include "Game/VoxelIterator.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/IntRange.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/RendererDX12.hpp"
#include <unordered_map>
#include <vector>
#include <set>
#include <string>
#include <queue>
#include <unordered_set>

class Chunk;
struct Vec3;
struct LightConstants;
class Timer;
struct FloatRange;
class TextureDX12;
class ShaderDX12;
class Image;
enum class Biome: int;
struct Frustum;
struct LightRayConstants;
class VertexBufferDX12;
class IndexBufferDX12;

namespace std
{
	template<>
	struct hash<IntVec3>
	{
		size_t operator()(IntVec3 const& v) const noexcept
		{
			size_t h1 = std::hash<int>()(v.x);
			size_t h2 = std::hash<int>()(v.y);
			size_t h3 = std::hash<int>()(v.z);
			size_t seed = h1;
			seed ^= h2 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
			seed ^= h3 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
			return seed;
		}
	};
}

struct ChunkLoadCandidate
{
	IntVec3 coords;
	float   priority; // lower = higher priority

	bool operator<(const ChunkLoadCandidate& rhs) const { return priority > rhs.priority; }
};


class TerrainImageLoadJob : public Job
{
public:
	explicit TerrainImageLoadJob(std::string const& imageFilePath, Biome biome, bool isFloorFile, int textureIndex);
	virtual void Execute() override;

	std::string m_imageFilePath;
	Image* m_image = nullptr;
	Biome m_biome;
	bool m_isFloorFile = false;
	int m_textureIndex = 0;
};


class FishMeshLoadJob : public Job
{
public:
	explicit FishMeshLoadJob(std::string const& meshFolderPath, FishType fishType);
	virtual ~FishMeshLoadJob();
	virtual void Execute() override;

	std::string m_meshFolderPath;
	MeshImages m_outImages;
	VertTBNs m_outVerts;
	IndexList m_outIndexes;
	std::string m_outShaderPath;
	FishType m_fishType;
};

class VegetationMeshLoadJob : public Job
{
public:
	explicit VegetationMeshLoadJob(std::string const& meshFolderPath, std::string const& diffuseTextureName, std::string const& normalTextureName, VegetationType vegType);
	virtual ~VegetationMeshLoadJob();
	virtual void Execute() override;

	std::string m_meshFolderPath;
	MeshImages m_outImages;
	VertTBNs m_outVerts;
	IndexList m_outIndexes;
	std::string m_outShaderPath;
	VegetationType m_vegetationType;
	std::string m_diffuseTextureName;
	std::string m_normalTextureName;
	float m_meshRadius = 1.f;
	
};

class ChunkGenerateJob : public Job
{
public:
	explicit ChunkGenerateJob(Chunk* chunk);
	virtual void Execute() override;

	Chunk* m_chunk = nullptr;
	double m_timeStarted = 0.0;
};

enum class WorldState : int
{
	LOADING_RESOURCES,
	ACTIVE,
	COUNT,
};

enum class WorldDebugView : int
{
	NONE,
	MESH_CHUNK_BOUNDS,
	CHUNK_BOUNDS,
	WIRE_FRAME,
	DEBUG_FISH,
	COUNT,
};


struct FishBufferInfo
{
	StructuredBufferDX12* m_fishDataBuffer;
	unsigned int m_instanceCount = 0;
};

class World
{
public:
	explicit World(std::string const& worldName);
	~World();

	//Frame Flow
	//------------------------------------------------------------------------
	void AttractScreenUpdate();
	void ResourceLoadingUpdate();
	void Update();
	void Render() const;
	void RenderSkyBox() const;
	void RenderResourceLoading() const;
	void Shutdown();


	//Chunk Helpers
	//------------------------------------------------------------------------
	IntVec3		GetChunkCoordsFromPosition(Vec3 const& pos) const;
	Vec3		GetGlobalCenterPosFromChunkCoords(IntVec3 const& chunkCoords) const;
	Chunk*		GetChunkFromChunkCoords(IntVec3 const& chunkCoords) const;
	Chunk*		GetActiveChunkFromPosition(Vec3 const& pos) const;


	//Density and Terrain Helpers
	//------------------------------------------------------------------------
	float		GetDensityAtPosition(Vec3 const& pos) const;
	Vec3		GetNormalAtPosition(Vec3 const& pos) const;
	Vec3		GetBaseTerrainNormalAtPosition(Vec3 const& pos, float epsilon = 0.001f) const;
	void		PushSphereOutOfTerrain(Vec3& sphereCenter, float radius);
	void		BounceSphereOffTerrain(Vec3& out_sphereCenter, Vec3& out_velocity, float radius, float elasticity);
	bool		IsPlayerUnderwater() const;

	void		QueueChunkForVegetation(Chunk* chunk, IntVec3 const& chunkCoords);

	uint32_t GetFishID();
	int GetTotalNumFish() const;


	//Events
	//------------------------------------------------------------------------
	static bool Event_SunSettings(EventArgs& args);

private:

	void InitWorldObjects();
	//Update
	//------------------------------------------------------------------------
	void UpdateLighting();
	//Input
	void CheckKeyboardControls();
	void CheckControllerControls();
	//chunk loading
	void ProcessChunkLoading();
	void BuildChunkMeshBuffers();
	void BuildChunkVegetation();

	void UpdateSkyBox();
	void UpdateChunks();
	//Debug
	void UpdateDebugMessages();
	void UpdateImGui();
	void ManageMapContainers();
	void IterateDebugView();

	//Render
	//------------------------------------------------------------------------
	void RenderChunkDebug() const;
	void RenderChunkTerrain() const;
	void RenderChunkVegetation() const;
	void RenderFish() const;
	void RenderSun() const;
	void RenderWaterSurface(RenderTarget const& worldRT, RenderTarget const& terrainRT) const;
	void DispatchFogVolumeCompute() const;
	void RenderLightRayAndParticleMask(RenderTarget const& worldRT) const;
	void RenderFog(RenderTarget const& worldRT, TextureDX12 const* lightRaysMask) const;

	void BuildFishRenderData();

	//Chunk Management
	//------------------------------------------------------------------------
	void LoadChangedWorld();
	void LoadChunksInActivationRange();
	void UnloadChunksOutsideActivationRange();
	void AddNewChunkAtCoords(IntVec3 const& coords);
	void ActivateChunk(Chunk* chunk);
	void DeleteChunk(Chunk* chunk);

	//Chunk Helpers
	//-----------------------------------------------------------------------
	bool DoesChunkExist(IntVec3 const& chunkCoords) const;
	bool DoesChunkExist(IntVec3 const& chunkCoords, Chunk*& out_foundChunk) const;
	bool AreChunkCoordsInViewFrustum(IntVec3 const& coords, Frustum const& frustum) const;


	//Lighting
	//------------------------------------------------------------------------
	void	AdjustLightSettings(EventArgs& args);
	void	ResetSun();

	//Fish
	//------------------------------------------------------------------------
	int TryToDeleteFish(int numFishToDelete);


	//World Definitions
	//------------------------------------------------------------------------
	bool	TryLoadFromXML(std::string const& filePath);
	void	ResetWorldToDefault();
	void	SelectMostRecentSaves(int saveIndex);

	//Job System
	//------------------------------------------------------------------------
	float GetLoadingPercent() const;
	void ExecuteChunkGenerateJob(Chunk* chunk);
	void ExecuteImageLoadJob(std::string const& filePath, Biome biome, bool floorFile, int textureIndex);
	void ExecuteFishLoadJob(std::string const& filePath, FishType fishType);
	void ExecuteVegetationLoadJob(std::string const& filePath, VegetationType vegType);
	void LoadFishMeshInfo();
	void LoadTerrainImages(TextureFolderPaths const& folderPaths, Biome biome, bool floorFile);
	void LoadVegetationMeshInfo();
	void ProcessFinishedJobs();

public:
	WorldState m_state = WorldState::LOADING_RESOURCES;
	WorldDefinition const* m_definition = nullptr;
	WorldDebugView m_debugView = WorldDebugView::NONE;
	IntVec3 m_chunkSize;
	WorldDefinition m_unsavedDef;

	//std::atomic<int> m_totalNumFish = 0;
	std::atomic<int> m_totalNumFishByType[(int)FishType::COUNT] = {};

	uint32_t m_gameplayFrameCount = 0;
private:

	bool m_needsInitialization = true;
	//Chunk Management
	//------------------------------------------------------------------------
	Vec3 m_invChunkSize;
	IntRange m_chunkZRange;
	int m_numActiveChunks = 0;
	int m_maxActiveChunks = 0;
	

	std::vector<Chunk*> m_activeChunks;
	std::unordered_map<IntVec3, Chunk*> m_coordsChunkPair;
	std::unordered_map<IntVec3, Chunk*> m_pendingMeshBufferChunks;
	std::unordered_map<IntVec3, Chunk*> m_pendingVegetationChunks;

	bool m_loadChunks = true;
	std::unordered_set<IntVec3> m_checkedCoords;
	std::priority_queue<ChunkLoadCandidate> m_chunkLoadQueue;

	//Lighting
	//------------------------------------------------------------------------
	LightConstants* m_lightSettings = nullptr;
	VertexBufferDX12* m_sunVBO = nullptr;
	IndexBufferDX12* m_sunIBO = nullptr;

	Timer* m_debugSunTimer = nullptr;
	bool m_renderDebugSunDirection = false;

	TextureDX12* m_fogVolumeTexture = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE m_fogVolumeUAV;
	D3D12_CPU_DESCRIPTOR_HANDLE m_fogVolumeSRV;

	//Fish
	//------------------------------------------------------------------------
	std::atomic<uint32_t> m_nextFishID = 0;
	VertexBufferDX12* m_debugFishRadiusVBO = nullptr;
	FishBufferInfo m_fishBufferInfos[(int)FishType::COUNT] = {};


	//XML
	std::string m_loadXMLName;

	//Job Management
	//------------------------------------------------------------------------
	bool m_showJobInfo = false;
	std::set<Job*> m_activeJobs;
	int m_numTexturesLoaded = 0;
	int m_neededTexturesLoaded = 0;

	int m_numFishLoaded = 0;
	int m_neededFishLoaded = 0;

	int m_numVegetationLoaded = 0;
	int m_neededVegetationLoaded = 0;

	//Debug
	//------------------------------------------------------------------------
	bool m_showMemoryInfo = false;
	bool m_shouldRebuild = false;
	Timer* m_canRebuildTimer = nullptr;

	int64_t m_numCompletedMeshGenerateJobs = 0;
	double m_totalMeshGenerateSpeed = 0.0;
	double m_averageMeshGenerateSpeed = 0.0;

	int64_t m_numCompletedMeshlessGenerateJobs = 0;
	double m_totalMeshlessGenerateSpeed = 0.0;
	double m_averageMeshlessGenerateSpeed = 0.0;

	int64_t m_numMeshBufferGenerates = 0;
	double m_totalMeshBufferSpeed = 0.0;
	double m_averageMeshBufferSpeed = 0.0;

};

