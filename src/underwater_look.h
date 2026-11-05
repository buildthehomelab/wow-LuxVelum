#pragma once

#include "water_classify.h"

#include <cstdint>

constexpr uint32_t kLiquidSoundBankWater = 0;
constexpr uint32_t kLiquidSoundBankMagma = 2;
constexpr uint32_t kLiquidSoundBankSlime = 3;

constexpr float kWaterRefractiveIndex = 1.333f;
constexpr float kMinUnderwaterVisibility = 6.0f;
constexpr float kMaxUnderwaterVisibility = 400.0f;
constexpr float kMinThickLiquidVisibility = 1.5f;
constexpr float kMaxThickLiquidVisibility = 12.0f;
constexpr float kUnderwaterRemainingLightAtVisibility = 0.05f;
constexpr float kMoonUnderwaterLight = 0.2f;

enum class UnderwaterMedium
{
    None,
    Water,
    Magma,
    Slime,
};

struct UnderwaterSettings
{
    float clarity = 1.0f;
    float caustics = 0.7f;
    float shafts = 0.6f;
    float surface = 1.0f;
    float wobble = 0.4f;
    float depthDarkening = 0.35f;
};

struct UnderwaterLookInputs
{
    UnderwaterMedium medium = UnderwaterMedium::None;
    float cameraDepth = 0.0f;
    float cameraHeight = 0.0f;
    float toLight[3] = {0.0f, 0.0f, 1.0f};
    bool lightIsMoon = false;
    float clientFogColour[3] = {};
    float clientFogEnd = 0.0f;
    float directLight[3] = {};
    float outdoors = 1.0f;
};

struct UnderwaterLook
{
    UnderwaterMedium medium = UnderwaterMedium::None;
    float visibility = 0.0f;
    float extinction[3] = {};
    float scatterColour[3] = {};
    float surfaceHeight = 0.0f;
    float cameraDepth = 0.0f;
    float lightInWater[3] = {0.0f, 0.0f, 1.0f};
    float sunLight[3] = {};
    float caustics = 0.0f;
    float shafts = 0.0f;
    float surface = 0.0f;
    float wobble = 0.0f;
    float depthDarkening = 0.0f;
    float selfLit = 0.0f;
};

UnderwaterMedium UnderwaterMediumOf(uint32_t liquidType, uint32_t soundBank);
const char* UnderwaterMediumLabel(UnderwaterMedium medium);
bool RefractIntoWater(const float toLight[3], float out[3]);
UnderwaterLook BuildUnderwaterLook(const UnderwaterLookInputs& in, const UnderwaterSettings& settings);
