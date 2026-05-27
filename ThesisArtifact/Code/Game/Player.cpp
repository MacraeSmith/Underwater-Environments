#include "Game/Player.hpp"
#include "Game/Game.hpp"
#include "Game/World.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Input/XboxController.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/RendererDX12.hpp"
#include "Engine/Renderer/VertexBufferDX12.hpp"
#include "Engine/ImGui/ImGuiSystem.hpp"

Player::Player(Game* owner)
	:Entity(owner)
	, m_camera(new Camera())

{
	m_camera->SetCameraToRenderTransform(Mat44::IFWRD_JLEFT_KUP_TO_DX11RENDER);
	m_velocity = Vec3(50.f, 50.f, 50.f);
	m_sprintSpeed = 5.f;
	m_position = Vec3(0.f, 0.f, g_game->m_world->m_definition->GetSeaLevel() + 20.f);
	m_positionLastFrame = m_position;
	m_orientationEuler = EulerAngles(0.f, 15.f, 0.f);
	CreateCompass();
}

Player::~Player()
{
	delete(m_compassVbo);
	m_compassVbo = nullptr;
}

void Player::Update()
{
	float deltaSeconds = Clock::GetSystemClock().GetDeltaSeconds();
	CheckKeyboardInputControls(deltaSeconds);
	CheckControllerInputControls(deltaSeconds);

	Vec3 fwrd;

	if (m_cameraMode != CameraMode::FRUSTUM_CULL)
	{
		fwrd = m_orientationEuler.Get_IFwd();
		m_compassPos = m_position + (fwrd * 0.25f);
	}

	else
	{
		fwrd = m_camera->GetOrientation().Get_IFwd();
		m_compassPos = m_camera->GetPosition() + (fwrd * 0.25f);
	}

	if (g_showGameDebugMessages)
	{
		std::string text = "PlayerPos: " + m_position.GetAsText();
		DebugAddMessage(text, 0.f);
		DebugAddMessage(Stringf("Current Density: %.2f", m_densityLastFrame), 0.f, Rgba8::BLUE);
		DebugAddMessage(Stringf("Camera Mode [C]: %s", GetTextForCameraMode()), 0.f, Rgba8::ORANGE);

	}

}

