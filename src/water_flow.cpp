#include "water_flow.h"

#include <cmath>

WaterFlowPhases ComputeWaterFlowPhases(double seconds)
{
    const double cycles = seconds / kWaterFlowCycleSeconds;
    const double first = cycles - std::floor(cycles);
    const double second = first + 0.5;
    return {static_cast<float>(first), static_cast<float>(second - std::floor(second))};
}
