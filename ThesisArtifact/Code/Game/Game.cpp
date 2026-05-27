#include "Game/Game.hpp"
#include "Game/Entity.hpp"
#include "Game/Player.hpp"
#include "Game/GameCommon.hpp"
#include "Game/App.hpp"
#include "Game/World.hpp"
#include "Game/WorldDefinition.hpp"

#include <Engine/Core/ErrorWarningAssert.hpp>
#include "Engine/Renderer/RendererDX12.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/LineSegment2.hpp"
#include "Engine/Math/Disc2.hpp"
#include "Engine/Math/OBB2.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/Timer.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Renderer/CommandQueue.hpp"
#include "Engine/Renderer/VertexBufferDX12.hpp"
#include "Engine/Renderer/IndexBufferDX12.hpp"
#include "Engine/Renderer/ShaderDX12.hpp"
#include "Engine/Renderer/PipelineStateObjectDX12.hpp"
#include "Engine/Renderer/RootSignatureDX12.hpp"
#include "Engine/Renderer/CommandList.hpp"
#include "Engine/Renderer/TextureDX12.hpp"
#include "Engine/Core/StaticMesh.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/ImGui/ImGuiSystem.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include <vector>

#include "ThirdParty/TinyXML2/tinyxml2.h"




RandomNumberGenerator* g_rng;
bool g_showGameDebugMessages = false;
bool g_debugMode = false;
bool g_invertMouseX = false;
bool g_invertMouseY = false;
bool g_invertControllerViewX = false;
bool g_invertControllerViewY = false;
float g_mouseSensitivity = 0.125f;
float g_controllerSensitivity = 76.f;

int g_debugShaderInt = 0;

Game::Game()
	:m_gameClock(new Clock(Clock::GetSystemClock()))
{
	
}

Game::~Game()
{
	delete m_worldCamera;
	m_worldCamera = nullptr;
	delete m_screenCamera;
	m_screenCamera = nullptr;
	delete m_skyBoxCamera;
	m_skyBoxCamera = nullptr;
	delete g_rng;
	g_rng = nullptr;
	delete m_gameClock;
	m_gameClock = nullptr;

	delete m_world;
	m_world = nullptr;

	for (int i = 0; i < (int)m_entities.size(); ++i)
	{
		if (m_entities[i])
		{
			delete m_entities[i];
			m_entities[i] = nullptr;
		}
	}

	UnsubscribeAllEventCallbacksForObject(this);
}

//Game management
//--------------------------------------------------------------------
void Game::Startup()
{

	g_rng = new RandomNumberGenerator();
	LoadAllAudioAssets();
	//PlayGameMusic(ATTRACT_SCREEN);
	
	SubscribeEvents();

	//Protogame attract screen delete later
	m_attractTimer = new Timer(1.f, m_gameClock);

	std::string defaultWorldFilePath = g_gameConfigBlackboard.GetValue("DefaultWorldFile", "Data/Definitions/WorldDefinitions/DefaultWorld.xml");
	LoadWorldFromXmlFile(defaultWorldFilePath);
	m_player = new Player(this);
	m_entities.push_back(m_player);

	m_screenCamera = new Camera();
	m_skyBoxCamera = new Camera();
	m_skyBoxCamera->SetCameraToRenderTransform(Mat44::IFWRD_JLEFT_KUP_TO_DX11RENDER);

	m_worldCamera = m_player->m_camera;
	m_worldCamBottomLeft = Vec2(-1.f, -1.f);
	m_worldCamTopRight = Vec2(1.f, 1.f);
	
	LoadAttractScreenImages();
}

void Game::Shutdown()
{
	if (m_world)
	{
		m_world->Shutdown();
	}

	WorldDefinition::ClearWorldDefinitionData();
}

//Frame Flow
//--------------------------------------------------------------------
void Game::BeginFrame()
{
	//m_worldCamera->SetOrthoView(m_worldCamBottomLeft, m_worldCamTopRight);
	m_screenCamera->SetOrthoView(Vec2(0.f, 0.f), Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));
}

