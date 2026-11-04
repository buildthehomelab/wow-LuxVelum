#pragma once

#include "sky_clouds.h"
#include "weather_director.h"

#include <string>

constexpr float kEngineFarClipMin = 183.333f;
constexpr float kEngineFarClipMax = 1583.33f;
constexpr float kFarClipMaxKeepsClientCap = 0.0f;

constexpr unsigned kVirtualKeyF7 = 0x76;

struct Hotkey
{
    unsigned virtualKey;
    bool ctrl;
    bool shift;
    bool alt;
};

constexpr Hotkey kDefaultOverlayHotkey = {kVirtualKeyF7, true, false, false};

bool ParseHotkey(const char* text, Hotkey& out);
std::string HotkeyName(const Hotkey& key);

struct Config
{
    bool enable = true;
    bool hooks = true;
    bool lightStay = true;
    bool overlay = true;
    Hotkey overlayKey = kDefaultOverlayHotkey;
    int quality = 2;
    float density = 1.0f;
    float haze = 1.0f;
    float groundFog = 0.6f;
    float farFog = 1.0f;
    int stockFog = 1;
    bool transparentFog = false;
    int dataMode = 1;
    int colorSpace = 1;
    int classicPhase = 0;
    float sunScatter = 1.0f;
    float ambient = 1.0f;
    float exposure = 1.0f;
    float classicExposure = 1.0f;
    bool localLights = true;
    float localLightIntensity = 1.0f;
    float localLightPhase = 0.3f;
    bool interiorAware = true;
    float interiorDensity = 0.15f;
    float interiorFarFog = 1.0f;
    float noiseAmount = 0.15f;
    float noiseScale = 0.025f;
    float noiseWindSpeed = 0.5f;
    bool classicNoise = true;
    bool weatherEffects = true;
    bool stormClouds = true;
    float cloudPreviewWeight = 0.0f;
    float cloudPreviewStorm = 0.0f;
    float cloudFairCover = 0.0f;
    float cloudDarkness = 1.0f;
    float cloudOpacity = 1.0f;
    float cloudWind = 1.0f;
    float weatherBolts = 1.0f;
    bool weatherThunder = true;
    float weatherStrength = 1.0f;
    float weatherEaseSeconds = 30.0f;
    float weatherAfterRainMist = 0.5f;
    float weatherDryingSeconds = 600.0f;
    float weatherLowSunRays = 0.5f;
    float weatherClearingRays = 1.0f;
    float weatherRainRings = 1.0f;
    float weatherLightning = 0.8f;
    float weatherDawnMist = 0.4f;
    float rainSun = 0.35f, rainRays = 0.0f, rainExposure = 0.85f, rainDensity = 1.25f, rainDrift = 2.5f,
          rainVariation = 2.0f, rainTint = 0.0f, rainWaves = 1.35f, rainWind = 2.0f, rainFoam = 1.6f, rainGlint = 0.25f;
    float snowSun = 0.5f, snowRays = 0.0f, snowExposure = 1.05f, snowDensity = 1.3f, snowDrift = 0.6f,
          snowVariation = 1.2f, snowTint = 0.35f, snowWaves = 0.8f, snowWind = 0.8f, snowFoam = 0.8f, snowGlint = 0.4f;
    float sandSun = 0.4f, sandRays = 0.0f, sandExposure = 0.9f, sandDensity = 1.5f, sandDrift = 3.0f,
          sandVariation = 2.5f, sandTint = 0.65f, sandWaves = 1.2f, sandWind = 1.5f, sandFoam = 1.2f, sandGlint = 0.3f;
    float ashSun = 0.45f, ashRays = 0.0f, ashExposure = 0.8f, ashDensity = 1.4f, ashDrift = 0.5f,
          ashVariation = 1.3f, ashTint = 0.55f, ashWaves = 1.0f, ashWind = 1.0f, ashFoam = 1.0f, ashGlint = 0.5f;
    float fogSun = 0.45f, fogRays = 0.0f, fogExposure = 0.95f, fogDensity = 1.8f, fogDrift = 1.0f,
          fogVariation = 1.5f, fogTint = 0.5f, fogWaves = 0.8f, fogWind = 0.6f, fogFoam = 0.8f, fogGlint = 0.5f;
    float godRays = 0.0f;
    bool glowCompensation = true;
    int foreverGlow = 0;
    float colorGrading = 0.0f;
    float farClipMax = kFarClipMaxKeepsClientCap;
    float maxDistance = 5000.0f;
    float temporal = 0.85f;
    bool underwater = false;
    bool liquidDepth = true;
    bool multisampling = true;
    int debugView = 0;
    bool sunMarker = false;
    int logLevel = 1;
    bool water = true;
    int waterQuality = 2;
    float waterWaves = 1.0f;
    float waterWind = 2.0f;
    float waterFoam = 1.0f;
    float waterReflections = 1.0f;
    float waterSpecular = 1.0f;
    float waterClarity = 1.0f;
    float waterZoneColors = 0.5f;
    float waterRipples = 0.5f;
    bool waterClientSplashes = false;
    int waterDebugView = 0;
};

WeatherTuning WeatherTuningFrom(const Config& config);
CloudTuning CloudTuningFrom(const Config& config);
bool SameFogSettings(const Config& a, const Config& b);
bool SameLiveSettings(const Config& a, const Config& b);
std::string SettingChanges(const Config& before, const Config& after);

class ConfigStore
{
public:
    void Load(const std::string& path);
    bool ReloadIfChanged();
    void Override(const Config& config);
    void Apply(const Config& edited);
    void LogSettledEdits();
    bool Save();
    void Revert();
    bool HasUnsavedChanges() const { return !SameLiveSettings(m_config, m_saved); }
    const Config& Get() const { return m_config; }
    const std::string& Path() const { return m_path; }

private:
    void Read();
    Config ReadFile() const;
    void ReadKeepingStartupSwitches();
    bool LogChanges(const char* origin);

    std::string m_path;
    unsigned long long m_stamp = 0;
    Config m_config;
    Config m_saved;
    Config m_logged;
};

const char* AdoptEarlierSettings(const std::string& directory);
ConfigStore& GlobalConfig();
