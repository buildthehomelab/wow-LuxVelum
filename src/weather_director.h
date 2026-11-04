#pragma once

#include <cstdint>

constexpr int32_t kClientWeatherNone = 0;
constexpr int32_t kClientWeatherRain = 1;
constexpr int32_t kClientWeatherSnow = 2;
constexpr int32_t kClientWeatherSand = 3;
constexpr int32_t kClientWeatherNoRequest = -1;
constexpr float kClientFogMinimumIntensity = 0.27f;

enum WeatherKind
{
    kWeatherRain,
    kWeatherSnow,
    kWeatherSand,
    kWeatherFog,
    kWeatherAsh,
    kWeatherKinds
};
constexpr int kNoWeatherKind = -1;

struct WeatherReading
{
    int32_t type = kClientWeatherNone;
    float intensity = 0.0f;
    bool ash = false;
    float dayFraction = 0.5f;
    int mapId = 0;
    float position[2] = {};
    float overcastWeight = 0.0f;
    float thunderOvercast = 0.0f;
    bool strikeRequested = false;
};

struct WeatherProfile
{
    float sun = 1.0f;
    float rays = 1.0f;
    float exposure = 1.0f;
    float density = 1.0f;
    float drift = 1.0f;
    float variation = 1.0f;
    float tint = 0.0f;
    float waves = 1.0f;
    float wind = 1.0f;
    float foam = 1.0f;
    float glint = 1.0f;
};

struct WeatherTuning
{
    WeatherProfile profiles[kWeatherKinds];
    float strength = 1.0f;
    float easeSeconds = 30.0f;
    float wetSeconds = 120.0f;
    float dryingSeconds = 600.0f;
    float afterRainMist = 0.5f;
    float lowSunRays = 0.0f;
    float clearingRays = 0.0f;
    float rainRings = 0.0f;
    float lightning = 0.0f;
    float dawnMist = 0.0f;
    float tintColors[kWeatherKinds][3] = {};
};

struct WeatherLook
{
    float stormBlend = 0.0f;
    float sun = 1.0f;
    float raysGain = 1.0f;
    float lowSunRays = 0.0f;
    float clearingRays = 0.0f;
    float exposure = 1.0f;
    float density = 1.0f;
    float drift = 1.0f;
    float variation = 1.0f;
    float tint = 0.0f;
    float tintColor[3] = {1.0f, 1.0f, 1.0f};
    float waves = 1.0f;
    float wind = 1.0f;
    float foam = 1.0f;
    float glint = 1.0f;
    float wetness = 0.0f;
    float rainDropsPerSecond = 0.0f;
    float flash = 0.0f;
    float strikeSeed = 0.0f;
    bool strikeAsked = false;
};

int32_t LiveClientWeatherType(int32_t requested, bool rainEffect, bool snowEffect, bool sandEffect);
bool ClientWeatherIsAsh(int32_t type, float transition, const char* texture);
int WeatherKindOf(int32_t weatherType, float intensity, bool ash);
WeatherLook NeutralWeatherLook(const WeatherLook& look, float outdoorShare);
float WeatherLightningEnvelope(double sinceStrike);
float WeatherDawnWeight(float dayFraction);
float WeatherGodRayStrength(float configured, const WeatherLook& look, float sunHeight);

class WeatherDirector
{
public:
    WeatherLook Step(const WeatherReading& reading, const WeatherTuning& tuning, double nowSeconds);
    float KindBlend(int kind) const { return m_blend[kind]; }

private:
    void Advance(const WeatherReading& reading, const WeatherTuning& tuning, double elapsed);
    float LocalWetness(const WeatherReading& reading) const;
    bool LightningWeather(const WeatherReading& reading, double nowSeconds);
    float Flash(const WeatherReading& reading, const WeatherTuning& tuning, double nowSeconds);
    void Strike(double nowSeconds, bool asked);
    float NextUnit();

    bool m_started = false;
    int m_lastMapId = 0;
    double m_lastSeconds = 0.0;
    float m_blend[kWeatherKinds] = {};
    float m_wetness = 0.0f;
    bool m_hasWetPlace = false;
    int m_wetMapId = 0;
    float m_wetPosition[2] = {};
    double m_nextStrike = 0.0;
    double m_lastStrike = -1000.0;
    float m_strikeSeed = 0.0f;
    bool m_strikeAsked = false;
    double m_lastAshSeconds = -1000.0;
    uint32_t m_random = 0x51F15EEDu;
};
