#pragma once

#include "sky_clouds.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace sky_clouds_checks
{
constexpr float kAllowedError = 1e-4f;
constexpr uint32_t kRainLight = 8951;
constexpr uint32_t kStormLight = 8952;
constexpr uint32_t kSandLight = 8954;
constexpr uint32_t kWeightRow045 = 8909;
constexpr uint32_t kWeightRow080 = 8916;
constexpr uint32_t kNightLevelCopy = 2 * kLightCopyStep;
constexpr uint32_t kCheckedFieldSize = 64;
constexpr uint32_t kNightLevelFour = 9004;
constexpr uint32_t kLevelFourBaseLight = 30001;

bool Near(float a, float b)
{
    return std::fabs(a - b) < kAllowedError;
}

OvercastReading Finished(OvercastReading reading)
{
    FinishOvercast(reading);
    return reading;
}

void CheckOvercastReading()
{
    OvercastReading rain;
    AddLightSwapToOvercast(rain, kWeightRow045, kRainLight, 1.0f);
    rain = Finished(rain);
    Check(Near(rain.weight, 0.45f) && Near(rain.share[kOvercastRain], 1.0f),
          "a rain swap on the 0.45 row reads as a 0.45 rain overcast");

    OvercastReading copy;
    AddLightSwapToOvercast(copy, kWeightRow080 + kNightLevelCopy, kStormLight + kNightLevelCopy, 1.0f);
    copy = Finished(copy);
    Check(Near(copy.weight, 0.8f) && Near(copy.share[kOvercastStorm], 1.0f),
          "a night level's copies of the rows read the same as the rows");

    OvercastReading fading;
    AddLightSwapToOvercast(fading, kWeightRow080, kRainLight, 0.25f);
    AddLightSwapToOvercast(fading, kWeightRow080, kStormLight, 0.75f);
    fading = Finished(fading);
    Check(Near(fading.weight, 0.8f) && Near(fading.share[kOvercastRain], 0.25f) &&
              Near(fading.share[kOvercastStorm], 0.75f),
          "heavy rain fading into thunder keeps the weight and moves the shares");

    OvercastReading raisedLevel;
    AddLightSwapToOvercast(raisedLevel, kWeightRow045, kRainLight, 1.0f);
    AddLightSwapToOvercast(raisedLevel, kNightLevelFour, kLevelFourBaseLight, 1.0f);
    AddLightSwapToOvercast(raisedLevel, kWeightRow045 + kNightLevelCopy, kRainLight + kNightLevelCopy, 1.0f);
    raisedLevel = Finished(raisedLevel);
    Check(Near(raisedLevel.weight, 0.45f), "a raised night level's own overcast replaces the one under it");

    OvercastReading turnedToThunder;
    AddLightSwapToOvercast(turnedToThunder, kOvercastFirstWeightRow + 2, kRainLight, 1.0f);
    AddLightSwapToOvercast(turnedToThunder, kNightLevelFour, kLevelFourBaseLight, 1.0f);
    AddLightSwapToOvercast(turnedToThunder, kWeightRow080 + kNightLevelCopy, kStormLight + kNightLevelCopy, 1.0f);
    turnedToThunder = Finished(turnedToThunder);
    Check(Near(turnedToThunder.weight, 0.8f) && Near(turnedToThunder.share[kOvercastStorm], 1.0f),
          "rain left under a raised level doesn't count once thunder is on top");

    OvercastReading stale;
    AddLightSwapToOvercast(stale, kWeightRow045, kRainLight, 1.0f);
    AddLightSwapToOvercast(stale, kNightLevelFour, kLevelFourBaseLight, 1.0f);
    stale = Finished(stale);
    Check(stale.weight == 0.0f, "an overcast left under a raised level with a clear sky on top draws nothing");

    OvercastReading levelFadingIn;
    AddLightSwapToOvercast(levelFadingIn, kWeightRow080, kRainLight, 1.0f);
    AddLightSwapToOvercast(levelFadingIn, kNightLevelFour, kLevelFourBaseLight, 0.25f);
    levelFadingIn = Finished(levelFadingIn);
    Check(Near(levelFadingIn.weight, 0.6f), "a level fading in covers what's under it by its fade");

    OvercastReading other;
    AddLightSwapToOvercast(other, 9003, 12345, 1.0f);
    AddLightSwapToOvercast(other, 30000 + 75, 30000 + 75, 1.0f);
    AddLightSwapToOvercast(other, kWeightRow045, 12345, 1.0f);
    AddLightSwapToOvercast(other, 12345, kRainLight, 1.0f);
    AddLightSwapToOvercast(other, kWeightRow045, kSandLight + 1, 1.0f);
    AddLightSwapToOvercast(other, kWeightRow045, kRainLight, 0.0f);
    other = Finished(other);
    Check(other.weight == 0.0f, "night level swaps and rows that aren't the overcast's read as no overcast");
}

void CheckPreview()
{
    OvercastReading live;
    AddLightSwapToOvercast(live, kWeightRow045, kRainLight, 1.0f);
    live = Finished(live);
    CloudTuning tuning;
    Check(Near(OvercastWithPreview(live, tuning).weight, 0.45f), "no preview follows the game");
    tuning.previewWeight = 0.8f;
    tuning.previewStorm = 1.0f;
    const OvercastReading preview = OvercastWithPreview(live, tuning);
    Check(Near(preview.weight, 0.8f) && Near(preview.share[kOvercastStorm], 1.0f), "a preview replaces the overcast");
}

void CheckCover()
{
    Check(CloudCoverOfWeight(0.0f) == 0.0f, "no overcast, no clouds");
    Check(Near(CloudCoverOfWeight(0.15f), 0.35f) && Near(CloudCoverOfWeight(0.45f), 0.68f) &&
              Near(CloudCoverOfWeight(0.8f), 0.95f) && CloudCoverOfWeight(1.0f) == 1.0f,
          "the cover follows its table");
    float last = 0.0f;
    bool rises = true;
    for (int step = 1; step <= 100; ++step)
    {
        const float cover = CloudCoverOfWeight(static_cast<float>(step) / 100.0f);
        rises = rises && cover >= last;
        last = cover;
    }
    Check(rises, "the clouds only grow as the weight rises");

    OvercastReading none;
    CloudTuning fair;
    Check(CloudShapeFor(none, fair).cover == 0.0f, "a clear sky has nothing to draw");
    fair.fairCover = 0.3f;
    Check(Near(CloudShapeFor(none, fair).cover, 0.3f), "fair-weather cover draws under a clear sky");
    Check(Near(CloudShapeFor(none, fair).threshold, 0.7f), "the field is evenly spread, so 30% cover is the top 30%");
    OvercastReading heavy;
    heavy.weight = 0.8f;
    Check(CloudShapeFor(none, fair).thickRange < CloudShapeFor(heavy, CloudTuning{}).thickRange,
          "small clouds have a shallower relief scale, so they keep their shape");
}

void CheckColours()
{
    CloudLight light;
    for (int k = 0; k < 3; ++k)
    {
        light.sky[k] = 0.4f;
        light.ambient[k] = 0.4f;
        light.direct[k] = 0.5f;
    }
    OvercastReading lightRain;
    AddLightSwapToOvercast(lightRain, kOvercastFirstWeightRow + 2, kRainLight, 1.0f);
    lightRain = Finished(lightRain);
    OvercastReading heavyRain;
    AddLightSwapToOvercast(heavyRain, kWeightRow080, kRainLight, 1.0f);
    heavyRain = Finished(heavyRain);
    OvercastReading thunder;
    AddLightSwapToOvercast(thunder, kWeightRow080, kStormLight, 1.0f);
    thunder = Finished(thunder);
    const CloudTuning tuning;
    const CloudColours a = CloudColoursFor(light, lightRain, tuning);
    const CloudColours b = CloudColoursFor(light, heavyRain, tuning);
    const CloudColours c = CloudColoursFor(light, thunder, tuning);
    Check(a.dark[0] > b.dark[0] && b.dark[0] > c.dark[0], "the thick parts darken from light rain to thunder");
    Check(a.lit[0] >= b.lit[0] && b.lit[0] > c.lit[0], "the lit parts dim too");
    Check(c.dark[0] < c.lit[0], "the thick parts are darker than the lit ones");

    CloudTuning darker;
    darker.darkness = 2.0f;
    Check(CloudColoursFor(light, thunder, darker).dark[0] < c.dark[0], "the darkness setting darkens the thick parts");

    CloudLight night;
    Check(CloudColoursFor(night, thunder, tuning).lit[0] == 0.0f, "the colours come from the client's light");
}

uint32_t RedOf(uint32_t texel)
{
    return (texel >> 16) & 0xFFu;
}

void CheckField()
{
    const std::vector<uint32_t> field = BuildCloudField(kCheckedFieldSize);
    Check(field.size() == kCheckedFieldSize * kCheckedFieldSize, "the field has a texel per pixel");
    int aboveMiddle = 0;
    for (uint32_t texel : field)
        aboveMiddle += RedOf(texel) >= 128 ? 1 : 0;
    const float share = static_cast<float>(aboveMiddle) / static_cast<float>(field.size());
    Check(std::fabs(share - 0.5f) < 0.02f, "half of the field is above the middle, so a threshold is a cover");
    Check(field == BuildCloudField(kCheckedFieldSize), "the same field comes out every time");
    int seamJump = 0;
    int innerJump = 0;
    for (uint32_t y = 0; y < kCheckedFieldSize; ++y)
    {
        const int first = static_cast<int>(RedOf(field[static_cast<size_t>(y) * kCheckedFieldSize]));
        const size_t lastInRow = static_cast<size_t>(y) * kCheckedFieldSize + kCheckedFieldSize - 1;
        const int last = static_cast<int>(RedOf(field[lastInRow]));
        const int nearby = static_cast<int>(RedOf(field[static_cast<size_t>(y) * kCheckedFieldSize + 1]));
        seamJump += std::abs(first - last);
        innerJump += std::abs(first - nearby);
    }
    Check(seamJump < innerJump * 3 / 2, "the field wraps at its edge without a seam");
    const std::vector<uint32_t> smaller = ShrinkCloudField(field, kCheckedFieldSize);
    Check(smaller.size() == (kCheckedFieldSize / 2) * (kCheckedFieldSize / 2), "each mip is half the size");
    const std::vector<uint32_t> square = {0x10203040u, 0x10203040u, 0x30405060u, 0x30405060u};
    Check(ShrinkCloudField(square, 2) == std::vector<uint32_t>{0x20304050u}, "a mip averages each channel on its own");
}

void CheckDriftWraps()
{
    CloudDrift drift;
    constexpr double kOneStepSeconds = 7.0;
    constexpr double kFastWind = 24.0;
    constexpr double kLongestStepTiles = 0.25;
    bool windWrapped = false;
    bool smooth = true;
    for (int step = 0; step < 4000; ++step)
    {
        const CloudDrift before = drift;
        AdvanceCloudDrift(drift, kOneStepSeconds, kFastWind);
        windWrapped = windWrapped || drift.windU < before.windU;
        const double moved = std::fmod(std::fabs(drift.windU - before.windU), kCloudWindWrap);
        smooth = smooth && std::min(moved, kCloudWindWrap - moved) < kLongestStepTiles;
    }
    Check(windWrapped, "the wind wraps");
    Check(smooth, "the wind moves in small steps across its wrap");
    constexpr double kShaderRepeats[] = {0.25, 0.5, 1.0, 1.2, 4.8};
    bool wholeRepeats = true;
    for (double repeats : kShaderRepeats)
    {
        const double tiles = kCloudWindWrap * repeats;
        wholeRepeats = wholeRepeats && std::fabs(tiles - std::round(tiles)) < 1e-9;
    }
    Check(wholeRepeats, "the wrap is a whole number of tiles at every repeat vf_clouds samples");
    Check(std::fabs(drift.warpV) < kCloudWarpWrap && std::fabs(drift.windV) < kCloudWindWrap,
          "the drift stays small enough for a float");
}

void CheckWind()
{
    OvercastReading heavy;
    AddLightSwapToOvercast(heavy, kWeightRow080, kStormLight, 1.0f);
    heavy = Finished(heavy);
    Check(CloudWindSpeedFor(heavy) > CloudWindSpeedFor(OvercastReading{}), "the clouds move faster in a storm");
}

void Run()
{
    CheckOvercastReading();
    CheckPreview();
    CheckCover();
    CheckColours();
    CheckField();
    CheckDriftWraps();
    CheckWind();
}
}
