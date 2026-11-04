#include "weather_director.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kMinimumEaseSeconds = 0.1f;
constexpr float kLowSunFullBelow = 0.20f;
constexpr float kLowSunNoneAbove = 0.55f;

float Mix(float from, float to, float amount)
{
    return from + (to - from) * amount;
}

float EasedToward(float value, float target, double elapsed, float seconds)
{
    const float step = static_cast<float>(elapsed / std::max(seconds, kMinimumEaseSeconds));
    return value + std::clamp(target - value, -step, step);
}

float Blended(const float* blend, const WeatherProfile* profiles, float WeatherProfile::*member, float neutral)
{
    float value = neutral;
    for (int kind = 0; kind < kWeatherKinds; ++kind)
        value += blend[kind] * (profiles[kind].*member - neutral);
    return std::max(value, 0.0f);
}

float SmoothFall(float value, float fullBelow, float noneAbove)
{
    const float t = std::clamp((value - fullBelow) / (noneAbove - fullBelow), 0.0f, 1.0f);
    return 1.0f - t * t * (3.0f - 2.0f * t);
}
}

int WeatherKindOf(int32_t weatherType, float intensity)
{
    if (!std::isfinite(intensity) || intensity <= 0.0f)
        return kNoWeatherKind;
    switch (weatherType)
    {
    case kClientWeatherNone:
        return intensity >= kClientFogMinimumIntensity ? kWeatherFog : kNoWeatherKind;
    case kClientWeatherRain:
        return kWeatherRain;
    case kClientWeatherSnow:
        return kWeatherSnow;
    case kClientWeatherSand:
        return kWeatherSand;
    default:
        return kNoWeatherKind;
    }
}

void WeatherDirector::Advance(const WeatherReading& reading, const WeatherTuning& tuning, double elapsed)
{
    const int current = WeatherKindOf(reading.type, reading.intensity);
    const float target = current == kNoWeatherKind ? 0.0f : std::min(reading.intensity, 1.0f);
    for (int kind = 0; kind < kWeatherKinds; ++kind)
    {
        const float kindTarget = kind == current ? target : 0.0f;
        m_blend[kind] =
            elapsed < 0.0 ? kindTarget : EasedToward(m_blend[kind], kindTarget, elapsed, tuning.easeSeconds);
    }
    if (elapsed < 0.0)
        return;
    const float wetting = std::max(m_blend[kWeatherRain], m_blend[kWeatherSnow]);
    const float seconds = wetting > m_wetness ? tuning.wetSeconds : tuning.dryingSeconds;
    m_wetness = EasedToward(m_wetness, wetting, elapsed, seconds);
}

WeatherLook WeatherDirector::Step(const WeatherReading& reading, const WeatherTuning& tuning, double nowSeconds)
{
    const double elapsed = m_started ? std::max(nowSeconds - m_lastSeconds, 0.0) : -1.0;
    m_started = true;
    m_lastSeconds = nowSeconds;
    Advance(reading, tuning, elapsed);

    float scaled[kWeatherKinds];
    float cover = 0.0f;
    float totalBlend = 0.0f;
    for (int kind = 0; kind < kWeatherKinds; ++kind)
    {
        scaled[kind] = m_blend[kind] * std::max(tuning.strength, 0.0f);
        cover = std::max(cover, m_blend[kind]);
        totalBlend += m_blend[kind];
    }

    WeatherLook look;
    look.stormBlend = cover;
    look.sun = Blended(scaled, tuning.profiles, &WeatherProfile::sun, 1.0f);
    look.raysGain = Blended(scaled, tuning.profiles, &WeatherProfile::rays, 1.0f);
    look.exposure = Blended(scaled, tuning.profiles, &WeatherProfile::exposure, 1.0f);
    look.density = Blended(scaled, tuning.profiles, &WeatherProfile::density, 1.0f);
    look.drift = Blended(scaled, tuning.profiles, &WeatherProfile::drift, 1.0f);
    look.variation = Blended(scaled, tuning.profiles, &WeatherProfile::variation, 1.0f);
    look.tint = std::min(Blended(scaled, tuning.profiles, &WeatherProfile::tint, 0.0f), 1.0f);
    look.waves = Blended(scaled, tuning.profiles, &WeatherProfile::waves, 1.0f);
    look.wind = Blended(scaled, tuning.profiles, &WeatherProfile::wind, 1.0f);
    look.foam = Blended(scaled, tuning.profiles, &WeatherProfile::foam, 1.0f);
    look.glint = Blended(scaled, tuning.profiles, &WeatherProfile::glint, 1.0f);
    if (totalBlend > 0.0f)
        for (int channel = 0; channel < 3; ++channel)
        {
            float mixed = 0.0f;
            for (int kind = 0; kind < kWeatherKinds; ++kind)
                mixed += m_blend[kind] * tuning.tintColors[kind][channel];
            look.tintColor[channel] = mixed / totalBlend;
        }

    const float clearing = m_wetness * (1.0f - cover);
    look.wetness = m_wetness;
    look.density *= 1.0f + clearing * std::max(tuning.afterRainMist, 0.0f);
    look.variation *= 1.0f + clearing * std::max(tuning.afterRainMist, 0.0f);
    look.lowSunRays = std::max(tuning.lowSunRays, 0.0f) * (1.0f - cover);
    look.clearingRays = std::max(tuning.clearingRays, 0.0f) * clearing;
    return look;
}

WeatherLook NeutralWeatherLook(const WeatherLook& look, float outdoorShare)
{
    const float share = std::clamp(outdoorShare, 0.0f, 1.0f);
    WeatherLook out = look;
    out.sun = Mix(1.0f, look.sun, share);
    out.raysGain = Mix(1.0f, look.raysGain, share);
    out.lowSunRays = look.lowSunRays * share;
    out.clearingRays = look.clearingRays * share;
    out.exposure = Mix(1.0f, look.exposure, share);
    out.density = Mix(1.0f, look.density, share);
    out.drift = Mix(1.0f, look.drift, share);
    out.variation = Mix(1.0f, look.variation, share);
    out.tint = look.tint * share;
    out.waves = Mix(1.0f, look.waves, share);
    out.wind = Mix(1.0f, look.wind, share);
    out.foam = Mix(1.0f, look.foam, share);
    out.glint = Mix(1.0f, look.glint, share);
    return out;
}

float WeatherGodRayStrength(float configured, const WeatherLook& look, float sunHeight)
{
    const float lowSun = SmoothFall(sunHeight, kLowSunFullBelow, kLowSunNoneAbove);
    return std::max(configured * look.raysGain + look.lowSunRays * lowSun + look.clearingRays, 0.0f);
}