void Game::Update()
{
	double timeStarted = GetCurrentTimeSeconds();
	CheckKeyboardInputs();
	CheckControllerInputs();
	UpdateAttractMode();
	UpdateGameplay();
	UpdateCameras();

	m_updateSpeed = GetCurrentTimeSeconds() - timeStarted;
}

void Game::Render() const
{
	double timeStarted = GetCurrentTimeSeconds();
 	g_renderer->ClearScreen(Rgba8(100,150,255));

	if(m_world && m_gameState == GameState::GAMEPLAY)
	{
		g_renderer->BeginCamera(*m_skyBoxCamera);
		m_world->RenderSkyBox();
		g_renderer->EndCamera(*m_skyBoxCamera);
	}

 	//World Camera
 	g_renderer->BeginCamera(*m_worldCamera);
	RenderGameplay();
	RenderDebugWorld();
 	g_renderer->EndCamera(*m_worldCamera);


	//Screen Camera
 	g_renderer->BeginCamera(*m_screenCamera);
	RenderAttractMode();
	RenderResourceLoading();
	g_devConsole->Render(m_screenCamera);
	RenderPerformanceStats(timeStarted);
	DebugRenderScreen(*m_screenCamera);
	g_renderer->EndCamera(*m_screenCamera);
}

void Game::EndFrame()
{
	if (m_shouldRebuildWorld)
	{
		g_renderer->FlushGPU();
		LoadWorldFromWorldName(m_pendingWorldName);
	}

	m_shouldRebuildWorld = false;
}

//Update Functions
//--------------------------------------------------------------------
void Game::UpdateAttractMode()
{
	if(m_gameState != GameState::ATTRACT_SCREEN)
		return;

	
	if (m_attractTimer->Tick())
	{
		m_numAttractScreenLoadingDots++;
		if(m_numAttractScreenLoadingDots > 3)
			m_numAttractScreenLoadingDots = 0;
	}

	UpdateAttractScreenSlideShow();

	if (m_world && m_loadWorldInAttract)
	{
		m_world->AttractScreenUpdate();
	}
}

void Game::UpdateAttractScreenSlideShow()
{
	float const deltaSeconds = m_gameClock->GetDeltaSeconds();

	if (m_attractScreenImages.empty())
		return;

	if (m_attractScreenImages.size() == 1)
	{
		m_currentAttractImageIndex = 0;
		m_nextAttractImageIndex = 0;
		m_isFadingAttractImage = false;
		m_attractImageHoldTimer = 0.0f;
		m_attractImageFadeTimer = 0.0f;
		return;
	}

	if (!m_isFadingAttractImage)
	{
		m_attractImageHoldTimer += deltaSeconds;

		if (m_attractImageHoldTimer >= m_attractImageHoldSeconds)
		{
			m_attractImageHoldTimer = 0.0f;
			m_attractImageFadeTimer = 0.0f;
			m_isFadingAttractImage = true;

			m_nextAttractImageIndex = (m_currentAttractImageIndex + 1);
			if (m_nextAttractImageIndex >= (int)m_attractScreenImages.size())
			{
				m_nextAttractImageIndex = 0;
			}
		}
	}
	else
	{
		m_attractImageFadeTimer += deltaSeconds;

		if (m_attractImageFadeTimer >= m_attractImageFadeSeconds)
		{
			m_attractImageFadeTimer = 0.0f;
			m_isFadingAttractImage = false;
			m_currentAttractImageIndex = m_nextAttractImageIndex;
		}
	}
}

void Game::UpdateGameplay()
{
	if(m_gameState != GameState::GAMEPLAY)
		return;

	PerFrameConstants frameConstants;
	frameConstants.m_debugInt = g_debugShaderInt;
	frameConstants.m_time = m_gameClock->GetTotalSeconds();
	g_renderer->SetPerFrameConstants(frameConstants);

	for (int entityNum = 0; entityNum < (int)m_entities.size(); ++entityNum)
	{
		Entity* currentEntity = m_entities[entityNum];
		if (currentEntity)
		{
			currentEntity->Update();
		}
	}

	if (m_world)
	{
		m_world->Update();
	}
}

