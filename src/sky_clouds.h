#pragma once

#include <cstdint>
#include <vector>

constexpr uint32_t kCloudFieldSize = 512;

constexpr uint32_t kLightCopyStep = 10000;
constexpr uint32_t kOvercastFirstWeightRow = 8901;
constexpr uint32_t kOvercastWeightRows = 18;
constexpr float kOvercastWeightPerRow = 0.05f;
constexpr uint32_t kOvercastFirstPaletteRow = 8951;
constexpr uint32_t kNightLevelFirstRow = 9001;
constexpr uint32_t kNightLevelLastRow = 9005;
constexpr double kCloudWindWrap = 4.0;
constexpr double kCloudWarpWrap = 1.0;

enum OvercastPalette
{
    kOvercastRain,
    kOvercastStorm,
    kOvercastSnow,
    kOvercastSand,
    kOvercastPalettes
};

struct OvercastReading
{
    float weight = 0.0f;
    float share[kOvercastPalettes] = {};
};

struct CloudTuning
{
    float previewWeight = 0.0f;
    float previewStorm = 0.0f;
    float fairCover = 0.0f;
    float darkness = 1.0f;
    float opacity = 1.0f;
};

struct CloudShape
{
    float cover = 0.0f;
    float threshold = 1.0f;
    float softness = 0.0f;
    float thickRange = 0.0f;
    float opacity = 0.0f;
};

struct CloudColours
{
    float lit[3] = {};
    float dark[3] = {};
};

struct CloudDrift
{
    double windU = 0.0;
    double windV = 0.0;
    double warpU = 0.0;
    double warpV = 0.0;
};

struct CloudLight
{
    float sky[3] = {};
    float ambient[3] = {};
    float direct[3] = {};
};

void AddLightSwapToOvercast(OvercastReading& reading, uint32_t areaLight, uint32_t light, float fade);
void FinishOvercast(OvercastReading& reading);
OvercastReading OvercastWithPreview(const OvercastReading& reading, const CloudTuning& tuning);
float CloudCoverOfWeight(float weight);
CloudShape CloudShapeFor(const OvercastReading& reading, const CloudTuning& tuning);
CloudColours CloudColoursFor(const CloudLight& light, const OvercastReading& reading, const CloudTuning& tuning);
float CloudWindSpeedFor(const OvercastReading& reading);
void AdvanceCloudDrift(CloudDrift& drift, double seconds, double speed);

std::vector<uint32_t> BuildCloudField(uint32_t size);
const std::vector<uint32_t>& PreparedCloudField();
std::vector<uint32_t> ShrinkCloudField(const std::vector<uint32_t>& field, uint32_t size);
