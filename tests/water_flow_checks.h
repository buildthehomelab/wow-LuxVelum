#pragma once

#include "water_flow.h"

#include <cmath>

namespace water_flow_checks
{
constexpr float kAllowedError = 1e-4f;

float LayerWeight(float phase)
{
    return 1.0f - std::fabs(2.0f * phase - 1.0f);
}

void CheckPhases()
{
    bool inRange = true;
    bool halfApart = true;
    bool weightsSumToOne = true;
    for (int step = 0; step < 4000; ++step)
    {
        const WaterFlowPhases phases = ComputeWaterFlowPhases(1000.0 + step * 0.0173);
        inRange = inRange && phases.first >= 0.0f && phases.first < 1.0f && phases.second >= 0.0f &&
                  phases.second < 1.0f;
        const float apart = std::fabs(phases.first - phases.second);
        halfApart = halfApart && std::fabs(apart - 0.5f) < kAllowedError;
        weightsSumToOne =
            weightsSumToOne && std::fabs(LayerWeight(phases.first) + LayerWeight(phases.second) - 1.0f) < kAllowedError;
    }
    Check(inRange, "the flow's two layers stay within one cycle");
    Check(halfApart, "the two layers are half a cycle apart");
    Check(weightsSumToOne, "each layer is fully faded out when it jumps back, so no jump shows");
}

void CheckLongSessions()
{
    constexpr double kThirtyDays = 30.0 * 24.0 * 3600.0;
    const WaterFlowPhases before = ComputeWaterFlowPhases(kThirtyDays);
    const WaterFlowPhases after = ComputeWaterFlowPhases(kThirtyDays + 0.016);
    const float expected = static_cast<float>(0.016 / kWaterFlowCycleSeconds);
    Check(std::fabs(after.first - before.first - expected) < 1e-3f || after.first < before.first,
          "the phase still advances smoothly after a month of uptime");
}

void Run()
{
    CheckPhases();
    CheckLongSessions();
}
}
