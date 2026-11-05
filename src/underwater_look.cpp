#include "underwater_look.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kBrightestChannelExtinction = 0.7f;
constexpr float kDarkestChannelExtinction = 1.6f;
constexpr float kSunAboveHorizonForLight = 0.2f;
constexpr float kSmallestSine = 1e-6f;

float Saturate(float x)
{
    return std::clamp(x, 0.0f, 1.0f);
}

float SmoothStep(float edge0, float edge1, float x)
{
    const float t = Saturate((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

bool ThickLiquid(UnderwaterMedium medium)
{
    return medium == UnderwaterMedium::Magma || medium == UnderwaterMedium::Slime;
}

float Visibility(const UnderwaterLookInputs& in, const UnderwaterSettings& settings)
{
    const float clientEnd = std::isfinite(in.clientFogEnd) ? in.clientFogEnd : 0.0f;
    if (ThickLiquid(in.medium))
        return std::clamp(clientEnd, kMinThickLiquidVisibility, kMaxThickLiquidVisibility);
    return std::clamp(clientEnd * settings.clarity, kMinUnderwaterVisibility, kMaxUnderwaterVisibility);
}

void ChannelExtinction(const float* fogColour, float visibility, float* extinction)
{
    const float base = -std::log(kUnderwaterRemainingLightAtVisibility) / visibility;
    const float brightest = std::max({fogColour[0], fogColour[1], fogColour[2]});
    for (int c = 0; c < 3; ++c)
    {
        const float share = brightest > 0.0f ? Saturate(fogColour[c] / brightest) : 1.0f;
        const float weight =
            kDarkestChannelExtinction + (kBrightestChannelExtinction - kDarkestChannelExtinction) * share;
        extinction[c] = base * weight;
    }
}

float LightAboveSurface(const UnderwaterLookInputs& in)
{
    const float aboveHorizon = SmoothStep(0.0f, kSunAboveHorizonForLight, in.toLight[2]);
    const float moon = in.lightIsMoon ? kMoonUnderwaterLight : 1.0f;
    return aboveHorizon * moon * Saturate(in.outdoors);
}
}

UnderwaterMedium UnderwaterMediumOf(uint32_t liquidType, uint32_t soundBank)
{
    if (!liquidType)
        return UnderwaterMedium::None;
    switch (soundBank)
    {
    case kLiquidSoundBankMagma:
        return UnderwaterMedium::Magma;
    case kLiquidSoundBankSlime:
        return UnderwaterMedium::Slime;
    default:
        return UnderwaterMedium::Water;
    }
}

const char* UnderwaterMediumLabel(UnderwaterMedium medium)
{
    switch (medium)
    {
    case UnderwaterMedium::Water:
        return "water";
    case UnderwaterMedium::Magma:
        return "magma";
    case UnderwaterMedium::Slime:
        return "slime";
    default:
        return "none";
    }
}

bool RefractIntoWater(const float toLight[3], float out[3])
{
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = 1.0f;
    const float length = std::sqrt(toLight[0] * toLight[0] + toLight[1] * toLight[1] + toLight[2] * toLight[2]);
    if (!(length > 0.0f) || toLight[2] <= 0.0f)
        return false;
    const float up = toLight[2] / length;
    const float sineInAir = std::sqrt(std::max(0.0f, 1.0f - up * up));
    if (sineInAir < kSmallestSine)
        return true;
    const float sineInWater = sineInAir / kWaterRefractiveIndex;
    const float horizontal = sineInWater / (sineInAir * length);
    out[0] = toLight[0] * horizontal;
    out[1] = toLight[1] * horizontal;
    out[2] = std::sqrt(1.0f - sineInWater * sineInWater);
    return true;
}

UnderwaterLook BuildUnderwaterLook(const UnderwaterLookInputs& in, const UnderwaterSettings& settings)
{
    UnderwaterLook look;
    look.medium = in.medium;
    if (in.medium == UnderwaterMedium::None)
        return look;
    look.visibility = Visibility(in, settings);
    ChannelExtinction(in.clientFogColour, look.visibility, look.extinction);
    for (int c = 0; c < 3; ++c)
        look.scatterColour[c] = Saturate(in.clientFogColour[c]);
    look.cameraDepth = std::max(0.0f, in.cameraDepth);
    look.surfaceHeight = in.cameraHeight + look.cameraDepth;
    look.wobble = std::max(0.0f, settings.wobble);
    if (ThickLiquid(in.medium))
    {
        look.selfLit = in.medium == UnderwaterMedium::Magma ? 1.0f : 0.0f;
        return look;
    }
    const bool lit = RefractIntoWater(in.toLight, look.lightInWater);
    const float light = lit ? LightAboveSurface(in) : 0.0f;
    for (int c = 0; c < 3; ++c)
        look.sunLight[c] = std::max(0.0f, in.directLight[c]) * light;
    look.caustics = std::max(0.0f, settings.caustics);
    look.shafts = std::max(0.0f, settings.shafts);
    look.surface = Saturate(settings.surface);
    look.depthDarkening = std::max(0.0f, settings.depthDarkening);
    return look;
}
