#pragma once
#include "Game/GameCommon.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Vec2.hpp"
#include <map>
#include <string>


class RandomNumberGenerator;
class Camera;
struct Vec2;
class Timer;
class Clock;
struct AABB2;

class VertexBufferDX12;
class IndexBufferDX12;
class ShaderDX12;
class StaticMesh;
class Entity;
class Player;
class World;
class TextureDX12;

constexpr int NUM_LIGHTS = 2;

enum GameMusic : int
{
	ATTRACT_SCREEN,
	NUM_GAME_MUSIC
};

enum GameSFX : int
{
	BUTTON_SELECT,
	NUM_GAME_SFX
};

enum class GameState : int 
{
	ATTRACT_SCREEN,
	GAMEPLAY,
	NUM_GAME_STATES
};


class Game : public EventSubscriber
{
public:
	 Game();
	~Game();

	//Game Flow Management
	void Startup();
	void Shutdown();
	void BeginFrame();
	void Update();
	void Render() const;
	void EndFrame();

	//Events
	bool		Event_ShowGameControls(EventArgs& args);
	bool		Event_TimeScale(EventArgs& args);
	bool		Event_ControlsSettings(EventArgs& args);
	bool		Event_RenderDuration(EventArgs& args);

	//Music and SFX
	void const PlayGameSFX(GameSFX const& sfx);
	void const PlayGameSFX(GameSFX const& sfx, Vec2 const& worldPos);
	void const PlayLoopingGameSFX(GameSFX const& sfx);
	void const StopLoopingGameSFX(GameSFX const& sfx);
	void const PlayGameMusic(GameMusic const& music);
	void const StopGameMusic(GameMusic const& music);
	float GetAudioBalanceFromWorldPosition(Vec2 const& inPosition) const;
	void const SetAudioBalanceAndVolumeFromWorldPosition(SoundPlaybackID& sound, Vec2 const& worldPos); // mutes sound if it is off screen

	AABB2 GetScreenBounds() const;

	void LoadWorldFromXmlFile(std::string const& xmlFile);
	void LoadWorldFromWorldName(std::string const& worldName);
	void QueueLoadWorldFromWorldName(std::string const& worldName);

private:
	//Construction
	void LoadAllAudioAssets();

	//Update Methods
	void UpdateAttractMode();
	void UpdateAttractScreenSlideShow();
	void UpdateGameplay();
	void UpdateCameras();

	//Render Methods
	float GetAttractImageFadeFraction() const;
	void RenderAttractMode() const;
	void RenderResourceLoading() const;
	void RenderGameplay() const;
	void RenderDebugWorld() const;
	void RenderPerformanceStats(double const& timeStarted) const;

	//Input
	void CheckKeyboardInputs();
	void CheckControllerInputs();

	//Commands
	void SubscribeEvents();

	void StartGame();
	void ExitGame();

	void LoadAttractScreenImages();

public:

	//Game States
	GameState m_gameState = GameState::ATTRACT_SCREEN;
	Clock* m_gameClock = nullptr;
	Player* m_player = nullptr;
	World* m_world = nullptr;

	bool m_shouldRebuildWorld;
	std::string m_pendingWorldName;

private:
	std::vector<TextureDX12*> m_attractScreenImages;
	Timer* m_attractTimer = nullptr;
	int m_numAttractScreenLoadingDots = 1;
	int m_currentAttractImageIndex = 0;
	int m_nextAttractImageIndex = 1;
	bool m_isFadingAttractImage = false;
	float m_attractImageHoldSeconds = 3.5f;
	float m_attractImageFadeSeconds = 1.0f;
	float m_attractImageHoldTimer = 0.0f;
	float m_attractImageFadeTimer = 0.0f;


	float m_lastFrameRenderDuration = 0.f;

	//Camera
	Camera* m_worldCamera = nullptr;
	Camera* m_skyBoxCamera = nullptr;
	Camera* m_screenCamera = nullptr;
	Vec2 m_worldCamBottomLeft;
	Vec2 m_worldCamTopRight;

	//Restart
	bool m_shouldRestart = false;

	//Audio
	GameMusic m_gameMusic;
	std::vector<std::string> m_gameMusicDataPaths;
	std::vector<std::string> m_sfxDataPaths;
	SoundPlaybackID* m_musicSoundPlaybacks [NUM_GAME_MUSIC];
	std::map<GameSFX, SoundPlaybackID> m_loopingSFXSoundPlaybackPairs;

	std::vector<Entity*> m_entities;

	//Time
	bool m_isSlowMo = false;
	double m_updateSpeed = 0.0;
	double m_renderSpeed = 0.0;
	float m_fps = 0.f;

	bool m_loadWorldInAttract = true;
};

