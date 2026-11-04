#pragma once

#include <cstdint>

constexpr int32_t kClientWeatherNone = 0;
constexpr int32_t kClientWeatherRain = 1;
constexpr int32_t kClientWeatherSnow = 2;
constexpr int32_t kClientWeatherSand = 3;
constexpr float kClientFogMinimumIntensity = 0.27f;

enum WeatherKind
{
    kWeatherRain,
    kWeatherSnow,
    kWeatherSand,
    kWeatherFog,
    kWeatherKinds
};
constexpr int kNoWeatherKind = -1;

struct WeatherReading
{
    int32_t type = kClientWeatherNone;
    float intensity = 0.0f;
    float dayFraction = 0.5f;
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
};

int WeatherKindOf(int32_t weatherType, float intensity);
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
    float Flash(const WeatherTuning& tuning, double nowSeconds);
    float NextUnit();

    bool m_started = false;
    double m_lastSeconds = 0.0;
    float m_blend[kWeatherKinds] = {};
    float m_wetness = 0.0f;
    double m_nextStrike = 0.0;
    double m_lastStrike = -1000.0;
    uint32_t m_random = 0x51F15EEDu;
};