void Game::UpdateCameras()
{
	//Set perspective mode
	IntVec2 windowDimensions = Window::s_mainWindow->GetClientDimensions();
	float aspect = (float)windowDimensions.x / (float)windowDimensions.y;
	if (m_player && m_player->GetCameraMode() == CameraMode::FRUSTUM_CULL)
	{
		m_worldCamera->SetPerspectiveView(aspect, 60.f, 0.1f, CAMERA_FAR_DISTANCE * 10.f);
		m_skyBoxCamera->SetPerspectiveView(aspect, 60.f, 0.1f, SKYBOX_FAR_DISTANCE * 10.f);
	}

	else
	{
		m_worldCamera->SetPerspectiveView(aspect, 60.f, 0.1f, CAMERA_FAR_DISTANCE);
		m_skyBoxCamera->SetPerspectiveView(aspect, 60.f, 0.1f, SKYBOX_FAR_DISTANCE);
	}

	m_skyBoxCamera->SetPosition(m_worldCamera->GetPosition());
	m_skyBoxCamera->SetOrientation(m_worldCamera->GetOrientation());
}

//Render Functions
//--------------------------------------------------------------------

float Game::GetAttractImageFadeFraction() const
{
	if (!m_isFadingAttractImage)
		return 0.0f;

	if (m_attractImageFadeSeconds <= 0.0f)
		return 1.0f;

	float fadeFraction = (m_attractImageFadeTimer / m_attractImageFadeSeconds);

	if (fadeFraction < 0.0f)
	{
		fadeFraction = 0.0f;
	}
	if (fadeFraction > 1.0f)
	{
		fadeFraction = 1.0f;
	}

	return fadeFraction;
}

void Game::RenderAttractMode() const
{
	if (m_gameState != GameState::ATTRACT_SCREEN)
		return;

	g_renderer->BeginRendererEvent("Draw - AttractScreen");

	AABB2 screenBounds = m_screenCamera->GetOrthoBounds();
	Vec2 screenDimensions = screenBounds.GetDimensions();
	float screenAspect = (screenDimensions.x / screenDimensions.y);
	float imageAspect = (16.f / 9.f);

	AABB2 coverBounds = screenBounds;

	if (screenAspect > imageAspect)
	{
		float coverHeight = (screenDimensions.x / imageAspect);
		float heightOffset = ((coverHeight - screenDimensions.y) * 0.5f);
		coverBounds.m_mins.y = (screenBounds.m_mins.y - heightOffset);
		coverBounds.m_maxs.y = (screenBounds.m_maxs.y + heightOffset);
	}
	else
	{
		float coverWidth = (screenDimensions.y * imageAspect);
		float widthOffset = ((coverWidth - screenDimensions.x) * 0.5f);
		coverBounds.m_mins.x = (screenBounds.m_mins.x - widthOffset);
		coverBounds.m_maxs.x = (screenBounds.m_maxs.x + widthOffset);
	}

	Verts verts;
	AddVertsForAABB2D(verts, coverBounds, Rgba8::WHITE);

	g_renderer->SetDepthMode(DepthMode::DISABLED);
	g_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->BindShader(nullptr);
	g_renderer->BindRootSignature(nullptr);

	if (!m_attractScreenImages.empty())
	{
		float fadeFraction = GetAttractImageFadeFraction();

		TextureDX12* currentTexture = m_attractScreenImages[m_currentAttractImageIndex];
		TextureDX12* nextTexture = m_attractScreenImages[m_nextAttractImageIndex];

		if (!m_isFadingAttractImage)
		{
			g_renderer->SetBlendMode(BlendMode::OPAQUE);
			g_renderer->SetModelConstants(Mat44::IDENTITY, Rgba8::WHITE);
			g_renderer->BindTexture(currentTexture);
			g_renderer->DrawVertexArray(verts);
		}
		else
		{
			unsigned char currentAlpha = (unsigned char)(255.0f * (1.0f - fadeFraction));
			unsigned char nextAlpha = (unsigned char)(255.0f * fadeFraction);

			g_renderer->SetBlendMode(BlendMode::ALPHA);

			g_renderer->SetModelConstants(Mat44::IDENTITY, Rgba8(255, 255, 255, currentAlpha));
			g_renderer->BindTexture(currentTexture);
			g_renderer->DrawVertexArray(verts);

			g_renderer->SetModelConstants(Mat44::IDENTITY, Rgba8(255, 255, 255, nextAlpha));
			g_renderer->BindTexture(nextTexture);
			g_renderer->DrawVertexArray(verts);
		}
	}

	verts.clear();

	TextureDX12* titleTexture = g_renderer->CreateOrGetTextureFromFile("Data/Images/CoverImages/Title.png");
	AABB2 titleBounds = coverBounds.GetBoxAtUVS(AABB2(0.2f, 0.3f, 0.8f, 0.9f));
	AddVertsForAABB2D(verts, titleBounds);


	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->SetDepthMode(DepthMode::DISABLED);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_renderer->SetModelConstants(Mat44::IDENTITY, Rgba8::WHITE);
	g_renderer->BindShader(nullptr);
	g_renderer->BindTexture(titleTexture);
	g_renderer->DrawVertexArray(verts);

	g_renderer->EndRendererEvent();

	AABB2 textBounds = coverBounds.GetBoxAtUVS(AABB2(0.75f, 0.05f, 0.99f, 0.15f));
	std::string text = "Press SPACEBAR to Start";
	for (int i = 0; i < m_numAttractScreenLoadingDots; ++i)
	{
		text.append(".");
	}

	int maxLoadingDots = 3;
	int difference = (maxLoadingDots - m_numAttractScreenLoadingDots);
	for (int i = 0; i < difference; ++i)
	{
		text.append(" ");
	}

	DebugAddScreenText(text, textBounds, textBounds.GetHeight(), Vec2(1.f, 0.5f), 0.f, Rgba8::WHITE);
}

