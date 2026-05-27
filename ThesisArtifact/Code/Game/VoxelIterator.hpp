#pragma once
class Chunk;
struct Voxel;

enum class TerrainDirection : int;

class VoxelIterator
{
public:
	VoxelIterator();
	VoxelIterator(Chunk* chunk, int voxelIndex);
	~VoxelIterator();

	VoxelIterator GetIterator(TerrainDirection direction) const;
	Voxel* GetVoxel() const;

public:
	Chunk* m_chunk = nullptr;
	int m_voxelIndex = -1;
};

