#pragma once

#include "weather_storm.h"

#include <cmath>
#include <limits>

namespace weather_storm_checks
{
constexpr int32_t kRain = 1;
constexpr int32_t kSnow = 2;
constexpr int32_t kUnknownWeather = 4;
constexpr float kLightRain = 0.3f;
constexpr float kAllowedError = 1e-4f;

bool Near(float a, float b)
{
    return std::fabs(a - b) < kAllowedError;
}

void CheckTarget()
{
    Check(Near(WeatherStormTarget(kRain, kLightRain), kLightRain) && Near(WeatherStormTarget(kSnow, 0.9f), 0.9f) &&
              Near(WeatherStormTarget(kClientWeatherMist, 0.6f), 0.6f),
          "rain, snow and mist weigh the storm fog by their intensity");
    Check(WeatherStormTarget(kClientWeatherNone, 0.2f) == 0.0f,
          "clear weather keeps the clear fog, even when the server sends it a low grade");
    Check(WeatherStormTarget(kUnknownWeather, 0.5f) == 0.0f &&
              WeatherStormTarget(kRain, std::numeric_limits<float>::quiet_NaN()) == 0.0f,
          "an unknown weather type or a bad intensity keeps the clear fog");
    Check(WeatherStormTarget(kRain, 2.0f) == 1.0f && WeatherStormTarget(kRain, -1.0f) == 0.0f,
          "the storm weight stays within 0..1");
}

void CheckEase()
{
    WeatherStormEase ease;
    Check(Near(ease.Step(0.8f, 100.0), 0.8f), "the first reading applies at once, so a login during rain isn't clear");
    Check(Near(ease.Step(0.0f, 100.0 + kWeatherStormEaseSeconds / 2.0), 0.3f),
          "the storm fog eases away across half the time by half the range");
    Check(Near(ease.Step(0.0f, 100.0 + kWeatherStormEaseSeconds * 2.0), 0.0f), "the ease stops at its target");
    Check(Near(ease.Step(1.0f, 90.0), 0.0f), "a clock that runs backwards doesn't move the blend");
}

void Run()
{
    CheckTarget();
    CheckEase();
}
}