void Game::RenderResourceLoading() const
{
	if (!m_world || m_world->m_state != WorldState::LOADING_RESOURCES)
		return;

	m_world->RenderResourceLoading();
}

void Game::RenderDebugWorld() const
{
	if (m_gameState == GameState::ATTRACT_SCREEN)
		return;

	g_renderer->BeginRendererEvent("Draw - Debug Render World");
	DebugRenderWorld(*m_worldCamera);
	g_renderer->EndRendererEvent();
}

void Game::RenderPerformanceStats(double const& timeStarted) const
{
	if(!g_showGameDebugMessages)
		return;
	float deltaSeconds = Clock::GetSystemClock().GetDeltaSeconds();
	float fps = 1.f / deltaSeconds;
	Rgba8 color = fps >= 59.f ? Rgba8::WHITE : Rgba8::ORANGE;
	if (fps < 30.f)
		color = Rgba8::RED;

	AABB2 screenBounds = GetScreenBounds();
	AABB2 bounds = screenBounds;
	bounds.AddPadding(-.9f, -.95f, -.01f, -.0001f);
	std::string text = Stringf("FPS: %.1f", 1.f / deltaSeconds);
	DebugAddScreenText(text, bounds, 0.1f * SCREEN_SIZE_Y, Vec2(1.f, 0.5f), 0.f, color);

	color = m_updateSpeed <= 0.0167 ? Rgba8::WHITE : Rgba8::ORANGE;
	if (m_updateSpeed > 0.0334f)
		color = Rgba8::RED;

	bounds = screenBounds;
	bounds.AddPadding(-0.9f, -.9f, -.01f, 0.f);
	text = Stringf("Update: %.1f ms", m_updateSpeed * 1000.0);
	DebugAddScreenText(text, bounds, 0.1f * SCREEN_SIZE_Y, Vec2(1.f, 0.5f), 0.f, color);

	double renderSpeed = (GetCurrentTimeSeconds() - timeStarted) + m_lastFrameRenderDuration;
	color = renderSpeed <= 0.0167 ? Rgba8::WHITE : Rgba8::ORANGE;
	if(m_renderSpeed > 0.0334f)
		color = Rgba8::RED;

	bounds = screenBounds;
	bounds.AddPadding(-0.9f, -.85f, -.01f, 0.f);
	text = Stringf("Render: %.1f ms", renderSpeed * 1000.0);
	DebugAddScreenText(text, bounds, 0.1f * SCREEN_SIZE_Y, Vec2(1.f, 0.5f), 0.f, color);
}

