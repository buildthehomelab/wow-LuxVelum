#pragma once

#include "water_contacts.h"

#include <cstdint>

constexpr uint32_t kMaxWaterRainDropsPerStep = 6;
constexpr float kWaterRainDropRadius = 0.25f;
constexpr float kWaterRainDropImpulse = -0.35f;

class WaterRainDrops
{
public:
    uint32_t Take(float dropsThisStep, const float* centre, float halfExtent, WaterRippleDisturbance* out,
                  uint32_t capacity);

private:
    float NextUnit();

    float m_owed = 0.0f;
    uint32_t m_state = 0x2F6E2B1u;
};
