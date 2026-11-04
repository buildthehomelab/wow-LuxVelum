#pragma once

#include <cstdint>

constexpr int32_t kClientWeatherNone = 0;
constexpr int32_t kClientWeatherMist = 3;
constexpr double kWeatherStormEaseSeconds = 30.0;

float WeatherStormTarget(int32_t weatherType, float intensity);

class WeatherStormEase
{
public:
    float Step(float target, double nowSeconds);

private:
    bool m_started = false;
    float m_blend = 0.0f;
    double m_lastSeconds = 0.0;
};