void Game::RenderGameplay() const
{
	if (m_gameState != GameState::GAMEPLAY)
		return;

	g_renderer->BeginRendererEvent("Draw - Gameplay");

	g_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_renderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->BindShader(nullptr);
	g_renderer->BindRootSignature(nullptr);

	m_world->Render();

	for (int entityNum = 0; entityNum < (int)m_entities.size(); ++entityNum)
	{
		Entity* currentEntity = m_entities[entityNum];
		if (currentEntity)
		{
			currentEntity->Render();
		}
	}

	g_renderer->EndRendererEvent();
}

//Input
//--------------------------------------------------------------------
void Game::CheckKeyboardInputs()
{

	//Tab - free mouse for IMGUI
	if (g_inputSystem->WasKeyJustPressed(KEYCODE_TAB))
	{
		g_freeMouse = !g_freeMouse;
	}

	if (g_inputSystem->WasKeyJustPressed(KEYCODE_F1))
	{
		g_showGameDebugMessages = !g_showGameDebugMessages;
	}

	//Escape
	if (g_inputSystem->WasKeyJustPressed(KEYCODE_ESC))
	{
		if (m_gameState == GameState::ATTRACT_SCREEN)
		{
			g_app->HandleQuitRequested();
		}

		//Return to attract screen
		else
		{
			ExitGame();
		}
	}

	//Toggle Debug
	if (g_inputSystem->WasKeyJustPressed(KEYCODE_F1)) //F1 key
	{
		g_debugMode = !g_debugMode;
	}

	//Restart Game
	if (g_inputSystem->WasKeyJustPressed(KEYCODE_F8)) //F8 key
	{
		m_shouldRestart = true;
	}

	ImGuiIO const& imguiIO = ImGui::GetIO();
	if (imguiIO.WantCaptureMouse)
		return;

	//Unfree Mouse
	if (g_inputSystem->WasKeyJustPressed(KEYCODE_LEFT_MOUSE_BUTTON))
	{
		Vec2 mousePositionUV = g_window->GetNormalizedMouseUV();

		if (mousePositionUV.x >= 0.f && mousePositionUV.x <= 1.f && mousePositionUV.y >= 0.f && mousePositionUV.y <= 1.f)
		{
			g_freeMouse = false;
		}
	}

	
	if (g_inputSystem->WasKeyJustPressed(' '))
	{
		if (m_gameState == GameState::ATTRACT_SCREEN)
		{
			StartGame();
		}
	}
	

	//Pause
	if (g_inputSystem->WasKeyJustPressed('P') )
	{
		m_gameClock->TogglePause();
	}

	//SloMo
	if (g_inputSystem->WasKeyJustPressed('I'))
	{
		m_isSlowMo = !m_isSlowMo;
		if (m_isSlowMo)
		{
			m_gameClock->SetTimeScale(0.1f);
		}
		else
		{
			m_gameClock->SetTimeScale(1.f);
		}
	}

	//Move one Frame
	if (g_inputSystem->WasKeyJustPressed('O') )
	{
		m_gameClock->StepSingleFrame();
	}

	//PerFrameConstants
	bool triggeredDebug = false;
	bool isShiftDown = g_inputSystem->IsKeyDown(KEYCODE_SHIFT);
	if (g_inputSystem->WasKeyJustPressed('1'))
	{
		g_debugShaderInt = isShiftDown ? 11 : 1;
		triggeredDebug = true;
	}

	else if (g_inputSystem->WasKeyJustPressed('0'))
	{
		g_debugShaderInt = isShiftDown ? 10 : 0;
		triggeredDebug = true;
	}

	else if (g_inputSystem->WasKeyJustPressed('2'))
	{
		g_debugShaderInt = isShiftDown ? 12 : 2;
		triggeredDebug = true;
	}

	else if (g_inputSystem->WasKeyJustPressed('3'))
	{
		g_debugShaderInt = isShiftDown ? 13 : 3;
		triggeredDebug = true;
	}

	else if (g_inputSystem->WasKeyJustPressed('4'))
	{
		g_debugShaderInt = isShiftDown ? 14 : 4;
		triggeredDebug = true;
	}

	else if (g_inputSystem->WasKeyJustPressed('5'))
	{
		g_debugShaderInt = isShiftDown ? 15 : 5;
		triggeredDebug = true;
	}

	else if (g_inputSystem->WasKeyJustPressed('6'))
	{
		g_debugShaderInt = isShiftDown ? 16 : 6;
		triggeredDebug = true;
	}

	else if (g_inputSystem->WasKeyJustPressed('7'))
	{
		g_debugShaderInt = isShiftDown ? 17 : 7;
		triggeredDebug = true;
	}

	else if (g_inputSystem->WasKeyJustPressed('8'))
	{
		g_debugShaderInt = isShiftDown ? 18 : 8;
		triggeredDebug = true;
	}

	else if (g_inputSystem->WasKeyJustPressed('9'))
	{
		g_debugShaderInt = isShiftDown ? 19 : 9;
		triggeredDebug = true;
	}

	else if (g_inputSystem->WasKeyJustPressed('5'))
	{
		g_debugShaderInt = isShiftDown ? 15 : 5;
		triggeredDebug = true;
	}

	if (triggeredDebug)
	{
		PerFrameConstants frameConstants;
		frameConstants.m_debugInt = g_debugShaderInt;
		frameConstants.m_time = m_gameClock->GetTotalSeconds();
		g_renderer->SetPerFrameConstants(frameConstants);
	}
}

