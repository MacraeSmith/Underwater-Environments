#include "Game/Voxel.hpp"
#include "Game/WorldUtils.hpp"

float Voxel::GetDensity() const
{
	return m_baseCornerDensity;
}

void Voxel::SetDensity(float density)
{
	m_baseCornerDensity = density;
}