void Player::Render() const
{
	if (m_cameraMode != CameraMode::FREE_FLY && g_showGameDebugMessages)
	{
		//Compass
		g_renderer->BeginRendererEvent("Draw - Compass");
		g_renderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
		g_renderer->SetBlendMode(BlendMode::OPAQUE);
		g_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_renderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP);
		Mat44 compassTransform = Mat44::MakeTranslation3D(m_compassPos);
		//compassTransform.AppendScaleUniform3D(100.f);
		g_renderer->SetModelConstants(compassTransform, Rgba8::WHITE);
		g_renderer->BindTexture(nullptr);
		g_renderer->BindShader(nullptr);
		g_renderer->DrawVertexBuffer(m_compassVbo, m_compassVbo->GetNumberVerts());
		g_renderer->EndRendererEvent();
	}

	if (m_cameraMode == CameraMode::FRUSTUM_CULL)
	{
		g_renderer->BeginRendererEvent("Draw - Player Camera");
		Verts verts;
		//AddVertsForCylinder3D(verts, Vec3(0.f, 0.f, -1.f), Vec3(0.f, 0.f, 1.f), 0.5f, Rgba8::GREEN);
		AddVertsForCone3D(verts, Vec3(0.f, 0.f, 0.f), Vec3(-1.f, 0.f, 0.f), 0.5f, Rgba8::BLACK, AABB2::ZERO_TO_ONE, 6);

		g_renderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
		g_renderer->SetBlendMode(BlendMode::OPAQUE);
		g_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_renderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP);
		g_renderer->BindTexture(nullptr);
		g_renderer->BindShader(nullptr);
		g_renderer->SetModelConstants(GetModelToWorldTransform());
		g_renderer->DrawVertexArray(verts);
		g_renderer->EndRendererEvent();

		verts.clear();
		
		g_renderer->BeginRendererEvent("Draw - Camera Frustrum");
		//Render camera frustrum
		Frustum cameraFrustrum = GetViewFrustrum();
		Rgba8 color = Rgba8::RED;
		AddVertsForLineSegment3D(verts, cameraFrustrum.m_nearPlanePoints[0], cameraFrustrum.m_nearPlanePoints[1], 0.025f,color);
		AddVertsForLineSegment3D(verts, cameraFrustrum.m_nearPlanePoints[1], cameraFrustrum.m_nearPlanePoints[2], 0.025f,color);
		AddVertsForLineSegment3D(verts, cameraFrustrum.m_nearPlanePoints[2], cameraFrustrum.m_nearPlanePoints[3], 0.025f,color);
		AddVertsForLineSegment3D(verts, cameraFrustrum.m_nearPlanePoints[3], cameraFrustrum.m_nearPlanePoints[0], 0.025f,color);

		AddVertsForLineSegment3D(verts, cameraFrustrum.m_farPlanePoints[0], cameraFrustrum.m_farPlanePoints[1], 1.5f, color);
		AddVertsForLineSegment3D(verts, cameraFrustrum.m_farPlanePoints[1], cameraFrustrum.m_farPlanePoints[2], 1.5f, color);
		AddVertsForLineSegment3D(verts, cameraFrustrum.m_farPlanePoints[2], cameraFrustrum.m_farPlanePoints[3], 1.5f, color);
		AddVertsForLineSegment3D(verts, cameraFrustrum.m_farPlanePoints[3], cameraFrustrum.m_farPlanePoints[0], 1.5f, color);

		AddVertsForLineSegment3D(verts, cameraFrustrum.m_nearPlanePoints[0], cameraFrustrum.m_farPlanePoints[0], 1.5f, color);
		AddVertsForLineSegment3D(verts, cameraFrustrum.m_nearPlanePoints[1], cameraFrustrum.m_farPlanePoints[1], 1.5f, color);
		AddVertsForLineSegment3D(verts, cameraFrustrum.m_nearPlanePoints[2], cameraFrustrum.m_farPlanePoints[2], 1.5f, color);
		AddVertsForLineSegment3D(verts, cameraFrustrum.m_nearPlanePoints[3], cameraFrustrum.m_farPlanePoints[3], 1.5f, color);

		/*
		for (int i = 0; i < 6; ++i)
		{
			Plane3D const& plane = cameraFrustrum.m_planes[i];

			Vec3 center;
			if (i == 0) //near plane
			{
				for (int j = 0; j < 4; ++j)
				{
					center += cameraFrustrum.m_nearPlanePoints[j];
				}
			}

			else if (i == 1) // far plane
			{
				for (int j = 0; j < 4; ++j)
				{
					center += cameraFrustrum.m_farPlanePoints[j];
				}
			}

			else if (i == 2) // bottom plane
			{
				center += cameraFrustrum.m_nearPlanePoints[0];
				center += cameraFrustrum.m_nearPlanePoints[1];
				center += cameraFrustrum.m_farPlanePoints[0];
				center += cameraFrustrum.m_farPlanePoints[1];
			}

			else if (i == 3) // top plane
			{
				center += cameraFrustrum.m_nearPlanePoints[2];
				center += cameraFrustrum.m_nearPlanePoints[3];
				center += cameraFrustrum.m_farPlanePoints[2];
				center += cameraFrustrum.m_farPlanePoints[3];
			}

			else if (i == 4) // left plane
			{
				center += cameraFrustrum.m_nearPlanePoints[0];
				center += cameraFrustrum.m_nearPlanePoints[3];
				center += cameraFrustrum.m_farPlanePoints[0];
				center += cameraFrustrum.m_farPlanePoints[3];
			}

			else if (i == 5) // right plane
			{
				center += cameraFrustrum.m_nearPlanePoints[1];
				center += cameraFrustrum.m_nearPlanePoints[2];
				center += cameraFrustrum.m_farPlanePoints[1];
				center += cameraFrustrum.m_farPlanePoints[2];
			}


			center *= 0.25f;

			AddVertsForArrow3D(verts, center, plane.m_normal, 10.f, 1.f, 6, Rgba8::ORANGE);
		}
		*/

		g_renderer->SetModelConstants();
		g_renderer->DrawVertexArray(verts);
		g_renderer->EndRendererEvent();
	}
}


Frustum Player::GetViewFrustrum() const
{
	if (m_cameraMode == CameraMode::FRUSTUM_CULL)
	{
		return m_camera->GetPerspectiveFrustum(m_position, m_orientationEuler, m_camera->GetFOV(),
			m_camera->GetAspect(), m_camera->GetPerspectiveNearDistance(),CAMERA_FAR_DISTANCE);
	}

	return m_camera->GetPerspectiveFrustum();
}