void Game::CheckControllerInputs()
{
	
}


void Game::StartGame()
{
	if(!m_world || m_world->m_state != WorldState::ACTIVE)
		return;

	m_gameState = GameState::GAMEPLAY;
	PlayGameSFX(BUTTON_SELECT);
}

void Game::ExitGame()
{
	m_gameState = GameState::ATTRACT_SCREEN;
	m_loadWorldInAttract = false;
}

void Game::LoadAttractScreenImages()
{
	XmlDocument coverImagesDoc;
	XmlResult result = coverImagesDoc.LoadFile("Data/Definitions/CoverImages.xml");
	if(result != tinyxml2::XML_SUCCESS)
		return;

	XmlElement* rootElement = coverImagesDoc.RootElement();
	if(!rootElement)
		return;

	XmlElement const* childElement = rootElement->FirstChildElement("Image");

	while (childElement)
	{
		std::string filePath = ParseXmlAttribute(*childElement, "filePath", UN_INITIALIZED_WORLD_DATA_NAME);
		m_attractScreenImages.push_back(g_renderer->CreateOrGetTextureFromFile(filePath.c_str()));
		childElement = childElement->NextSiblingElement("Image");
	}
}

//Audio and SFX
//-----------------------------------------------------------------------------------------------
void Game::LoadAllAudioAssets()
{
	//Music
	m_gameMusicDataPaths.reserve(NUM_GAME_MUSIC);

	g_audioSystem->CreateOrGetSound("Data/Audio/Music/AttractScreenMusic.mp3");
	m_gameMusicDataPaths.push_back("Data/Audio/Music/AttractScreenMusic.mp3");

	//SFX
	m_sfxDataPaths.reserve(NUM_GAME_SFX);

	g_audioSystem->CreateOrGetSound("Data/Audio/SFX/ButtonSelect.mp3");
	m_sfxDataPaths.push_back("Data/Audio/SFX/ButtonSelect.mp3");
}

void const Game::PlayGameSFX(GameSFX const& sfx)
{
	if ((int)m_sfxDataPaths.size() <= sfx)
		return;

	SoundID newSound = g_audioSystem->CreateOrGetSound(m_sfxDataPaths[sfx]);
	g_audioSystem->StartSound(newSound);
}

