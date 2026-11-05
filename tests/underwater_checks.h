#pragma once

#include "underwater_look.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace underwater_checks
{
constexpr float kAllowedError = 1e-4f;
constexpr float kDegrees = 3.14159265f / 180.0f;

UnderwaterLookInputs LakeAtNoon()
{
    UnderwaterLookInputs in;
    in.medium = UnderwaterMedium::Water;
    in.cameraDepth = 4.0f;
    in.cameraHeight = 20.0f;
    in.toLight[0] = 0.0f;
    in.toLight[1] = 0.0f;
    in.toLight[2] = 1.0f;
    in.clientFogColour[0] = 0.1f;
    in.clientFogColour[1] = 0.35f;
    in.clientFogColour[2] = 0.4f;
    in.clientFogEnd = 60.0f;
    in.directLight[0] = 1.0f;
    in.directLight[1] = 0.9f;
    in.directLight[2] = 0.8f;
    return in;
}

float Remaining(const UnderwaterLook& look, int channel, float yards)
{
    return std::exp(-look.extinction[channel] * yards);
}

float AngleFromUp(const float* direction)
{
    return std::acos(std::clamp(direction[2], -1.0f, 1.0f));
}

void CheckMediums()
{
    Check(UnderwaterMediumOf(0, kLiquidSoundBankMagma) == UnderwaterMedium::None,
          "no liquid type means the camera is not in a liquid");
    Check(UnderwaterMediumOf(1, kLiquidSoundBankWater) == UnderwaterMedium::Water &&
              UnderwaterMediumOf(2, kLiquidSoundBankOcean) == UnderwaterMedium::Water,
          "lakes and the sea are water under the surface");
    Check(UnderwaterMediumOf(3, kLiquidSoundBankMagma) == UnderwaterMedium::Magma,
          "a magma sound bank is magma");
    Check(UnderwaterMediumOf(21, kLiquidSoundBankSlime) == UnderwaterMedium::Slime,
          "a slime sound bank is slime");
}

void CheckVisibility()
{
    const UnderwaterSettings settings;
    const UnderwaterLook look = BuildUnderwaterLook(LakeAtNoon(), settings);
    Check(std::fabs(look.visibility - 60.0f) < kAllowedError, "you see as far as the client's own under-water fog");
    Check(look.extinction[2] < look.extinction[0] && look.extinction[1] < look.extinction[0],
          "the zone's water colour carries farthest and red fades first");
    const float mean = (Remaining(look, 0, 60.0f) + Remaining(look, 1, 60.0f) + Remaining(look, 2, 60.0f)) / 3.0f;
    Check(mean < 0.2f && Remaining(look, 0, 60.0f) < 0.05f, "little is left of the scene at the client's distance");

    UnderwaterSettings clearer;
    clearer.clarity = 2.0f;
    Check(std::fabs(BuildUnderwaterLook(LakeAtNoon(), clearer).visibility - 120.0f) < kAllowedError,
          "Clarity 2 doubles the distance you see");

    UnderwaterLookInputs fogless = LakeAtNoon();
    fogless.clientFogEnd = 0.0f;
    Check(BuildUnderwaterLook(fogless, settings).visibility >= kMinUnderwaterVisibility,
          "a client fog of 0 yards still leaves a few yards of view");
    fogless.clientFogEnd = std::numeric_limits<float>::quiet_NaN();
    Check(std::isfinite(BuildUnderwaterLook(fogless, settings).extinction[0]),
          "a broken client fog distance gives a finite fog");
}

void CheckSurface()
{
    const UnderwaterLook look = BuildUnderwaterLook(LakeAtNoon(), UnderwaterSettings());
    Check(std::fabs(look.surfaceHeight - 24.0f) < kAllowedError,
          "the surface lies the client's depth above the camera");
}

void CheckSunInWater()
{
    float overhead[3];
    const float zenith[3] = {0.0f, 0.0f, 1.0f};
    Check(RefractIntoWater(zenith, overhead) && std::fabs(overhead[2] - 1.0f) < kAllowedError,
          "a sun straight overhead shines straight down");

    const float low[3] = {std::cos(10.0f * kDegrees), 0.0f, std::sin(10.0f * kDegrees)};
    float bent[3];
    const bool lit = RefractIntoWater(low, bent);
    const float length = std::sqrt(bent[0] * bent[0] + bent[1] * bent[1] + bent[2] * bent[2]);
    const float critical = std::asin(1.0f / kWaterRefractiveIndex);
    Check(lit && std::fabs(length - 1.0f) < kAllowedError && AngleFromUp(bent) < critical + kAllowedError &&
              bent[0] > 0.0f,
          "a low sun bends steeply down into the water, toward the same side");

    const float below[3] = {0.0f, 1.0f, -0.2f};
    float none[3];
    Check(!RefractIntoWater(below, none), "a sun below the horizon sends no light under water");
}

bool Dark(const UnderwaterLook& look)
{
    return look.sunLight[0] == 0.0f && look.sunLight[1] == 0.0f && look.sunLight[2] == 0.0f;
}

void CheckSunlight()
{
    const UnderwaterSettings settings;
    Check(!Dark(BuildUnderwaterLook(LakeAtNoon(), settings)), "a noon sun lights the water");

    UnderwaterLookInputs night = LakeAtNoon();
    night.toLight[2] = -0.3f;
    night.toLight[0] = 0.95f;
    Check(Dark(BuildUnderwaterLook(night, settings)), "no sunlight under water once the sun has set");

    UnderwaterLookInputs indoors = LakeAtNoon();
    indoors.outdoors = 0.0f;
    Check(Dark(BuildUnderwaterLook(indoors, settings)), "no sunlight in an indoor pool");

    UnderwaterLookInputs moonlit = LakeAtNoon();
    moonlit.lightIsMoon = true;
    Check(BuildUnderwaterLook(moonlit, settings).sunLight[0] < BuildUnderwaterLook(LakeAtNoon(), settings).sunLight[0],
          "moonlight under water is fainter than sunlight");
}

void CheckThickLiquids()
{
    UnderwaterLookInputs magma = LakeAtNoon();
    magma.medium = UnderwaterMedium::Magma;
    magma.clientFogEnd = 80.0f;
    const UnderwaterLook look = BuildUnderwaterLook(magma, UnderwaterSettings());
    Check(look.visibility <= kMaxThickLiquidVisibility, "you see only a few yards in magma");
    Check(look.caustics == 0.0f && look.shafts == 0.0f && look.surface == 0.0f && Dark(look),
          "magma has no rippling sunlight, shafts or see-through surface");
    Check(look.selfLit == 1.0f, "magma glows by itself");

    UnderwaterLookInputs slime = magma;
    slime.medium = UnderwaterMedium::Slime;
    Check(BuildUnderwaterLook(slime, UnderwaterSettings()).selfLit == 0.0f, "slime does not glow");
}

void Run()
{
    CheckMediums();
    CheckVisibility();
    CheckSurface();
    CheckSunInWater();
    CheckSunlight();
    CheckThickLiquids();
}
}
