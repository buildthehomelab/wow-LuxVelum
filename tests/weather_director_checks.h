#pragma once

#include "weather_director.h"

#include <cmath>
#include <limits>

namespace weather_director_checks
{
constexpr int32_t kUnknownWeather = 4;
constexpr float kAllowedError = 1e-4f;
constexpr double kStart = 100.0;

bool Near(float a, float b)
{
    return std::fabs(a - b) < kAllowedError;
}

WeatherTuning TestTuning()
{
    WeatherTuning tuning;
    tuning.profiles[kWeatherRain].sun = 0.4f;
    tuning.profiles[kWeatherRain].rays = 0.0f;
    tuning.profiles[kWeatherRain].density = 1.5f;
    tuning.profiles[kWeatherSnow].exposure = 1.2f;
    tuning.profiles[kWeatherSand].tint = 0.6f;
    tuning.easeSeconds = 30.0f;
    tuning.wetSeconds = 100.0f;
    tuning.dryingSeconds = 200.0f;
    tuning.afterRainMist = 0.5f;
    tuning.lowSunRays = 0.6f;
    tuning.clearingRays = 1.0f;
    return tuning;
}

void CheckKinds()
{
    Check(WeatherKindOf(kClientWeatherRain, 0.3f) == kWeatherRain &&
              WeatherKindOf(kClientWeatherSnow, 0.9f) == kWeatherSnow &&
              WeatherKindOf(kClientWeatherSand, 0.6f) == kWeatherSand,
          "the client's weather types name rain, snow and sand");
    Check(WeatherKindOf(kClientWeatherNone, 0.6f) == kWeatherFog,
          "fog has no particle type in the client's table, so type 0 with a real grade is fog");
    Check(WeatherKindOf(kClientWeatherNone, 0.2f) == kNoWeatherKind &&
              WeatherKindOf(kClientWeatherNone, 0.0f) == kNoWeatherKind,
          "clear weather stays clear, even when the server sends it a low grade");
    Check(WeatherKindOf(kUnknownWeather, 0.5f) == kNoWeatherKind &&
              WeatherKindOf(kClientWeatherRain, std::numeric_limits<float>::quiet_NaN()) == kNoWeatherKind &&
              WeatherKindOf(kClientWeatherRain, 0.0f) == kNoWeatherKind,
          "an unknown type or a bad intensity counts as clear");
}

void CheckClearIsNeutral()
{
    WeatherDirector director;
    WeatherTuning tuning = TestTuning();
    tuning.lowSunRays = 0.0f;
    tuning.clearingRays = 0.0f;
    const WeatherLook look = director.Step({}, tuning, kStart);
    Check(look.stormBlend == 0.0f && look.sun == 1.0f && look.exposure == 1.0f && look.density == 1.0f &&
              look.tint == 0.0f && look.waves == 1.0f && WeatherGodRayStrength(0.0f, look, 0.1f) == 0.0f,
          "clear weather leaves every setting alone");
    Check(WeatherGodRayStrength(0.8f, look, 0.9f) == 0.8f, "clear weather keeps the player's own god rays");
}

void CheckEase()
{
    WeatherDirector director;
    const WeatherTuning tuning = TestTuning();
    WeatherReading rain;
    rain.type = kClientWeatherRain;
    rain.intensity = 0.8f;
    WeatherLook look = director.Step(rain, tuning, kStart);
    Check(Near(look.stormBlend, 0.8f) && Near(look.sun, 1.0f + 0.8f * (0.4f - 1.0f)),
          "the first reading applies at once, so a login during rain isn't clear");
    look = director.Step({}, tuning, kStart + tuning.easeSeconds / 2.0);
    Check(Near(look.stormBlend, 0.3f), "weather eases away, half the time taking half the range");
    look = director.Step({}, tuning, kStart + tuning.easeSeconds * 2.0);
    Check(Near(look.stormBlend, 0.0f), "the ease stops at its target");
    look = director.Step(rain, tuning, kStart);
    Check(Near(look.stormBlend, 0.0f), "a clock that runs backwards doesn't move the blend");
}

void CheckKindsCrossFade()
{
    WeatherDirector director;
    const WeatherTuning tuning = TestTuning();
    WeatherReading rain;
    rain.type = kClientWeatherRain;
    rain.intensity = 1.0f;
    director.Step(rain, tuning, kStart);
    WeatherReading snow;
    snow.type = kClientWeatherSnow;
    snow.intensity = 1.0f;
    director.Step(snow, tuning, kStart + tuning.easeSeconds / 2.0);
    Check(Near(director.KindBlend(kWeatherRain), 0.5f) && Near(director.KindBlend(kWeatherSnow), 0.5f),
          "rain turning to snow fades one out while the other comes in");
}

void CheckStrength()
{
    WeatherDirector director;
    WeatherTuning tuning = TestTuning();
    tuning.strength = 0.5f;
    WeatherReading rain;
    rain.type = kClientWeatherRain;
    rain.intensity = 1.0f;
    const WeatherLook look = director.Step(rain, tuning, kStart);
    Check(Near(look.sun, 0.7f) && Near(look.stormBlend, 1.0f),
          "the strength scales the look but not the area's own storm fog");
    tuning.strength = 2.0f;
    tuning.profiles[kWeatherRain].sun = 0.2f;
    WeatherDirector strong;
    Check(strong.Step(rain, tuning, kStart).sun == 0.0f, "a strong setting can't push a value below zero");
}

void CheckAfterRain()
{
    WeatherDirector director;
    const WeatherTuning tuning = TestTuning();
    WeatherReading rain;
    rain.type = kClientWeatherRain;
    rain.intensity = 1.0f;
    double now = kStart;
    director.Step(rain, tuning, now);
    now += tuning.wetSeconds;
    WeatherLook look = director.Step(rain, tuning, now);
    Check(Near(look.wetness, 1.0f) && Near(look.clearingRays, 0.0f),
          "the ground gets wet while it rains, with no clearing rays under the rain");
    now += tuning.easeSeconds;
    look = director.Step({}, tuning, now);
    const float wetness = 1.0f - tuning.easeSeconds / tuning.dryingSeconds;
    Check(Near(look.stormBlend, 0.0f) && Near(look.wetness, wetness) && Near(look.density, 1.0f + wetness * 0.5f) &&
              Near(look.clearingRays, wetness),
          "when the rain ends, mist and rays through the clearing follow how wet it still is");
    now += tuning.dryingSeconds;
    look = director.Step({}, tuning, now);
    Check(Near(look.wetness, 0.0f) && Near(look.density, 1.0f) && Near(look.clearingRays, 0.0f),
          "once dry, the after-rain look has gone");
}

void CheckTintColour()
{
    WeatherDirector director;
    WeatherTuning tuning = TestTuning();
    tuning.tintColors[kWeatherSand][0] = 0.8f;
    tuning.tintColors[kWeatherSand][2] = 0.4f;
    WeatherReading sand;
    sand.type = kClientWeatherSand;
    sand.intensity = 1.0f;
    const WeatherLook look = director.Step(sand, tuning, kStart);
    Check(Near(look.tint, 0.6f) && Near(look.tintColor[0], 0.8f) && Near(look.tintColor[2], 0.4f),
          "a sandstorm tints the fog its own colour");
    const WeatherLook fading = director.Step({}, tuning, kStart + 1.0);
    Check(Near(fading.tintColor[0], 0.8f), "the colour holds while the sand fades, rather than turning white");
}

void CheckGodRays()
{
    WeatherLook look;
    look.lowSunRays = 0.6f;
    Check(Near(WeatherGodRayStrength(0.0f, look, 0.1f), 0.6f), "a low sun in clear weather brings rays");
    Check(Near(WeatherGodRayStrength(0.0f, look, 0.9f), 0.0f), "a high sun doesn't");
    look.raysGain = 0.0f;
    look.lowSunRays = 0.0f;
    Check(WeatherGodRayStrength(1.0f, look, 0.1f) == 0.0f, "rain can take away even the player's own god rays");
    look.clearingRays = 0.5f;
    Check(Near(WeatherGodRayStrength(0.0f, look, 0.9f), 0.5f), "clearing rays come at any sun height");
}

void CheckIndoors()
{
    WeatherLook look;
    look.sun = 0.4f;
    look.exposure = 0.8f;
    look.tint = 0.5f;
    look.lowSunRays = 1.0f;
    look.stormBlend = 0.7f;
    const WeatherLook indoors = NeutralWeatherLook(look, 0.0f);
    Check(indoors.sun == 1.0f && indoors.exposure == 1.0f && indoors.tint == 0.0f && indoors.lowSunRays == 0.0f &&
              indoors.stormBlend == 0.7f,
          "indoors the look goes neutral; the interior fade already handles the storm layers");
    const WeatherLook outdoors = NeutralWeatherLook(look, 1.0f);
    Check(Near(outdoors.sun, look.sun) && Near(outdoors.exposure, look.exposure) && Near(outdoors.tint, look.tint),
          "outdoors it's untouched");
}

void Run()
{
    CheckKinds();
    CheckClearIsNeutral();
    CheckEase();
    CheckKindsCrossFade();
    CheckStrength();
    CheckAfterRain();
    CheckTintColour();
    CheckGodRays();
    CheckIndoors();
}
}
