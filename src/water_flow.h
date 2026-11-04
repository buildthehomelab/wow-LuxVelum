#pragma once

constexpr double kWaterFlowCycleSeconds = 1.6;

struct WaterFlowPhases
{
    float first;
    float second;
};

WaterFlowPhases ComputeWaterFlowPhases(double seconds);