CameraConstants Player::GetCameraConstants() const
{
	CameraConstants cameraConstants;
	cameraConstants.m_worldToCameraTransform = m_camera->GetWorldToCameraTransform();
	cameraConstants.m_cameraToRenderTransform = m_camera->GetCameraToRenderTransform();
	cameraConstants.m_renderToClipTransform = m_camera->GetRenderToClipTransform();
	cameraConstants.m_cameraPosition = m_camera->GetPosition();
	return cameraConstants;
}

void Player::CheckKeyboardInputControls(float deltaSeconds)
{
	ImGuiIO const& imguiIO = ImGui::GetIO();
	if (imguiIO.WantCaptureMouse)
		return;

	//Rotation
	int xDirection = g_invertMouseX ? -1 : 1;
	int yDirection = g_invertMouseY ? 1 : -1;
	Vec2 mouseDelta = g_inputSystem->GetCursorClientDelta();
	EulerAngles orientation = m_orientationEuler;
	if (m_cameraMode == CameraMode::FRUSTUM_CULL)
	{
		orientation = m_camera->GetOrientation();
	}
	orientation.m_yawDegrees += mouseDelta.x * g_mouseSensitivity * yDirection;
	orientation.m_pitchDegrees = GetClamped(orientation.m_pitchDegrees + (mouseDelta.y * g_mouseSensitivity * xDirection), -85.f, 85.f);

	//Movement
	Vec3 translation;
	Vec3 fwrd;
	Vec3 left;
	Vec3 up;
	orientation.GetAsVectors_IFwd_JLeft_KUp(fwrd, left, up);

	if (m_cameraMode == CameraMode::WORLD_ALIGNED)
	{
		fwrd.z = 0.f;
		fwrd.Normalize();
		left.z = 0.f; left.Normalize();
		up = Vec3::UP;
	}


	if (g_inputSystem->IsKeyDown('W'))
	{
		translation += fwrd * (m_velocity * deltaSeconds);
	}

	if ((g_inputSystem->IsKeyDown('S')))
	{
		translation -= fwrd * (m_velocity * deltaSeconds);
	}

	if ((g_inputSystem->IsKeyDown('A')))
	{
		translation += left * (m_velocity * deltaSeconds);
	}

	if ((g_inputSystem->IsKeyDown('D')))
	{
		translation -= left * (m_velocity * deltaSeconds);
	}

	if ((g_inputSystem->IsKeyDown('E')))
	{
		translation += up * (m_velocity * deltaSeconds);
	}

	if ((g_inputSystem->IsKeyDown('Q')))
	{
		translation -= up * (m_velocity * deltaSeconds);
	}

	if (g_inputSystem->IsKeyDown(KEYCODE_SHIFT))
	{
		if (g_inputSystem->GetWheelDelta() > 0.f)
		{
			m_sprintSpeed += 100.f * deltaSeconds;
		}

		else if (g_inputSystem->GetWheelDelta() < 0.f)
		{
			m_sprintSpeed -= 100.f * deltaSeconds;
		}
		m_sprintSpeed = GetClamped(m_sprintSpeed, 1.f, 100.f);
		translation *= m_sprintSpeed;
		if (g_showGameDebugMessages)
		{
			DebugAddMessage(Stringf("Sprint Speed: %.2f", m_sprintSpeed), 0.f, Rgba8::GREEN);
		}
	}

	if (m_cameraMode == CameraMode::FRUSTUM_CULL)
	{
		Vec3 cameraPos = m_camera->GetPosition();
		m_camera->SetPositionAndOrientation(cameraPos + translation, orientation);
	}

	else
	{
		m_position += translation;
		CheckTerrainCollision();
		m_orientationEuler = orientation;
		m_camera->SetPositionAndOrientation(m_position, m_orientationEuler);
	}

	if (g_inputSystem->WasKeyJustPressed('H'))
	{
		m_position = Vec3(0.f, 0.f, 0.f);
		m_orientationEuler = EulerAngles(0.f, 0.f, 0.f);
	}

	if (g_inputSystem->WasKeyJustPressed('C'))
	{
		ChangeCameraMode();
		m_position = m_camera->GetPosition();
		m_orientationEuler = m_camera->GetOrientation();
	}

	if (g_inputSystem->WasKeyJustPressed('F'))
	{
		m_flashlightOn = !m_flashlightOn;
	}
}

