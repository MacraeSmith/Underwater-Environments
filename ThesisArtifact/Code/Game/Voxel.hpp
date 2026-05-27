#pragma once
#include <cstdint>


struct Voxel
{
	float GetDensity() const;
	void SetDensity(float density);
	void SetBlockType(uint8_t type) { m_blockType = type; }
	bool HasSolid() const { return m_blockType != 0 || m_baseCornerDensity > 0.f; }

private:
	float m_baseCornerDensity = 0.f;
	uint8_t m_blockType = 0;
};



