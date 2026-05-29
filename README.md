# Procedural Generation and Rendering Techniques for Underwater Environments

<p align="center">
  <img src="Media/CoverImage_001.png" width="100%">
</p>

<p align="center">
  <strong>Master's Thesis | Custom C++ Engine | DirectX 12</strong>
</p>

## Overview

This project was developed as my Master's Thesis at SMU Guildhall and built entirely within a custom C++ game engine.

The project explores the procedural generation and real-time rendering of large-scale underwater environments. Terrain is generated using layered noise functions and converted into 3D geometry, allowing natural underwater formations, caves, and landscapes to emerge dynamically.

A chunk-based multithreaded streaming system continuously generates and updates the world, enabling effectively endless exploration. The environment is brought to life through advanced rendering techniques, procedural vegetation systems, and boid-driven fish AI.

## Technical Deep Dive

For a detailed breakdown of the architecture, rendering systems, procedural generation techniques, and development process behind this project, visit the full project page:

**https://www.macraesmith.com/projects/thesis**

---

## Technologies

* C++
* DirectX 12
* HLSL
* ImGui
* Custom Game Engine
* Multi-threaded Job System
* Procedural Generation
* Boid-Based AI

---

## Key Features

### Procedural Terrain Generation

* Layered noise-based terrain generation
* Biome blending and variation
* Dynamic mesh generation
* Configurable world size and voxel resolution

### Infinite World Streaming

* Chunk-based world architecture
* Multithreaded generation pipeline
* Dynamic loading and unloading of terrain

### Advanced Underwater Rendering

* Water surface simulation
* Reflections and refractions
* Underwater light attenuation
* Caustics and volumetric light rays
* Atmospheric fog systems

### Procedural Ecosystem

* Boid-based fish schooling behaviors
* Procedural vegetation placement
* Coral generation and distribution
* Environment-aware spawning systems

### Real-Time Editor

* Full ImGui-based editing tools
* Live parameter adjustment
* Save and load world configurations
* Rapid iteration workflow

---

## Screenshots

<p align="center">
  <img src="Media/CoverImage_003.png" width="90%">
</p>

<p align="center">
  <img src="Media/CoverImage_005.png" width="90%">
</p>

---

## Running the Project

### Launch Prebuilt Executable

```text
ThesisArtifact/Run/ThesisArtifact_Release_x64.exe
```

### Build From Source

1. Open the solution in Visual Studio 2022
2. Build:

```text
Ctrl + Shift + B
```

3. Run:

```text
F5
```

Recommended build configurations:

* Release
* Fast_Break

---

## Controls

### General

| Key   | Action         |
| ----- | -------------- |
| Space | Enter Artifact |
| Esc   | Exit           |
| P     | Pause          |

### Movement

| Key                 | Action              |
| ------------------- | ------------------- |
| WASD                | Move                |
| E / F               | Move Up / Down      |
| Mouse               | Look Around         |
| Shift               | Sprint              |
| Shift + Mouse Wheel | Adjust Sprint Speed |

### Camera Modes

| Key | Action             |
| --- | ------------------ |
| C   | Change Camera Mode |

Available Modes:

* Free Fly
* No Clip
* World Aligned
* Frustum Cull

### Debug

| Key         | Action                      |
| ----------- | --------------------------- |
| F1          | Toggle Debug Messages       |
| F2          | Chunk Debug Views           |
| F3          | Toggle Chunk Loading        |
| F5          | Job and Memory Statistics   |
| F8          | Reload World                |
| 0-9         | Render Modes 0-9            |
| Shift + 0-9 | Render Modes 10-19          |
| Arrow Keys  | Directional Light Direction |
| Tab         | Toggle Mouse Visibility     |
| O           | Step Forward One Frame      |
| ~           | Developer Console           |

---

## Editor Features

### Load Worlds

* Generate worlds
* Save and load XML configurations
* Restore previous world states

### Terrain

* Terrain generation settings
* Biome controls
* Texture assignment

### Water and Wind

* Wave simulation
* Ocean rendering
* Current systems

### Lighting and Fog

* Sun and ambient lighting
* Underwater attenuation
* Fog and atmospheric effects
* Caustics and light rays

### Vegetation

* Procedural placement
* Density controls
* Coral systems

### Fish AI

* Schooling behavior
* Movement tuning
* Terrain avoidance
* Player interaction

### Debug Rendering

* Albedo visualization
* Normal visualization
* Depth visualization
* Lighting diagnostics

