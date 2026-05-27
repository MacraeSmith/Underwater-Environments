#include "Game/TerrainUtils.hpp"
#include "Game/GameCommon.hpp"
int GetNorthIndex(int voxelIndex)
{
	int y = (voxelIndex & CHUNK_MASK_Y) >> CHUNK_BITS_X;
	return (y + 1 >= CHUNK_SIZE_Y) ? -1 : voxelIndex + STRIDE_Y;
}

int GetEastIndex(int voxelIndex)
{
	int x = voxelIndex & CHUNK_MASK_X;
	return (x + 1 >= CHUNK_SIZE_X) ? -1 : voxelIndex + 1;
}

int GetSouthIndex(int voxelIndex)
{
	int y = (voxelIndex & CHUNK_MASK_Y) >> CHUNK_BITS_X;
	return (y - 1 < 0) ? -1 : voxelIndex - STRIDE_Y;
}

int GetWestIndex(int voxelIndex)
{
	int x = voxelIndex & CHUNK_MASK_X;
	return (x - 1 < 0) ? -1 : voxelIndex - 1;
}

int GetUpIndex(int voxelIndex)
{
	int z = (voxelIndex & CHUNK_MASK_Z) >> (CHUNK_BITS_X + CHUNK_BITS_Y);
	return (z + 1 >= CHUNK_SIZE_Z) ? -1 : voxelIndex + STRIDE_Z;
}

int GetDownIndex(int voxelIndex)
{
	int z = (voxelIndex & CHUNK_MASK_Z) >> (CHUNK_BITS_X + CHUNK_BITS_Y);
	return (z - 1 < 0) ? -1 : voxelIndex - STRIDE_Z;
}

void GetCardinalIndexes_NESWUD(int voxelIndex, int out_indexes[6])
{
	int x = voxelIndex & CHUNK_MASK_X;
	int y = (voxelIndex & CHUNK_MASK_Y) >> CHUNK_BITS_X;
	int z = (voxelIndex & CHUNK_MASK_Z) >> (CHUNK_BITS_X + CHUNK_BITS_Y);

	// +Y
	out_indexes[0] = (y + 1 >= CHUNK_SIZE_Y) ? -1 : voxelIndex + STRIDE_Y;

	// +X
	out_indexes[1] = (x + 1 >= CHUNK_SIZE_X) ? -1 : voxelIndex + 1;

	// -Y
	out_indexes[2] = (y - 1 < 0) ? -1 : voxelIndex - STRIDE_Y;

	// -X
	out_indexes[3] = (x - 1 < 0) ? -1 : voxelIndex - 1;

	// +Z
	out_indexes[4] = (z + 1 >= CHUNK_SIZE_Z) ? -1 : voxelIndex + STRIDE_Z;

	// -Z
	out_indexes[5] = (z - 1 < 0) ? -1 : voxelIndex - STRIDE_Z;
}

