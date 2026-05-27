#include "Game/Fish.hpp"
#include "Game/Game.hpp"
#include "Game/World.hpp"
#include "Game/Chunk.hpp"
#include "Game/WorldDefinition.hpp"
#include "Engine/Renderer/RendererDX12.hpp"
#include "Engine/Renderer/VertexBufferDX12.hpp"
#include "Engine/Renderer/IndexBufferDX12.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Renderer/DebugRender.hpp"

Fish::Fish(World* world, Chunk* chunk, Vec3 const& position, FishType fishType)
	:m_world(world)
	,m_chunk(chunk)
	,m_type(fishType)
	,m_position(position)
{
	m_id = m_world->GetFishID();
	m_instanceData.m_fishID = m_id;

	WorldDefinition const& worldDef = m_world->m_unsavedDef;
	m_fishInfo = worldDef.m_fishInfos[(int)m_type];
	EulerAngles startOrientation = EulerAngles(g_rng->RollRandomFloatInRange(0.f, 360.f), g_rng->RollRandomFloatInRange(-20.f, 20.f), 0.f);
	m_fwrdNormal = startOrientation.Get_IFwd();

	m_velocity = m_fwrdNormal * m_fishInfo.m_maxSpeed * g_rng->RollRandomFloatZeroToOne();

}

Fish::~Fish()
{
}

void Fish::Update()
{
	m_fishInfo = m_world->m_unsavedDef.m_fishInfos[(int)m_type];
	m_density = m_world->GetDensityAtPosition(m_position);
	float radius = m_fishInfo.m_swimConstants.m_modelLength * 0.5f;
	//m_world->BounceSphereOffTerrain(m_position, m_velocity, radius, 0.5f);
	m_world->PushSphereOutOfTerrain(m_position, radius);
	float seaLevel = m_world->m_definition->GetSeaLevel();
	if (m_position.z > seaLevel - radius)
	{
		m_position.z = seaLevel - radius;
	}

	//m_world->BounceSphereOffTerrain(m_position, m_velocity, m_fishInfo.m_swimConstants.m_modelLength * 0.5f, 0.9f);

	m_instanceData.UpdateInfo(m_position, m_fwrdNormal, m_speed, m_fishInfo);
}


void Fish::RenderDebug(VertexBufferDX12* sphereVBO) const
{
	g_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_renderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_renderer->SetRasterizerMode(RasterizerMode::WIREFRAME_CULL_BACK);
	g_renderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP);
	g_renderer->BindRootSignature(nullptr);
	g_renderer->BindTexture(nullptr);
	g_renderer->BindShader(nullptr);

	Mat44 transform = Mat44::MakeTranslation3D(m_position);
	Mat44 flockingTransform = transform;
	flockingTransform.AppendScaleUniform3D(sqrtf(m_fishInfo.m_flockingRadiusSqrd));

	g_renderer->SetModelConstants(flockingTransform, Rgba8::GREEN);
	g_renderer->DrawVertexBuffer(sphereVBO, sphereVBO->GetNumberVerts());

	Mat44 separationTransform = transform;
	separationTransform.AppendScaleUniform3D(sqrtf(m_fishInfo.m_separationRadiusSqrd));

	g_renderer->SetModelConstants(separationTransform, Rgba8::RED);
	g_renderer->DrawVertexBuffer(sphereVBO, sphereVBO->GetNumberVerts());

	Mat44 radiusTransform = transform;
	radiusTransform.AppendScaleUniform3D(m_fishInfo.m_swimConstants.m_modelLength * 0.5f);

	g_renderer->SetModelConstants(radiusTransform, Rgba8::MAGENTA);
	g_renderer->DrawVertexBuffer(sphereVBO, sphereVBO->GetNumberVerts());


}


