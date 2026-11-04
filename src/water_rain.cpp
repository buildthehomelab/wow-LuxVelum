#include "water_rain.h"

#include <algorithm>
#include <cmath>

float WaterRainDrops::NextUnit()
{
    m_state = m_state * 1664525u + 1013904223u;
    return static_cast<float>(m_state >> 8) / static_cast<float>(1u << 24);
}

uint32_t WaterRainDrops::Take(float dropsThisStep, const float* centre, float halfExtent, WaterRippleDisturbance* out,
                              uint32_t capacity)
{
    if (!(dropsThisStep > 0.0f) || !(halfExtent > 0.0f))
    {
        m_owed = 0.0f;
        return 0;
    }
    m_owed = std::min(m_owed + dropsThisStep, static_cast<float>(kMaxWaterRainDropsPerStep));
    const uint32_t count = std::min({static_cast<uint32_t>(m_owed), kMaxWaterRainDropsPerStep, capacity});
    m_owed -= static_cast<float>(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        WaterRippleDisturbance& drop = out[i];
        const float x = centre[0] + (NextUnit() * 2.0f - 1.0f) * halfExtent;
        const float y = centre[1] + (NextUnit() * 2.0f - 1.0f) * halfExtent;
        drop.from[0] = drop.to[0] = x;
        drop.from[1] = drop.to[1] = y;
        drop.radius = kWaterRainDropRadius;
        drop.amplitude = kWaterRainDropImpulse * (0.6f + 0.4f * NextUnit());
        drop.held = false;
    }
    return count;
}
