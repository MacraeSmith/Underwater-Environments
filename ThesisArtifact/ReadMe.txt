
Procedural Generation and Rendering Techniques for Underwater Environments - Made by Macrae Smith Cohort 34

Description:
---------------------------------------------

This project is built entirely within my custom C++ game engine and focuses on creating a real-time, procedurally generated underwater world. Terrain is generated using layered noise and converted into 3D geometry, enabling complex natural features to emerge.

The world streams dynamically through a chunk-based, multithreaded system, allowing for an effectively endless environment. Rendering techniques simulate water, lighting, and atmospheric effects, while boid-based fish and procedural vegetation add motion and life.

An in-engine ImGui editor exposes all major parameters for real-time editing and iteration.


Instructions:
---------------------------------------------
Open up the ThesisArtifact_Release_x64.exe file in the Run folder to launch the game

or

Open the ThesisArtifact Solution in Visual Studio 2022 and press ctrl + shift + B to build, and then F5 to play the game. 


Keyboard Controls:
----------------Navigation------------------
[SPACE]			Enter Artifact
[ESC]			Exit
[P]			Pause


-------------------Debug--------------------

[F1]                    Toggle Debug Messages

[F2]                    Chunk Debug Views
                        - Mesh Chunks
                        - All Chunks
                        - Wireframe
                        - Fish Radii

[F3]                    Toggle Chunk Loading
[F5]                    Job and Memory Stats
[F8]                    Reload World
[0-9]                   Render Modes 0 - 9
[SHIFT] + [0-9]         Render Modes 10 - 19
[ARROWS]                Directional Light Direction
[TAB]                   Mouse Visibility

[C]                     Change Camera / Physics Mode
                        - Free Fly
                        - No Clip
                        - World Aligned
                        - Frustum Cull

[O]			Step Forward One Frame
[~]			Open Dev Console

-----------------Movement--------------------

[WASD]                  Move
[E / F]                 Move Up and Down
[MOUSE]                 Look Around
[SHIFT]                 Sprint
[SHIFT] + [MOUSE WHEEL] Change Sprint Speed




ImGui Main Features by Tab:
---------------Load Worlds-------------------
Handles world state management.

-Generate or rebuild the world using current settings
-Reset to default configuration
-Load recent unsaved world states
-Save and load worlds using XML files

This tab is the entry point for applying changes made elsewhere in the editor.

-----------------Terrain---------------------
Controls procedural terrain generation.

-World size, height, and voxel resolution
-Noise layers for shaping terrain (scale, octaves, strength, etc.)
-Biome blending and variation
-Texture assignment per biome (floor and walls)

Changes here affect the structure and visual composition of the world geometry.


-------------Water And Wind------------------
Controls ocean and current behavior.

-Ocean surface properties (waves, foam, reflection, refraction)
-Fine-tuned wave simulation using multiple layered waves
-Directional and procedural wind/current systems

This section defines both visual water appearance and environmental motion.


-------------Lightning and Fog---------------
Controls overall scene lighting and atmospheric effects.

-Sun direction, intensity, and color
-Ambient lighting
-Underwater attenuation and color absorption
-Fog above and below water
-Caustics, light rays, and particle effects

These settings strongly influence mood, visibility, and realism.


----------------Vegetation------------------
Controls procedural placement and appearance of plant life.

-Enable or disable vegetation globally
-Configure individual vegetation types (size, density, placement rules)
-Coral-specific controls and blending behavior
-Noise-based distribution for natural variation

This system determines how life populates the terrain.


-------------------Fish---------------------
Controls AI and spawning behavior for aquatic life.

-Global fish limits and performance constraints
-Per-species behavior settings:
	-Movement (speed, turning)
	-Schooling (cohesion, alignment, separation)
	-Terrain avoidance
	-Interaction with the player

This tab governs dynamic ecosystem behavior.


---------------Debug Render-----------------
Provides visualization tools for rendering diagnostics.

-Switch between different shader outputs (albedo, normals, depth, etc.)
-Inspect internal rendering data such as biome weights and lighting

Used primarily for debugging graphics and shading.



