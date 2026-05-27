#include "Game/WorldUtils.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Math/Curves.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Renderer/RendererDX12.hpp"
#include "Engine/Core/Image.hpp"

std::string GetCurveTypeName(Curve<float>* curve, int& out_exponent)
{
	out_exponent = -1;
	LinearCurve<float>* linearCurve = dynamic_cast<LinearCurve<float>*>(curve);
	if (linearCurve)
		return "linear";

	SmoothStep3Curve<float>* smoothStep3Curve = dynamic_cast<SmoothStep3Curve<float>*>(curve);
	if (smoothStep3Curve)
		return "smoothStep3";

	SmoothStep5Curve<float>* smoothStep5Curve = dynamic_cast<SmoothStep5Curve<float>*>(curve);
	if (smoothStep5Curve)
		return "smoothStep5";

	Hesitate3Curve<float>* hesitate3Curve = dynamic_cast<Hesitate3Curve<float>*>(curve);
	if (hesitate3Curve)
		return "hesitate3";

	Hesitate5Curve<float>* hesitate5Curve = dynamic_cast<Hesitate5Curve<float>*>(curve);
	if (hesitate5Curve)
		return "hesitate5";

	SmoothStartCurve<float>* smoothStartCurve = dynamic_cast<SmoothStartCurve<float>*>(curve);
	if (smoothStartCurve)
	{
		out_exponent = smoothStartCurve->m_exponent;
		return "smoothStart";
	}

	SmoothStopCurve<float>* smoothStopCurve = dynamic_cast<SmoothStopCurve<float>*>(curve);
	if (smoothStopCurve)
	{
		out_exponent = smoothStopCurve->m_exponent;
		return "smoothStop";
	}

	EaseInBackCurve<float>* easeInBackCurve = dynamic_cast<EaseInBackCurve<float>*>(curve);
	if (easeInBackCurve)
		return"easeInBack";

	EaseOutBackCurve<float>* easeOutBackCurve = dynamic_cast<EaseOutBackCurve<float>*>(curve);
	if (easeOutBackCurve)
		return"easeOutBack";

	return "linear";
}

void LoadChildrenTexturePathsFromParentFolder(TextureFolderPaths& filePaths, std::string const& parentFolderPath)
{
	Strings filePathStrings = SplitStringOnDelimiter(parentFolderPath, '/', true);
	std::string texFileAppend = filePathStrings[filePathStrings.size() - 1];
	std::string texFullFile = Stringf("%s/%s", parentFolderPath.c_str(), texFileAppend.c_str());
	filePaths.m_parentFolder = parentFolderPath;

	filePaths.m_albedoFile = Stringf("%s_Albedo.png", texFullFile.c_str());
	filePaths.m_normalFile = Stringf("%s_Normal.png", texFullFile.c_str());
	filePaths.m_aoFile = Stringf("%s_AO.png", texFullFile.c_str());

	if (!FileExists(filePaths.m_albedoFile))
	{
		filePaths.m_albedoFile = UN_INITIALIZED_WORLD_DATA_NAME;
	}

	if (!FileExists(filePaths.m_normalFile))
	{
		filePaths.m_normalFile = UN_INITIALIZED_WORLD_DATA_NAME;
	}

	if (!FileExists(filePaths.m_aoFile))
	{
		filePaths.m_aoFile = UN_INITIALIZED_WORLD_DATA_NAME;
	}
}



std::string GetWindTypeNameFromType(WindType type)
{
	switch (type)
	{
	case WindType::WIND: return "Wind";
	case WindType::UNDER_WATER_CURRENT: return "Underwater Current";
	}
	return UN_INITIALIZED_WORLD_DATA_NAME;
}

WindType GetWindTypeFromName(std::string const& name)
{
	if (name == "Wind")
		return  WindType::WIND;
	if (name == "Underwater Current")
		return WindType::UNDER_WATER_CURRENT;
	return WindType::UN_INITIALIZED;
}

std::string GetFishTypeNameFromType(FishType type)
{
	switch (type)
	{
	case FishType::ANGEL_FISH: return "AngelFish";
	case FishType::CHROMIS: return "Chromis";
	case FishType::SHARK: return "Shark";
	default: return UN_INITIALIZED_WORLD_DATA_NAME;
	}
}

