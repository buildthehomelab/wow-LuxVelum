#include "weather_storm.h"

#include <algorithm>
#include <cmath>

float WeatherStormTarget(int32_t weatherType, float intensity)
{
    if (weatherType <= kClientWeatherNone || weatherType > kClientWeatherMist || !std::isfinite(intensity))
        return 0.0f;
    return std::clamp(intensity, 0.0f, 1.0f);
}

float WeatherStormEase::Step(float target, double nowSeconds)
{
    if (!m_started)
    {
        m_started = true;
        m_blend = target;
        m_lastSeconds = nowSeconds;
        return m_blend;
    }
    const double elapsed = std::max(nowSeconds - m_lastSeconds, 0.0);
    m_lastSeconds = nowSeconds;
    const float largestStep = static_cast<float>(elapsed / kWeatherStormEaseSeconds);
    m_blend += std::clamp(target - m_blend, -largestStep, largestStep);
    return m_blend;
}