void const Game::PlayGameSFX(GameSFX const& sfx, Vec2 const& worldPos)
{
	if ((int)m_sfxDataPaths.size() <= sfx)
		return;

 	float balance = GetAudioBalanceFromWorldPosition(worldPos);
	SoundID newSound = g_audioSystem->CreateOrGetSound(m_sfxDataPaths[sfx]);
	g_audioSystem->StartSound(newSound, false, 1.f, balance);
}

void const Game::PlayLoopingGameSFX(GameSFX const& sfx)
{
	if ((int)m_sfxDataPaths.size() <= sfx)
		return;

	SoundID newSound = g_audioSystem->CreateOrGetSound(m_sfxDataPaths[sfx]);
	auto foundLoopingSFX = m_loopingSFXSoundPlaybackPairs.find(sfx);
	if (foundLoopingSFX == m_loopingSFXSoundPlaybackPairs.end())
	{
		m_loopingSFXSoundPlaybackPairs.insert({ sfx, g_audioSystem->StartSound(newSound, true) });
	}
}

void const Game::StopLoopingGameSFX(GameSFX const& sfx)
{
	auto foundLoopingSFX = m_loopingSFXSoundPlaybackPairs.find(sfx);
	if (foundLoopingSFX != m_loopingSFXSoundPlaybackPairs.end())
	{
		g_audioSystem->StopSound(foundLoopingSFX->second);
		m_loopingSFXSoundPlaybackPairs.erase(sfx);
	}
}

void const Game::PlayGameMusic(GameMusic const& music)
{
	if ((int)m_gameMusicDataPaths.size() <= music)
		return;

	if (m_musicSoundPlaybacks[music] == nullptr)
	{
		SoundID newMusic = g_audioSystem->CreateOrGetSound(m_gameMusicDataPaths[music]);
		m_musicSoundPlaybacks[music] = &newMusic;
	}

	g_audioSystem->StartSound(*m_musicSoundPlaybacks[music], true);
}

void const Game::StopGameMusic(GameMusic const& music)
{
	if (m_musicSoundPlaybacks[music] == nullptr)
		return;

	g_audioSystem->StopSound(*m_musicSoundPlaybacks[music]);
}

float Game::GetAudioBalanceFromWorldPosition(Vec2 const& inPosition) const
{
	return RangeMapClamped(inPosition.x, m_worldCamBottomLeft.x, m_worldCamTopRight.x, -1.f, 1.f);;
}

void const Game::SetAudioBalanceAndVolumeFromWorldPosition(SoundPlaybackID& sound, Vec2 const& worldPos)
{
	AABB2 cameraView = AABB2(m_worldCamBottomLeft, m_worldCamTopRight);
	if (!cameraView.IsPointOnOrInside(worldPos)) // volume = 0 if position is not in camera view
	{
		g_audioSystem->SetSoundPlaybackVolume(sound, 0.f);
	}

	g_audioSystem->SetSoundPlaybackBalance(sound, RangeMapClamped(worldPos.x, m_worldCamBottomLeft.x, m_worldCamTopRight.x, -1.f, 1.f));
}

AABB2 Game::GetScreenBounds() const
{
	return AABB2(m_screenCamera->GetOrthoBottomLeft(), m_screenCamera->GetOrthoTopRight());
}

void Game::QueueLoadWorldFromWorldName(std::string const& worldName)
{
	m_shouldRebuildWorld = true;
	m_pendingWorldName = worldName;
}

void Game::LoadWorldFromXmlFile(std::string const& xmlFile)
{
	WorldDefinition::InitWorldDefinitionsFromFile(xmlFile);
	std::string worldName = WorldDefinition::s_worldDefinitions[WorldDefinition::s_worldDefinitions.size() - 1].m_worldName;
	LoadWorldFromWorldName(worldName);
}

void Game::LoadWorldFromWorldName(std::string const& worldName)
{
	if (m_world)
	{
		m_world->Shutdown();
	}

	delete m_world;
	m_world = nullptr;

	m_world = new World(worldName);
}



