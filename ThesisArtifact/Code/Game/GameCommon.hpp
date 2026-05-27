#pragma once
#include <string>

class Renderer;
class RendererDX12;
class App;
class RandomNumberGenerator;
class InputSystem;
struct Rgba8;
class AudioSystem;
class Window;
class Game;
class JobSystem;

extern RendererDX12* g_renderer;
extern App* g_app;
extern Game* g_game;
extern RandomNumberGenerator* g_rng;
extern InputSystem* g_inputSystem;
extern AudioSystem* g_audioSystem;
extern Window* g_window;
extern JobSystem* g_jobSystem;

extern bool g_freeMouse;
extern int g_debugShaderInt;
extern bool g_invertMouseX;
extern bool g_invertMouseY;
extern float g_mouseSensitivity;

extern bool g_showGameDebugMessages;

extern bool g_invertControllerViewX;
extern bool g_invertControllerViewY;
extern float g_controllerSensitivity;

//Screen Size
constexpr float SCREEN_SIZE_X = 1600.f;
constexpr float SCREEN_SIZE_Y = 800.f;
constexpr float SCREEN_CENTER_X = SCREEN_SIZE_X / 2.f;
constexpr float SCREEN_CENTER_Y = SCREEN_SIZE_Y / 2.f;


constexpr int MAX_NUM_WORLD_JOBS = 100;
constexpr int NUM_MESHBUFFER_BUILDS_PER_FRAME = 7;
constexpr int CHUNK_ACTIVATION_RANGE = 1500;

constexpr int MAX_NUM_CHUNKS_DELETE_PER_FRAME = 10;

constexpr int NUM_VEGETATION_BUILDS_PER_FRAME = 1;
constexpr int VEGETATION_ACTIVATION_RANGE = (int)(CHUNK_ACTIVATION_RANGE / 3);

//CHUNKS
constexpr int CHUNK_BITS_X = 6;
constexpr int CHUNK_BITS_Y = 6;
constexpr int CHUNK_BITS_Z = 6;

constexpr int CHUNK_SIZE_X = 1 << CHUNK_BITS_X;
constexpr int CHUNK_SIZE_Y = 1 << CHUNK_BITS_Y;
constexpr int CHUNK_SIZE_Z = 1 << CHUNK_BITS_Z;
constexpr int NUM_BLOCKS_IN_CHUNK_2D = CHUNK_SIZE_X * CHUNK_SIZE_Y;
constexpr int NUM_BLOCKS_IN_CHUNK = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;

constexpr int NUM_NOISE_VOXELS_X = (CHUNK_SIZE_X + 2);
constexpr int NUM_NOISE_VOXELS_Y = (CHUNK_SIZE_Y + 2);
constexpr int NUM_NOISE_VOXELS_Z = (CHUNK_SIZE_Z + 2);
constexpr int NUM_NOISE_VOXELS_2D = NUM_NOISE_VOXELS_X * NUM_NOISE_VOXELS_Y;
constexpr int NUM_NOISE_VOXELS_3D = NUM_NOISE_VOXELS_2D * NUM_NOISE_VOXELS_Z;

constexpr int CHUNK_MAX_X = CHUNK_SIZE_X - 1;
constexpr int CHUNK_MAX_Y = CHUNK_SIZE_Y - 1;
constexpr int CHUNK_MAX_Z = CHUNK_SIZE_Z - 1;


// Strides (these are *value increments*, not bits)
constexpr int STRIDE_X = 1;
constexpr int STRIDE_Y = (1 << CHUNK_BITS_X);
constexpr int STRIDE_Z = (1 << (CHUNK_BITS_X + CHUNK_BITS_Y));

// Bit offsets for each axis in packed index
constexpr int BITS_STRIDE_X = 0;
constexpr int BITS_STRIDE_Y = CHUNK_BITS_X;
constexpr int BITS_STRIDE_Z = CHUNK_BITS_X + CHUNK_BITS_Y;

constexpr int CHUNK_MASK_X = CHUNK_SIZE_X - 1;
constexpr int CHUNK_MASK_Y = (CHUNK_SIZE_Y - 1) << BITS_STRIDE_Y;
constexpr int CHUNK_MASK_Z = (CHUNK_SIZE_Z - 1) << BITS_STRIDE_Z;

