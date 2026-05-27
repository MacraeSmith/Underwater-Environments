#pragma once
#include "Game/Voxel.hpp"
#include "Game/WorldUtils.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/IntVec2.hpp"
#include <atomic>

class StructuredBufferDX12;
class StructuredBufferViewDX12;
class World;
class ChunkGenerateJob;
struct Vec3;
struct Frustum;
enum class TerrainDirection : int;
class Fish;

enum class ChunkState : int
{
	INACTIVE = -1,
	PENDING_GENERATE,
	GENERATING,
	PENDING_MESH_BUFFERS,
	READY_TO_ACTIVATE,
	ACTIVE,
	GARBAGE,
	COUNT
};


struct VegetationBufferInfo
{
	std::vector<VegetationVertex> m_terrainVerts;
	StructuredBufferDX12* m_terrainSBO = nullptr;
	StructuredBufferDX12* m_instanceSBO = nullptr;
	unsigned int m_instanceCount = 0;
	bool m_shouldRender = false;
};


class Chunk 
{
friend class World;
friend class ChunkGenerateJob;
public:
	explicit Chunk(World* world, IntVec3 const& chunkCoords);
	~Chunk();

	void Update(Frustum const& cameraFrustum);
	void UpdateFish();
	void UpdateGarbageFish();
	void RenderDebug() const;
	void RenderTerrain() const;
	void RenderVegetation(VegetationType vegetationType) const;
	void RenderWaterSurface() const;
	void GatherFishRenderData(std::vector<std::vector<FishInstanceData>>& fishDataGroups);
	void RenderDebugFish(VertexBufferDX12* sphereVBO) const;


	void InitNeighbors();
	void AddNeighbor(Chunk* neighborChunk, TerrainDirection direction);
	void RemoveNeighbor(TerrainDirection direction);
	void RemoveNeighborIfMatch(Chunk* neighbor);

	//Helpers
	//------------------------------------
	float	GetDensityAtPosition(Vec3 const& pos) const;

	Vec2	GetCenterXYCoords() const;
	Vec3	GetCenterPos() const;
	void	AddVertAndIndexCount(int& out_numVerts, int& out_numIndexes) const;
	Vec3	GetNormalAtPosition(Vec3 const& pos, float epsilon = .5f) const;
	bool	IsMeshChunk() const {return !m_isEmptyChunk && !m_isSolidChunk;}

	void AddFish(Fish* fishToAdd);
	void RemoveFish(Fish* fishToRemove);

private:
	
	//World Creation
	//------------------------------------
	void CreateVoxels(std::vector<TerrainNoiseValues2D>& terrainNoiseValuesXY, std::vector<VegetationNoiseValues2D>& vegetationNoiseValuesXY);
	void TrySpawnFish();
	void CreateTerrain(std::vector<int>& highestCrossingVoxelIndexXY, std::vector<BiomeSplat>& highestCrossingBiomeSplatXY,
		std::vector<TerrainNoiseValues2D> const& terrainNoiseValuesXY, std::vector<VegetationNoiseValues2D> const& vegetationNoiseValuesXY);

	void CreateVegetation(std::vector<TerrainNoiseValues2D> const& terrainNoiseValuesXY, std::vector<VegetationNoiseValues2D> const& vegetationNoiseValuesXY, 
		std::vector<int> const& highestCrossingVoxelIndexXY, std::vector<BiomeSplat> const& highestCrossingBiomeSplatXY);
	bool TryGenerateMeshBuffers();
	bool TryGenerateVegetationBuffers();
	void ClearNeighbors();

	//Vegetation Helpers
	//------------------------------------
	bool	GetSurfacePosAndNormalAtXY(Vec2 const& xyPosition, int crossingVoxelIndex, Vec3& out_surfacePosition, Vec3& out_surfaceNormal) const;

	bool	TryAddVegetationAtSurfacePoint(Vec3 const& surfacePos, Vec3 const& surfaceNormal, VegetationType vegType);
	TerrainNoiseValues2D GetInterpolatedTerrainNoiseValuesAtPosition(Vec3 const& vertexPos, Vec3 const& chunkStartPos, float voxelScale, std::vector<TerrainNoiseValues2D> const& terrainNoiseValuesXY) const;
	VegetationNoiseValues2D GetInterpolatedVegetationNoiseValuesAtPosition(Vec3 const& vertexPos, Vec3 const& chunkStartPos, float voxelScale, std::vector<VegetationNoiseValues2D> const& vegetationNoiseValuesXY) const;


	//Helpers
	//------------------------------------

	int		GetVoxelIndexFromLocalCoords(IntVec3 const& localCoords) const;
	int		GetVoxelIndexFromGlobalPos(Vec3 const& pos) const;


	IntVec3 GetLocalCoordsFromIndex(int index) const;
	Vec3	GetGlobalPosFromLocalCoords(IntVec3 const& localCoords) const;
	IntVec3 GetLocalCoordsFromGlobalPos(Vec3 const& pos) const;

	bool	IsPositionInChunk(Vec3 const& pos) const;
	bool	IsLocalCoordsInChunk(IntVec3 const& localCoords) const;

	bool	IsChunkInFrustrum(Frustum const& frustrum) const;

	void	GetApproximateMemoryUsage(size_t& out_voxelSize, size_t& out_cpuMemorySize, size_t& out_gpuMemorySize) const;

	bool    IsPendingVegetation();
	float	DeterministicRandom01_FromGlobalCoords(Vec3 globalPos);
	int	    TryToDeleteFish(int numFishToDelete);



public:
	World* m_world = nullptr;
	IntVec3 m_chunkCoords = IntVec3::ZERO;
	AABB3 m_chunkBounds;

	std::vector<Voxel> m_voxels;
	Chunk* m_neighbors[6] = {};
	std::atomic<ChunkState> m_state = ChunkState::INACTIVE;

private:
	int m_numVerts = 0;
	int m_numIndexes = 0;

	bool m_isSolidChunk = false;
	bool m_isEmptyChunk = false;

	bool m_inFrustum = false;

	//Debug
	VertexBufferDX12* m_debugVbo = nullptr;
	IndexBufferDX12* m_debugIbo = nullptr;

	//water surface
	bool m_isSeaSurfaceChunk = false;
	VertexBufferDX12* m_waterSurfaceVbo = nullptr;
	IndexBufferDX12* m_waterSurfaceIbo = nullptr;

	//fish
	std::vector<Fish*> m_fish = {};


	//Terrain
	TerrainVerts m_terrainVerts;
	IndexList m_terrainIndexes;
	VertexBufferDX12* m_terrainVbo = nullptr;
	IndexBufferDX12* m_terrainIbo = nullptr;

	bool m_dispatchedVegetation = false;
	VegetationBufferInfo m_vegetationBufferInfos[(int)VegetationType::COUNT] = {};

};