void Game::SubscribeEvents()
{
	SubscribeEventCallbackObjectMethod("RenderDuration", this, &Game::Event_RenderDuration, false);
	SubscribeEventCallbackObjectMethod("Controls", this, &Game::Event_ShowGameControls);

	Strings timeScaleArguments;
	timeScaleArguments.push_back("Scale=");
	timeScaleArguments.push_back("Scale=1.0");
	SubscribeEventCallbackObjectMethod("TimeScale", timeScaleArguments, this, &Game::Event_TimeScale);
	Strings inputControlsArguments;
	inputControlsArguments.push_back("MouseSensitivity=");
	inputControlsArguments.push_back("ControllerSensitivity=");
	inputControlsArguments.push_back("InvertMouseX=");
	inputControlsArguments.push_back("InvertMouseY=");
	inputControlsArguments.push_back("InvertControllerX=");
	inputControlsArguments.push_back("InvertControllerY=");
	inputControlsArguments.push_back("Reset");
	SubscribeEventCallbackObjectMethod("ControlsSettings", inputControlsArguments, this, &Game::Event_ControlsSettings);
	EventArgs args;
	Event_ShowGameControls(args);
}


//Events
//-----------------------------------------------------------------------------------------------
bool Game::Event_ShowGameControls(EventArgs& args)
{
	UNUSED(args);
	
	g_devConsole->AddLine(DevConsole::INFO_MAJOR, "--Thesis Artifact Controls--", 1.f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[SPACE]                  Start", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[ESC]                    Exit/Quit", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[P]                      Pause", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[O]                      Step One Frame", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[MOUSE]                  Look", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[WASD]                   Move", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[Q/E]                    Move Up/Down", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[SHIFT]                  Sprint", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[SHIFT] + [MouseWheel]   Sprint Speed", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[C]                      Change Camera Mode", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[0-9]                    Render Modes", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[SHIFT] + [0-9]          Render Modes Continued", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[F1]                     Toggle Frame Debug Messages", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[F2]                     Chunk Debug Views", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[F3]                     Toggle Chunk Loading", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[F5]                     Job and Memory Stats", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[F8]                     Reload World", 0.75f, true);
	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[TAB]                    Mouse Visibility", 0.75f, true);

	g_devConsole->AddLine(DevConsole::INFO_MINOR, "[ARROWS]                 Change Light Direction", 0.75f, true);
	return true;
}

bool Game::Event_TimeScale(EventArgs& args)
{
	float scale = args.GetValue("Scale", 1.f);
	m_gameClock->SetTimeScale(scale);
	return true;

}

bool Game::Event_ControlsSettings(EventArgs& args)
{
	if (args.HasKey("Reset"))
	{
		g_mouseSensitivity = 0.125f;
		g_invertMouseX = false;
		g_invertMouseY = false;
		g_controllerSensitivity = 50.f;
		g_invertControllerViewX = false;
		g_invertControllerViewY = false;
	}

	if (args.HasKey("MouseSensitivity"))
	{
		g_mouseSensitivity = RangeMapClamped(args.GetValue("MouseSensitivity", 0.5f), 0.f, 1.f, 0.05f, 0.2f);
	}

	if (args.HasKey("InvertMouseX"))
	{
		g_invertMouseX = args.GetValue("InvertMouseX", false);
	}

	if (args.HasKey("InvertMouseY"))
	{
		g_invertMouseX = args.GetValue("InvertMouseY", false);
	}

	if (args.HasKey("ControllerSensitivity"))
	{
		g_controllerSensitivity = RangeMapClamped(args.GetValue("ControllerSensitivity", 0.5f), 0.f, 1.f, 20.f, 200.f);
	}

	if (args.HasKey("InvertControllerX"))
	{
		g_invertControllerViewX = args.GetValue("InvertControllerX", false);
	}

	if (args.HasKey("InvertControllerY"))
	{
		g_invertControllerViewX = args.GetValue("InvertControllerY", false);
	}

	return true;
}

bool Game::Event_RenderDuration(EventArgs& args)
{
	float duration = args.GetValue("Duration", 0.f);
	m_lastFrameRenderDuration = duration;
	return false;
}





