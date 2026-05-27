#include "Game/VoxelIterator.hpp"
#include "Game/Chunk.hpp"
#include "Game/World.hpp"
#include "Game/GameCommon.hpp"
#include "Game/TerrainUtils.hpp"

VoxelIterator::VoxelIterator()
	:m_chunk(nullptr)
	,m_voxelIndex(-1)
{
}

VoxelIterator::VoxelIterator(Chunk* chunk, int voxelIndex)
	:m_chunk(chunk)
	,m_voxelIndex(voxelIndex)
{
	if (m_chunk && !m_chunk->IsMeshChunk())
	{
		m_voxelIndex = -1;
	}

}

VoxelIterator::~VoxelIterator()
{
	m_chunk = nullptr;
}

VoxelIterator VoxelIterator::GetIterator(TerrainDirection direction) const
{
	if (!m_chunk)
		return VoxelIterator(nullptr, -1);

	// Extract local coordinates safely
	int x = (m_voxelIndex >> BITS_STRIDE_X) & CHUNK_MAX_X;
	int y = (m_voxelIndex >> BITS_STRIDE_Y) & CHUNK_MAX_Y;
	int z = (m_voxelIndex >> BITS_STRIDE_Z) & CHUNK_MAX_Z;

	Chunk* nextChunk = m_chunk;

	switch (direction)
	{
	case TerrainDirection::EAST:
		if (x == CHUNK_MAX_X)
		{
			nextChunk = m_chunk->m_neighbors[(int)TerrainDirection::EAST];
			if (!nextChunk) return VoxelIterator(nullptr, -1);
			x = 0;
		}
		else x += 1;
		break;

	case TerrainDirection::WEST:
		if (x == 0)
		{
			nextChunk = m_chunk->m_neighbors[(int)TerrainDirection::WEST];
			if (!nextChunk) return VoxelIterator(nullptr, -1);
			x = CHUNK_MAX_X;
		}
		else x -= 1;
		break;

	case TerrainDirection::NORTH:
		if (y == CHUNK_MAX_Y)
		{
			nextChunk = m_chunk->m_neighbors[(int)TerrainDirection::NORTH];
			if (!nextChunk) return VoxelIterator(nullptr, -1);
			y = 0;
		}
		else y += 1;
		break;

	case TerrainDirection::SOUTH:
		if (y == 0)
		{
			nextChunk = m_chunk->m_neighbors[(int)TerrainDirection::SOUTH];
			if (!nextChunk) return VoxelIterator(nullptr, -1);
			y = CHUNK_MAX_Y;
		}
		else y -= 1;
		break;

	case TerrainDirection::UP:
		if (z == CHUNK_MAX_Z)
		{
			nextChunk = m_chunk->m_neighbors[(int)TerrainDirection::UP];
			if (!nextChunk) return VoxelIterator(nullptr, -1);
			z = 0;
		}
		else z += 1;
		break;

	case TerrainDirection::DOWN:
		if (z == 0)
		{
			nextChunk = m_chunk->m_neighbors[(int)TerrainDirection::DOWN];
			if (!nextChunk) return VoxelIterator(nullptr, -1);
			z = CHUNK_MAX_Z;
		}
		else z -= 1;
		break;

	default:
		break;
	}

	// Rebuild voxel index
	int newIndex =
		(x << BITS_STRIDE_X) +
		(y << BITS_STRIDE_Y) +
		(z << BITS_STRIDE_Z);

	return VoxelIterator(nextChunk, newIndex);
}

Voxel* VoxelIterator::GetVoxel() const
{
	if (m_chunk && m_chunk->m_state == ChunkState::ACTIVE && m_chunk->IsMeshChunk())
	{
		return &m_chunk->m_voxels[m_voxelIndex];
	}
	return nullptr;
}
