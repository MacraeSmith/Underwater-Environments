#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Game/WorldUtils.hpp"
#include <vector>
class World;
class Chunk;
struct FishInfo;
class Fish
{
public:
	Fish(World* world, Chunk* chunk, Vec3 const& position, FishType fishType);
	virtual ~Fish();

	void Update();
	void RenderDebug(VertexBufferDX12* sphereVBO) const;
	void UpdateFishBehavior(Vec3 const& alignment, Vec3 const& cohesion, Vec3 const& separation, Vec3 const& playerPos, float deltaSeconds);
	void MigrateChunk(Chunk* newChunk);
	Vec3 GetForwardNormal() const {return m_fwrdNormal;}

	FishInstanceData GetInstanceData() const {return m_instanceData;}


public:
	bool m_isGarbage = false;
	FishType m_type = FishType::ANGEL_FISH;
	FishInfo m_fishInfo;
	uint32_t m_id = 0;
	Vec3 m_position;
	Vec3 m_velocity;

private:
	World* m_world = nullptr;
	Chunk* m_chunk = nullptr;
	Vec3 m_fwrdNormal;
	float m_speed = 0.f;
	float m_density = 1.f;
	FishInstanceData m_instanceData;
};