constexpr int CHUNK_DEACTIVATION_RANGE = CHUNK_ACTIVATION_RANGE + CHUNK_SIZE_X + CHUNK_SIZE_Y + CHUNK_SIZE_Z;
/*
constexpr int CHUNK_ACTIVATION_RADIUS_X = 1 + (int)(CHUNK_ACTIVATION_RANGE / CHUNK_SIZE_X);
constexpr int CHUNK_ACTIVATION_RADIUS_Y = 1 + (int)(CHUNK_ACTIVATION_RANGE / CHUNK_SIZE_Y);
constexpr int CHUNK_ACTIVATION_RADIUS_Z = 1 + (int)((CHUNK_ACTIVATION_RANGE / CHUNK_SIZE_Z));
constexpr int MAX_ACTIVE_CHUNKS = (2 * CHUNK_ACTIVATION_RADIUS_X) * (2 * CHUNK_ACTIVATION_RADIUS_Y) * 5;
*/

constexpr int VEGETATION_ACTIVATION_RADIUS_X = 1 + (int)(VEGETATION_ACTIVATION_RANGE / CHUNK_SIZE_X);
constexpr int VEGETATION_ACTIVATION_RADIUS_Y = 1 + (int)(VEGETATION_ACTIVATION_RANGE / CHUNK_SIZE_Y);
constexpr int VEGETATION_ACTIVATION_RADIUS_Z = 1 + (int)((VEGETATION_ACTIVATION_RANGE / CHUNK_SIZE_Z));

constexpr float MAX_VEGETATION_THICKNESS_METERS = 1.f;
constexpr float MIN_VEGETATION_THICKNESS_METERS = 10.f;
constexpr float SEAFAN_MESH_HEIGHT = 20.f;

constexpr int NUM_ANENOME_IN_CLUSTER = 10;
constexpr float ANEMONE_CLUSTER_RADIUS = 20.f;
constexpr int NUM_GRASS_IN_CLUSTER = 10;
constexpr float GRASS_CLUSTER_RADIUS = 20.f;

//Fog
constexpr int FOG_VOLUME_TEX_DIMS_X = 750;
constexpr int FOG_VOLUME_TEX_DIMS_Y = 350;
constexpr int FOG_VOLUME_TEX_DEPTH = 75;

//Camera
constexpr float CAMERA_FAR_DISTANCE = 500.f;
constexpr float SKYBOX_FAR_DISTANCE= 5000.f;

constexpr int MAX_FISH_SPAWNED_IN_CHUNK = 50;

const std::string UN_INITIALIZED_WORLD_DATA_NAME = "Unitialized World Data Name";
constexpr int NUM_TERRAIN_MIPS = 12;
constexpr int NUM_VEGETATION_MIPS = 10;

enum class GAME_ROOT_PARAMETERS : int
{
	PER_FRAME_CONSTANTS,	//0 - b1
	CAMERA_CONSTANTS,		//1 - b2
	MODEL_CONSTANTS,			//2 - b3
	LIGHTS,					//3 - b4

	TEXTURES,				//4 - b5
	STRUCTURED_BUFFERS,		//5 - b6
	FOG = 5,
	CAUSTICS,				//6 - b7
	LIGHT_RAYS = 6,			
	WATER,					//7 - b8
	FISH,					//8 - b9
	FISH_TYPE_INSTANCE,		//9 - b10

	VEGETATION,				//10 - b11
	WIND,					//11 - b12
	CORAL_CONSTANTS = 11,
	CLIP_TO_WORLD,			//12 - b13
	PARTICLES,				//13 - b14
	DEBUG_RENDER,			//14 - b15

};

struct DebugRenderConstants
{
	int m_default = 0;
	int m_albedo = 1;
	int m_normals = 2;
	int m_ambientOcclusion = 3;
	int m_lightStrength = 4;

	int m_noNormals = 5;
	int m_noAmbientOcclusion = 6;

	int m_specularAttenuation = 7;
	int m_noSpecularAttenuation = 8;

	int m_sceneDepth = 9;
	int m_emission = 10;

	int m_biomeWeight1 = 11;
	int m_biomeWeight2 = 12;
	int m_biomeWeight3 = 13;
	int m_biomeWeight4 = 14;
	int m_strongestBiome = 15;

	int m_fogVolumeMasks = 16;


};




