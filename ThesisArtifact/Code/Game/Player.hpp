#pragma once
#include "Game/Entity.hpp"
class Game;
class Camera;
struct Frustum;
class VertexBufferDX12;
struct CameraConstants;
enum class CameraMode : int
{
	FREE_FLY,
	NO_CLIP,
	WORLD_ALIGNED,
	FRUSTUM_CULL,
	NUM_CAMERA_MODES
};

class Player : public Entity
{
public:
	explicit Player(Game* owner);
	virtual ~Player();

	virtual void Update() override;
	virtual void Render() const override;
	Frustum GetViewFrustrum() const;
	CameraConstants GetCameraConstants() const;
	CameraMode const& GetCameraMode() const {return m_cameraMode;}

private:
	void CheckKeyboardInputControls(float deltaSeconds);
	void CheckControllerInputControls(float deltaSeconds);
	void CreateCompass();
	void ChangeCameraMode();
	void CheckTerrainCollision();

	char const*	GetTextForCameraMode() const;

public:
	Camera* m_camera = nullptr;
	bool m_flashlightOn = true;
private:
	CameraMode m_cameraMode = CameraMode::FREE_FLY;
	Vec3 m_compassPos;
	VertexBufferDX12* m_compassVbo = nullptr;
	float m_sprintSpeed = 20.f;
	float m_densityLastFrame = -1.f;
	Vec3 m_positionLastFrame;
	float m_collisionRadius = 2.5f;
};