void Fish::UpdateFishBehavior(Vec3 const& alignment, Vec3 const& cohesion, Vec3 const& separation, Vec3 const& playerPos, float deltaSeconds)
{
	float turnSpeedT = 0.f;

	//Cohesion to player
	//--------------------------------------------------------------------------------------------------------------
	Vec3 diffFromPlayer = m_position - playerPos;
	float distFromPlayerSqrd = diffFromPlayer.GetLengthSquared();
	const float PLAYER_COHESION_STRENGTH = m_fishInfo.m_playerCohesion * 100.f;
	Vec3 playerCohesion = Vec3::ZERO;
	if (distFromPlayerSqrd >= m_fishInfo.m_flockingRadiusSqrd)
	{
		float t = GetClampedFractionWithinRange(distFromPlayerSqrd, 0.f, m_fishInfo.m_playerFlockingRadiusSqrd);
		Vec3 toPlayer = (playerPos - m_position).GetNormalized();
		playerCohesion = toPlayer * (PLAYER_COHESION_STRENGTH * t);
	}

	//Separation from player
	//--------------------------------------------------------------------------------------------------------------
	Vec3 playerSeparation = Vec3::ZERO;
	if (distFromPlayerSqrd <= m_fishInfo.m_playerSeparationRadiusSqrd)
	{
		Vec3 fromPlayer = (m_position - playerPos).GetNormalized();
		playerSeparation = fromPlayer * (m_fishInfo.m_playerSeparation * 1000.f);
		playerCohesion = Vec3::ZERO;

		float t = GetClampedFractionWithinRange(distFromPlayerSqrd, 0.f, m_fishInfo.m_playerSeparationRadiusSqrd);
		turnSpeedT = GetMax(t, turnSpeedT);
	}

	Vec3 fwrd = GetForwardNormal();
	Vec3 levelOutForce = Vec3::ZERO;
	//Level Out Force
	//--------------------------------------------------------------------------------------------------------------
	float dotWorldUp = DotProduct3D(fwrd, Vec3::UP);
	if (fabsf(dotWorldUp) > 0.5f)
	{
		levelOutForce = dotWorldUp * Vec3::DOWN * 50.f;
	}

	//Ocean Surface Avoidance
	//--------------------------------------------------------------------------------------------------------------
	const float SURFACE_AVOIDANCE_STRENGTH = m_fishInfo.m_terrainAvoidance * 75.f;
	const float MIN_Z = m_world->m_definition->GetSeaLevel() - m_fishInfo.m_minOceanDepth;
	Vec3 surfaceSeparation = Vec3::ZERO;
	if (m_position.z > MIN_Z)
	{
		float depthError = m_position.z - MIN_Z;
		surfaceSeparation = Vec3(0.f, 0.f, -1.f) * (depthError * 2.f);
		surfaceSeparation *= SURFACE_AVOIDANCE_STRENGTH;
		float t = GetClampedZeroToOne(depthError * m_fishInfo.m_minOceanDepth);
		levelOutForce *= 0.1f;// 1.0f - t;
		turnSpeedT = GetMax(t, turnSpeedT);
	}




	//Terrain Avoidance
	//--------------------------------------------------------------------------------------------------------------
	const float TERRAIN_AVOIDANCE_STRENGTH = m_fishInfo.m_terrainAvoidance * 200.f;
	Vec3 terrainSeparation = Vec3::ZERO;

	const float densityNearThreshold = -0.01f * m_fishInfo.m_probeDistance;
	const float densitySolidThreshold = 0.0f;

	float densityProximity = 0.0f;
	if (m_density > densityNearThreshold)
	{
		densityProximity = GetClampedFractionWithinRange(m_density, densityNearThreshold, densitySolidThreshold);
	}

	if (densityProximity > 0.f)
	{
		Vec3 terrainNormal = m_world->GetNormalAtPosition(m_position);
		float repulsionScale = SmoothStop3(densityProximity);

		if (densityProximity > 0.5f)
		{

			// Push away from terrain; ramp force with proximity
			terrainSeparation = terrainNormal * (TERRAIN_AVOIDANCE_STRENGTH * repulsionScale);

			// When close to terrain, allow faster turning
			turnSpeedT = GetMax(turnSpeedT, repulsionScale);
			surfaceSeparation *= 0.1f;
		}

		else
		{
			float projectedLength = DotProduct3D(m_velocity, terrainNormal);
			Vec3 vectorAlongNormal = terrainNormal * projectedLength;
			Vec3 vectorAlongPerpendicular = m_velocity - vectorAlongNormal;
			terrainSeparation = (vectorAlongPerpendicular - (terrainNormal)) * TERRAIN_AVOIDANCE_STRENGTH * repulsionScale;
		}
	}



	//Combine Forces
	//--------------------------------------------------------------------------------------------------------------
	Vec3 acceleration = alignment + cohesion + separation + playerCohesion + playerSeparation + surfaceSeparation + terrainSeparation + levelOutForce;

	m_velocity += acceleration * deltaSeconds;

	float speedSqrd = m_velocity.GetLengthSquared();
	float maxSpeed = m_fishInfo.m_maxSpeed;
	
	m_fwrdNormal = m_velocity.GetNormalized();

	if (speedSqrd > maxSpeed * maxSpeed)
	{
		m_velocity = m_fwrdNormal * maxSpeed;
	}


	m_speed = m_velocity.GetLength();

	m_speed = GetClamped(m_speed, m_fishInfo.m_maxSpeed * 0.3f, m_fishInfo.m_maxSpeed);

	// Always move forward at that speed
	m_velocity = (m_fwrdNormal * m_speed);

	// Integrate position ONCE
	m_position += (m_velocity * deltaSeconds);
	
}


void Fish::MigrateChunk(Chunk* newChunk)
{
	if (!newChunk)
	{
		m_isGarbage = true;
		return;
	}

	m_chunk->RemoveFish(this);
	m_chunk = newChunk;
	newChunk->AddFish(this);
}