void Player::CheckControllerInputControls(float deltaSeconds)
{
	XboxController controller = g_inputSystem->GetController(0);

	//Rotation
	int xDirection = g_invertControllerViewX ? 1 : -1;
	int yDirection = g_invertControllerViewY ? 1 : -1;

	AnalogJoystick rightStick = controller.GetRightStick();
	float rightStickMagnitude = rightStick.GetMagnitude();
	Vec2 rightStickPos = rightStick.GetPosition();

	m_orientationEuler.m_yawDegrees += xDirection * rightStickMagnitude * rightStickPos.x * g_controllerSensitivity * deltaSeconds;
	m_orientationEuler.m_pitchDegrees = GetClamped(m_orientationEuler.m_pitchDegrees + (yDirection * rightStickMagnitude * rightStickPos.y * g_controllerSensitivity * deltaSeconds), -85.f, 85.f);

	//Movement
	Vec3 translation;
	Vec3 fwrd;
	Vec3 left;
	Vec3 up;
	m_orientationEuler.GetAsVectors_IFwd_JLeft_KUp(fwrd, left, up);

	if (m_cameraMode == CameraMode::WORLD_ALIGNED)
	{
		fwrd.z = 0.f;
		fwrd.Normalize();
		left.z = 0.f; left.Normalize();
		up = Vec3::UP;
	}


	AnalogJoystick leftStick = controller.GetLeftStick();
	float leftStickMagnitude = leftStick.GetMagnitude();
	Vec2 leftStickPos = leftStick.GetPosition();
	if (leftStickMagnitude > 0.f)
	{
		translation += fwrd * (leftStickMagnitude * leftStickPos.y * m_velocity * deltaSeconds);
		translation -= left * (leftStickMagnitude * leftStickPos.x * m_velocity * deltaSeconds); //reversed because of left basis y
	}

	if (controller.IsButtonDown(XboxButtonID::BUTTON_LSHOULDER))
	{
		translation += up * (m_velocity * deltaSeconds);
	}

	if (controller.IsButtonDown(XboxButtonID::BUTTON_RSHOULDER))
	{
		translation -= up * (m_velocity * deltaSeconds);
	}

	if (controller.IsButtonDown(XboxButtonID::BUTTON_A))
	{
		translation *= 10.f;
	}

	m_position += translation;

	if (controller.WasButtonJustPressed(XboxButtonID::BUTTON_START))
	{
		m_position = Vec3(0.f, 0.f, 0.f);
		m_orientationEuler = EulerAngles(0.f, 0.f, 0.f);
	}
}

void Player::CreateCompass()
{
	Verts verts;
	AddVertsForArrow3D(verts, Vec3::ZERO, Vec3::FORWARD, .01f, 0.00075f, 8, Rgba8::RED);
	AddVertsForArrow3D(verts, Vec3::ZERO, Vec3::LEFT, .01f, 0.00075f, 8, Rgba8::GREEN);
	AddVertsForArrow3D(verts, Vec3::ZERO, Vec3::UP, .01f, 0.00075f, 8, Rgba8::BLUE);

	m_compassVbo = g_renderer->CreateVertexBuffer(verts, "Compass_VBO");

}

void Player::ChangeCameraMode()
{
	int cameraMode = (int)m_cameraMode;
	cameraMode++;
	if (cameraMode >= (int)CameraMode::NUM_CAMERA_MODES)
	{
		cameraMode = 0;
	}

	m_cameraMode = (CameraMode)cameraMode;
}

void Player::CheckTerrainCollision()
{
	if (m_cameraMode == CameraMode::FREE_FLY)
	{
		g_game->m_world->PushSphereOutOfTerrain(m_position, m_collisionRadius);
		float worldHeight = g_game->m_world->m_definition->m_worldHeight;
		if (m_position.z >= worldHeight)
		{
			m_position.z = worldHeight;
		}
	}


	m_densityLastFrame = g_game->m_world->GetDensityAtPosition(m_position);
	m_positionLastFrame = m_position;

}

char const* Player::GetTextForCameraMode() const
{
	switch (m_cameraMode)
	{
	case CameraMode::FREE_FLY: return "Free Fly";
	case CameraMode::NO_CLIP: return "No Clip";
	case CameraMode::WORLD_ALIGNED: return "No Clip, World Aligned";
	case CameraMode::FRUSTUM_CULL: return "Debug Frustum Cull";
	}
	return "";
}