FishType GetFishTypeFromName(std::string const& name)
{
	if(name == "AngelFish")
		return FishType::ANGEL_FISH;
	if(name =="Chromis")
		return FishType::CHROMIS;
	if(name == "Shark")
		return FishType::SHARK;

	return FishType::UN_INITIALIZED;
}

WindType GetWindTypeFromVegetationType(VegetationType vegType)
{
	switch (vegType)
	{
	case VegetationType::GRASS:  return WindType::WIND;
	}
	return WindType::UNDER_WATER_CURRENT;
}


Biome GetAboveWaterBiome(float temperature)
{
	if (temperature >= 0.6f)
	{
		return Biome::GRASSY_BEACH;
	}

	return Biome::BEACH;
}

Biome GetShallowBiome(float rockiness, float temperature, float kelpMask)
{
	if (kelpMask >= 0.7f)
	{
		return Biome::KELP_FOREST;
	}

	if (rockiness >= 0.5f)
	{
		return Biome::ROCKY_SAND;
	}

	if (temperature >= 0.6f)
	{
		return Biome::GRASSY_SAND;
	}

	return Biome::SANDY_PLAINS;
}

Biome GetMidBiome(float rockiness, float temperature, float coralMask, float kelpMask)
{
	if (coralMask >= 0.7f)
	{
		return Biome::CORAL_GARDEN;
	}

	if (kelpMask >= 0.7f)
	{
		return Biome::KELP_FOREST;
	}

	if (rockiness >= 0.55f)
	{
		return Biome::ROCKY_SAND;
	}

	if (temperature >= 0.55f)
	{
		return Biome::MOSSY_ROCK;
	}

	return Biome::SMOOTH_ROCK;
}

Biome GetDeepBiome(float temperature, float coralMask)
{
	if (coralMask >= 0.75f)
	{
		return Biome::CORAL_GARDEN;
	}

	if (temperature >= 0.55f)
	{
		return Biome::MOSSY_ROCK;
	}

	return Biome::SMOOTH_ROCK;
}



void MeshTextures::LoadTexturesFromMeshImages(MeshImages const& meshImages, int mipLevels)
{
	if (meshImages.m_diffuseImage && !m_diffuseTex)
	{
		m_diffuseTex = g_renderer->CreateOrGetTextureFromImage(*meshImages.m_diffuseImage, mipLevels);
	}

	if (meshImages.m_normalImage && !m_normalTex)
	{
		m_normalTex = g_renderer->CreateOrGetTextureFromImage(*meshImages.m_normalImage, mipLevels);
	}

	if (meshImages.m_specularImage && !m_specularTex)
	{
		m_specularTex = g_renderer->CreateOrGetTextureFromImage(*meshImages.m_specularImage, mipLevels);
	}

	if (meshImages.m_aoImage && !m_aoTex)
	{
		m_aoTex = g_renderer->CreateOrGetTextureFromImage(*meshImages.m_aoImage, mipLevels);
	}

	if (meshImages.m_emissiveImage && !m_emissiveTex)
	{
		m_emissiveTex = g_renderer->CreateOrGetTextureFromImage(*meshImages.m_emissiveImage, mipLevels);
	}
}

std::string FishInfo::GetFolderName() const
{
	Strings filePathStrings = SplitStringOnDelimiter(m_meshInfo, '/');
	if (filePathStrings.size() <= 0)
		return "INVALID_FOLDER";

	return Stringf("/%s", filePathStrings[filePathStrings.size() - 1].c_str());
}



void FishInstanceData::UpdateInfo(Vec3 position, Vec3 forwardNormal, float speed, FishInfo const& fishInfo)
{
	m_position = position;
	m_forwardNormal = forwardNormal;

	float t = GetClampedFractionWithinRange(speed, 0.f, 100.f);

	m_frequency = Lerp(0.0f, 5.f, 0.5f);

	float modelLength = fishInfo.m_swimConstants.m_modelLength;
	float minAmp = 0.02f * modelLength;
	float maxAmp = 0.08f * modelLength;

	m_amplitude = Lerp(minAmp, maxAmp, t * t);

	float baseWaveLength = modelLength * 0.9f;
	float sprintWaveShortening = modelLength * 0.15f;
	m_waveLength = baseWaveLength - (sprintWaveShortening * t);

	constexpr float minTailPower = 1.2f;
	constexpr float maxTailPower = 3.0f;

	m_tailPower = Lerp(minTailPower, maxTailPower, t);
}
