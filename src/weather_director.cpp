#include "weather_director.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace
{
constexpr float kMinimumEaseSeconds = 0.1f;
constexpr float kLowSunFullBelow = 0.20f;
constexpr float kLowSunNoneAbove = 0.55f;
constexpr float kFullRainDropsPerSecond = 180.0f;
constexpr float kStormRainBlend = 0.7f;
constexpr double kShortestStrikeGapSeconds = 8.0;
constexpr double kLongestStrikeGapSeconds = 35.0;
constexpr double kFirstStrokeSeconds = 0.09;
constexpr double kSecondStrokeDelay = 0.18;
constexpr double kSecondStrokeSeconds = 0.12;
constexpr float kSecondStrokeShare = 0.7f;
constexpr double kEnvelopeEndSeconds = 1.0;
constexpr float kDawnStart = 4.5f / 24.0f;
constexpr float kDawnPeak = 6.5f / 24.0f;
constexpr float kDawnEnd = 9.0f / 24.0f;
constexpr float kWetFullWithin = 150.0f;
constexpr float kWetNoneBeyond = 500.0f;
constexpr double kFlashRiseSeconds = 0.03;
constexpr float kLightestThunderOvercast = 0.4f;
constexpr double kAshOvercastFadeSeconds = 15.0;

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

int32_t LiveClientWeatherType(int32_t requested, bool rainEffect, bool snowEffect, bool sandEffect)
{
    if (requested > kClientWeatherNone)
        return requested;
    if (rainEffect)
        return kClientWeatherRain;
    if (snowEffect)
        return kClientWeatherSnow;
    if (sandEffect)
        return kClientWeatherSand;
    return kClientWeatherNone;
}

bool ClientWeatherIsAsh(int32_t type, float transition, const char* texture)
{
    if (type == kClientWeatherSand)
        return transition == 0.0f;
    if (type != kClientWeatherSnow || !texture)
        return false;
    for (const char* c = texture; c[0] && c[1] && c[2]; ++c)
        if (std::tolower(static_cast<unsigned char>(c[0])) == 'a' &&
            std::tolower(static_cast<unsigned char>(c[1])) == 's' &&
            std::tolower(static_cast<unsigned char>(c[2])) == 'h')
            return true;
    return false;
}

int WeatherKindOf(int32_t weatherType, float intensity, bool ash)
{
    if (!std::isfinite(intensity) || intensity <= 0.0f)
        return kNoWeatherKind;
    if (ash && weatherType > kClientWeatherNone)
        return kWeatherAsh;
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
    const int current = WeatherKindOf(reading.type, reading.intensity, reading.ash);
    const float target = current == kNoWeatherKind ? 0.0f : std::min(reading.intensity, 1.0f);
    const bool applyAtOnce = elapsed < 0.0 || reading.mapId != m_lastMapId;
    m_lastMapId = reading.mapId;
    for (int kind = 0; kind < kWeatherKinds; ++kind)
    {
        const float kindTarget = kind == current ? target : 0.0f;
        m_blend[kind] = applyAtOnce ? kindTarget : EasedToward(m_blend[kind], kindTarget, elapsed, tuning.easeSeconds);
    }
    if (elapsed < 0.0)
        return;
    if (m_hasWetPlace && reading.mapId != m_wetMapId)
    {
        m_wetness = 0.0f;
        m_hasWetPlace = false;
    }
    const float wetting = std::max(m_blend[kWeatherRain], m_blend[kWeatherSnow]);
    if (wetting > 0.0f)
    {
        m_wetness = LocalWetness(reading);
        m_hasWetPlace = true;
        m_wetMapId = reading.mapId;
        m_wetPosition[0] = reading.position[0];
        m_wetPosition[1] = reading.position[1];
    }
    const float seconds = wetting > m_wetness ? tuning.wetSeconds : tuning.dryingSeconds;
    m_wetness = EasedToward(m_wetness, wetting, elapsed, seconds);
}

float WeatherDirector::LocalWetness(const WeatherReading& reading) const
{
    if (!m_hasWetPlace)
        return 0.0f;
    const float dx = reading.position[0] - m_wetPosition[0];
    const float dy = reading.position[1] - m_wetPosition[1];
    return m_wetness * SmoothFall(std::sqrt(dx * dx + dy * dy), kWetFullWithin, kWetNoneBeyond);
}

float WeatherDirector::NextUnit()
{
    m_random = m_random * 1664525u + 1013904223u;
    return static_cast<float>(m_random >> 8) / static_cast<float>(1u << 24);
}

bool WeatherDirector::LightningWeather(const WeatherReading& reading, double nowSeconds)
{
    if (reading.ash)
        m_lastAshSeconds = nowSeconds;
    if (reading.overcastWeight > 0.0f)
        return nowSeconds - m_lastAshSeconds >= kAshOvercastFadeSeconds &&
               reading.thunderOvercast >= kLightestThunderOvercast;
    return m_blend[kWeatherRain] >= kStormRainBlend;
}

void WeatherDirector::Strike(double nowSeconds, bool asked)
{
    m_lastStrike = nowSeconds;
    m_strikeSeed = NextUnit();
    m_strikeAsked = asked;
}

float WeatherDirector::Flash(const WeatherReading& reading, const WeatherTuning& tuning, double nowSeconds)
{
    if (reading.strikeRequested && tuning.lightning > 0.0f)
        Strike(nowSeconds, true);
    const bool storm = LightningWeather(reading, nowSeconds) && tuning.lightning > 0.0f;
    if (!storm)
    {
        m_nextStrike = 0.0;
        return WeatherLightningEnvelope(nowSeconds - m_lastStrike) * std::max(tuning.lightning, 0.0f);
    }
    if (m_nextStrike == 0.0)
        m_nextStrike = std::max(nowSeconds + kShortestStrikeGapSeconds * NextUnit(),
                                m_lastStrike + kShortestStrikeGapSeconds);
    if (nowSeconds >= m_nextStrike)
    {
        Strike(nowSeconds, false);
        m_nextStrike = nowSeconds + kShortestStrikeGapSeconds +
                       (kLongestStrikeGapSeconds - kShortestStrikeGapSeconds) * NextUnit();
    }
    return WeatherLightningEnvelope(nowSeconds - m_lastStrike) * tuning.lightning;
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

    const float wetHere = LocalWetness(reading);
    const float clearing = wetHere * (1.0f - cover);
    const float mist = clearing * std::max(tuning.afterRainMist, 0.0f) +
                       (1.0f - cover) * WeatherDawnWeight(reading.dayFraction) * std::max(tuning.dawnMist, 0.0f);
    look.wetness = wetHere;
    look.density *= 1.0f + mist;
    look.variation *= 1.0f + mist;
    look.rainDropsPerSecond = m_blend[kWeatherRain] * kFullRainDropsPerSecond * std::max(tuning.rainRings, 0.0f);
    look.flash = Flash(reading, tuning, nowSeconds);
    look.strikeSeed = m_strikeSeed;
    look.strikeAsked = m_strikeAsked;
    look.lowSunRays = std::max(tuning.lowSunRays, 0.0f) * (1.0f - cover);
    look.clearingRays = std::max(tuning.clearingRays, 0.0f) * clearing * clearing;
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
    out.rainDropsPerSecond = look.rainDropsPerSecond * share;
    out.flash = look.flash * share;
    out.waves = Mix(1.0f, look.waves, share);
    out.wind = Mix(1.0f, look.wind, share);
    out.foam = Mix(1.0f, look.foam, share);
    out.glint = Mix(1.0f, look.glint, share);
    return out;
}

float WeatherLightningEnvelope(double sinceStrike)
{
    if (sinceStrike < 0.0 || sinceStrike > kEnvelopeEndSeconds)
        return 0.0f;
    float envelope = static_cast<float>(std::min(sinceStrike / kFlashRiseSeconds, 1.0) *
                                        std::exp(-sinceStrike / kFirstStrokeSeconds));
    if (sinceStrike > kSecondStrokeDelay)
    {
        const double sinceSecond = sinceStrike - kSecondStrokeDelay;
        envelope += kSecondStrokeShare * static_cast<float>(std::min(sinceSecond / kFlashRiseSeconds, 1.0) *
                                                            std::exp(-sinceSecond / kSecondStrokeSeconds));
    }
    return std::min(envelope, 1.0f);
}

float WeatherDawnWeight(float dayFraction)
{
    if (!std::isfinite(dayFraction) || dayFraction <= kDawnStart || dayFraction >= kDawnEnd)
        return 0.0f;
    const float t = dayFraction < kDawnPeak ? (dayFraction - kDawnStart) / (kDawnPeak - kDawnStart)
                                            : (kDawnEnd - dayFraction) / (kDawnEnd - kDawnPeak);
    return t * t * (3.0f - 2.0f * t);
}

float WeatherGodRayStrength(float configured, const WeatherLook& look, float sunHeight)
{
    const float lowSun = SmoothFall(sunHeight, kLowSunFullBelow, kLowSunNoneAbove);
    return std::max(configured * look.raysGain + look.lowSunRays * lowSun + look.clearingRays, 0.0f);
}
