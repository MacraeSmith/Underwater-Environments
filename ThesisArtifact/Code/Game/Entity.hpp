#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Core/Rgba8.hpp"
class Game;
struct Mat44;
class Entity
{
public:
	Entity(Game* owner);
	explicit Entity(Game* owner, Vec3 const& position, EulerAngles const& orientation = EulerAngles::ZERO, float uniformScale = 1.f);
	virtual ~Entity();

	virtual void Update() = 0;
	virtual void Render() const = 0;

	virtual Mat44 GetModelToWorldTransform() const;
	virtual Vec3 GetForwardNormal() const;

public:
	Game* m_game = nullptr;
	Vec3 m_position;
	Vec3 m_velocity;
	EulerAngles m_orientationEuler;
	EulerAngles m_angularVelocity;
	Rgba8 m_color = Rgba8::WHITE;
	float m_uniformScale = 1.f;
};

