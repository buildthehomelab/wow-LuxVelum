#include "sky_clouds.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace
{
struct CoverKey
{
    float weight;
    float cover;
};

constexpr CoverKey kCoverByWeight[] = {{0.0f, 0.0f},  {0.05f, 0.14f}, {0.15f, 0.35f}, {0.30f, 0.50f},
                                       {0.45f, 0.68f}, {0.60f, 0.82f}, {0.80f, 0.95f}, {0.90f, 1.0f}};

constexpr float kThinnestThickRange = 0.2f;
constexpr float kThickestThickRange = 0.55f;
constexpr float kThickRangePerCover = 0.6f;
constexpr float kBillowPullWhenClosing = 0.2f;
constexpr float kStartsClosingAtCover = 0.8f;
constexpr float kMiddleOfScatteredCloud = 0.4f;
constexpr float kMiddleOfClosedDeck = 0.8f;
constexpr float kMiddleStartsRisingAtCover = 0.55f;
constexpr float kMiddleFullyRisenAtCover = 0.9f;
constexpr float kLightestOvercast = 0.15f;
constexpr float kHeaviestOvercast = 0.8f;
constexpr float kBellyAtLightestOvercast = 0.72f;
constexpr float kBellyAtHeaviest[kOvercastPalettes] = {0.42f, 0.28f, 0.72f, 0.58f};
constexpr float kLitDimAtHeaviest[kOvercastPalettes] = {0.85f, 0.7f, 1.0f, 0.9f};
constexpr float kSkyInCloud = 0.5f;
constexpr float kAmbientInCloud = 0.55f;
constexpr float kDirectInCloud = 0.35f;
constexpr float kCloudGain = 1.2f;
constexpr float kWindAtHeaviest = 3.0f;
constexpr double kSecondsPerWindTile = 900.0;
constexpr double kSecondsPerWarpTile = 2700.0;
constexpr double kWarpCrosswind = 0.6;
constexpr double kWindBearingRadians = 20.0 * 3.14159265358979323846 / 180.0;

struct PuffTier
{
    uint32_t cells;
    uint32_t seed;
    float smallestRadius;
    float largestRadius;
    float lowestHeight;
    float highestHeight;
};

constexpr PuffTier kPuffTiers[] = {{14, 1000, 0.5f, 1.0f, 0.65f, 1.0f}, {28, 2000, 0.5f, 0.85f, 0.45f, 0.75f}};
constexpr int kPuffReach = 2;
constexpr float kPuffJoinSharpness = 24.0f;
constexpr float kPuffFadedSpread = 4.0f;
constexpr uint32_t kGatherCells = 3;
constexpr float kGatherPull = 0.55f;
constexpr uint32_t kFineCells = 32;
constexpr float kFineShare = 0.04f;
constexpr uint32_t kWarpCells = 4;
constexpr uint32_t kDetailCells = 8;
constexpr float kWarpReach = 0.06f;
constexpr float kFineGain = 0.55f;
constexpr float kBillowGain = 0.5f;
constexpr int kBillowLayers = 4;
constexpr float kCommonGain = 0.5f;

float Mix(float a, float b, float t)
{
    return a + (b - a) * t;
}

float SmoothShare(float low, float high, float x)
{
    const float t = std::clamp((x - low) / (high - low), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

uint32_t LatticeHash(uint32_t x, uint32_t y, uint32_t seed)
{
    uint32_t h = (x * 0x8DA6B343u) ^ (y * 0xD8163841u) ^ (seed * 0xCB1AB31Fu);
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

float LatticeValue(uint32_t x, uint32_t y, uint32_t seed)
{
    return static_cast<float>(LatticeHash(x, y, seed) & 0xFFFFu) / 65535.0f;
}

float QuinticFade(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float ValueNoise(float u, float v, uint32_t cells, uint32_t seed)
{
    const float x = u * cells;
    const float y = v * cells;
    const uint32_t x0 = static_cast<uint32_t>(x) % cells;
    const uint32_t y0 = static_cast<uint32_t>(y) % cells;
    const uint32_t x1 = (x0 + 1) % cells;
    const uint32_t y1 = (y0 + 1) % cells;
    const float fx = QuinticFade(x - std::floor(x));
    const float fy = QuinticFade(y - std::floor(y));
    return Mix(Mix(LatticeValue(x0, y0, seed), LatticeValue(x1, y0, seed), fx),
               Mix(LatticeValue(x0, y1, seed), LatticeValue(x1, y1, seed), fx), fy);
}

float LayeredNoise(float u, float v, uint32_t cells, int layers, float gain, uint32_t seed)
{
    float total = 0.0f;
    float amplitude = 1.0f;
    float sum = 0.0f;
    for (int layer = 0; layer < layers; ++layer)
    {
        total += amplitude * ValueNoise(u, v, cells << layer, seed + layer);
        sum += amplitude;
        amplitude *= gain;
    }
    return total / sum;
}

float Wrapped(float u)
{
    return u - std::floor(u);
}

struct Puff
{
    float x;
    float y;
    float radius;
    float height;
};

std::vector<Puff> PuffsOf(const PuffTier& tier)
{
    std::vector<Puff> puffs(static_cast<size_t>(tier.cells) * tier.cells);
    for (uint32_t cy = 0; cy < tier.cells; ++cy)
        for (uint32_t cx = 0; cx < tier.cells; ++cx)
        {
            Puff& puff = puffs[static_cast<size_t>(cy) * tier.cells + cx];
            puff.x = 0.1f + 0.8f * LatticeValue(cx, cy, tier.seed);
            puff.y = 0.1f + 0.8f * LatticeValue(cx, cy, tier.seed + 1);
            puff.radius = Mix(tier.smallestRadius, tier.largestRadius, LatticeValue(cx, cy, tier.seed + 2));
            const float gather =
                LayeredNoise((static_cast<float>(cx) + puff.x) / static_cast<float>(tier.cells),
                             (static_cast<float>(cy) + puff.y) / static_cast<float>(tier.cells), kGatherCells, 2,
                             kCommonGain, 300);
            puff.height = Mix(tier.lowestHeight, tier.highestHeight, LatticeValue(cx, cy, tier.seed + 3)) *
                          (1.0f - kGatherPull + kGatherPull * gather);
        }
    return puffs;
}

float PuffSum(float u, float v, const PuffTier& tier, const std::vector<Puff>& puffs)
{
    const float x = u * static_cast<float>(tier.cells);
    const float y = v * static_cast<float>(tier.cells);
    const int fx = static_cast<int>(std::floor(x));
    const int fy = static_cast<int>(std::floor(y));
    const int cells = static_cast<int>(tier.cells);
    float sum = 0.0f;
    for (int oy = -kPuffReach; oy <= kPuffReach; ++oy)
        for (int ox = -kPuffReach; ox <= kPuffReach; ++ox)
        {
            const int cx = fx + ox;
            const int cy = fy + oy;
            const Puff& puff = puffs[static_cast<size_t>(((cy % cells) + cells) % cells) * tier.cells +
                                     static_cast<size_t>(((cx % cells) + cells) % cells)];
            const float dx = x - (static_cast<float>(cx) + puff.x);
            const float dy = y - (static_cast<float>(cy) + puff.y);
            const float spread = (dx * dx + dy * dy) / (puff.radius * puff.radius);
            sum += spread > kPuffFadedSpread ? 1.0f : std::exp(kPuffJoinSharpness * puff.height * std::exp(-spread));
        }
    return sum;
}

float BodyAt(float u, float v, const std::vector<std::vector<Puff>>& tiers)
{
    float sum = 0.0f;
    for (size_t tier = 0; tier < tiers.size(); ++tier)
        sum += PuffSum(u, v, kPuffTiers[tier], tiers[tier]);
    return std::log(sum) / kPuffJoinSharpness +
           kFineShare * LayeredNoise(u, v, kFineCells, 4, kFineGain, 200);
}

float CellGradientDot(uint32_t x, uint32_t y, uint32_t seed, float dx, float dy)
{
    const float angle = LatticeValue(x, y, seed) * 6.2831853f;
    return std::cos(angle) * dx + std::sin(angle) * dy;
}

float GradientNoise(float u, float v, uint32_t cells, uint32_t seed)
{
    const float x = u * cells;
    const float y = v * cells;
    const uint32_t x0 = static_cast<uint32_t>(x) % cells;
    const uint32_t y0 = static_cast<uint32_t>(y) % cells;
    const uint32_t x1 = (x0 + 1) % cells;
    const uint32_t y1 = (y0 + 1) % cells;
    const float dx = x - std::floor(x);
    const float dy = y - std::floor(y);
    const float fx = QuinticFade(dx);
    const float fy = QuinticFade(dy);
    return Mix(Mix(CellGradientDot(x0, y0, seed, dx, dy), CellGradientDot(x1, y0, seed, dx - 1.0f, dy), fx),
               Mix(CellGradientDot(x0, y1, seed, dx, dy - 1.0f),
                   CellGradientDot(x1, y1, seed, dx - 1.0f, dy - 1.0f), fx),
               fy);
}

float BillowsAt(float u, float v, uint32_t cells, int layers, uint32_t seed)
{
    float total = 0.0f;
    float amplitude = 1.0f;
    float sum = 0.0f;
    for (int layer = 0; layer < layers; ++layer)
    {
        total += amplitude * std::fabs(GradientNoise(u, v, cells << layer, seed + layer));
        sum += amplitude;
        amplitude *= kBillowGain;
    }
    return total / sum;
}

std::vector<float> NormalisedToRange(const std::vector<float>& values)
{
    const auto range = std::minmax_element(values.begin(), values.end());
    const float low = *range.first;
    const float span = std::max(*range.second - low, 1e-6f);
    std::vector<float> normalised(values.size());
    for (size_t i = 0; i < values.size(); ++i)
        normalised[i] = (values[i] - low) / span;
    return normalised;
}

std::vector<float> EvenlySpread(const std::vector<float>& values)
{
    std::vector<uint32_t> order(values.size());
    std::iota(order.begin(), order.end(), 0u);
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        return values[a] < values[b] || (values[a] == values[b] && a < b);
    });
    std::vector<float> spread(values.size());
    const float last = static_cast<float>(values.size() - 1);
    for (size_t rank = 0; rank < order.size(); ++rank)
        spread[order[rank]] = static_cast<float>(rank) / last;
    return spread;
}

uint32_t ToByte(float value)
{
    return static_cast<uint32_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

uint32_t PackTexel(uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    return (a << 24) | (r << 16) | (g << 8) | b;
}

uint32_t Channel(uint32_t texel, int shift)
{
    return (texel >> shift) & 0xFFu;
}
}

void AddLightSwapToOvercast(OvercastReading& reading, uint32_t areaLight, uint32_t light, float fade)
{
    if (areaLight >= kNightLevelFirstRow && areaLight <= kNightLevelLastRow && fade > 0.0f)
    {
        const float below = 1.0f - std::min(fade, 1.0f);
        reading.weight *= below;
        for (float& share : reading.share)
            share *= below;
        return;
    }
    const uint32_t weightRow = areaLight % kLightCopyStep;
    const uint32_t paletteRow = light % kLightCopyStep;
    if (weightRow < kOvercastFirstWeightRow || weightRow >= kOvercastFirstWeightRow + kOvercastWeightRows)
        return;
    if (paletteRow < kOvercastFirstPaletteRow || paletteRow >= kOvercastFirstPaletteRow + kOvercastPalettes)
        return;
    if (!(fade > 0.0f))
        return;
    const float weight =
        static_cast<float>(weightRow - kOvercastFirstWeightRow + 1) * kOvercastWeightPerRow * std::min(fade, 1.0f);
    reading.weight += weight;
    reading.share[paletteRow - kOvercastFirstPaletteRow] += weight;
}

void FinishOvercast(OvercastReading& reading)
{
    if (reading.weight <= 0.0f)
    {
        reading = {};
        return;
    }
    for (float& share : reading.share)
        share /= reading.weight;
    reading.weight = std::min(reading.weight, 1.0f);
}

OvercastReading OvercastWithPreview(const OvercastReading& reading, const CloudTuning& tuning)
{
    if (tuning.previewWeight <= 0.0f)
        return reading;
    OvercastReading preview;
    const float storm = std::clamp(tuning.previewStorm, 0.0f, 1.0f);
    preview.weight = std::min(tuning.previewWeight, 1.0f);
    preview.share[kOvercastRain] = 1.0f - storm;
    preview.share[kOvercastStorm] = storm;
    return preview;
}

float CloudCoverOfWeight(float weight)
{
    const size_t keys = sizeof(kCoverByWeight) / sizeof(kCoverByWeight[0]);
    if (weight <= kCoverByWeight[0].weight)
        return kCoverByWeight[0].cover;
    for (size_t i = 1; i < keys; ++i)
        if (weight <= kCoverByWeight[i].weight)
        {
            const CoverKey& a = kCoverByWeight[i - 1];
            const CoverKey& b = kCoverByWeight[i];
            return Mix(a.cover, b.cover, (weight - a.weight) / (b.weight - a.weight));
        }
    return kCoverByWeight[keys - 1].cover;
}

CloudShape CloudShapeFor(const OvercastReading& reading, const CloudTuning& tuning)
{
    CloudShape shape;
    shape.cover = std::max(CloudCoverOfWeight(reading.weight), std::clamp(tuning.fairCover, 0.0f, 1.0f));
    shape.threshold =
        1.0f - shape.cover - kBillowPullWhenClosing * SmoothShare(kStartsClosingAtCover, 1.0f, shape.cover);
    shape.thickRange = std::clamp(shape.cover * kThickRangePerCover, kThinnestThickRange, kThickestThickRange);
    shape.middle = Mix(kMiddleOfScatteredCloud, kMiddleOfClosedDeck,
                       SmoothShare(kMiddleStartsRisingAtCover, kMiddleFullyRisenAtCover, shape.cover));
    shape.opacity = std::clamp(tuning.opacity, 0.0f, 1.0f);
    return shape;
}

CloudColours CloudColoursFor(const CloudLight& light, const OvercastReading& reading, const CloudTuning& tuning)
{
    const float heavy = SmoothShare(kLightestOvercast, kHeaviestOvercast, reading.weight);
    float bellyAtHeaviest = 0.0f;
    float litDimAtHeaviest = 0.0f;
    float shares = 0.0f;
    for (int palette = 0; palette < kOvercastPalettes; ++palette)
    {
        bellyAtHeaviest += reading.share[palette] * kBellyAtHeaviest[palette];
        litDimAtHeaviest += reading.share[palette] * kLitDimAtHeaviest[palette];
        shares += reading.share[palette];
    }
    if (shares <= 0.0f)
    {
        bellyAtHeaviest = kBellyAtLightestOvercast;
        litDimAtHeaviest = 1.0f;
    }
    const float belly = std::clamp(Mix(kBellyAtLightestOvercast, bellyAtHeaviest, heavy) /
                                       std::max(tuning.darkness, 0.01f),
                                   0.0f, 1.0f);
    const float litDim = Mix(1.0f, litDimAtHeaviest, heavy);
    CloudColours colours;
    for (int k = 0; k < 3; ++k)
    {
        const float body =
            kSkyInCloud * light.sky[k] + kAmbientInCloud * light.ambient[k] + kDirectInCloud * light.direct[k];
        colours.lit[k] = std::min(body * kCloudGain * litDim, 1.0f);
        colours.dark[k] = std::min(body * kCloudGain * belly, 1.0f);
    }
    return colours;
}

float CloudWindSpeedFor(const OvercastReading& reading)
{
    return Mix(1.0f, kWindAtHeaviest, SmoothShare(kLightestOvercast, kHeaviestOvercast, reading.weight));
}

void AdvanceCloudDrift(CloudDrift& drift, double seconds, double speed)
{
    const double step = seconds * speed;
    const double windTiles = step / kSecondsPerWindTile;
    const double warpTiles = step / kSecondsPerWarpTile;
    drift.windU = std::fmod(drift.windU + windTiles * std::cos(kWindBearingRadians), kCloudWindWrap);
    drift.windV = std::fmod(drift.windV + windTiles * std::sin(kWindBearingRadians), kCloudWindWrap);
    drift.warpU = std::fmod(drift.warpU + warpTiles, kCloudWarpWrap);
    drift.warpV = std::fmod(drift.warpV - warpTiles * kWarpCrosswind, kCloudWarpWrap);
}

const std::vector<uint32_t>& PreparedCloudField()
{
    static const std::vector<uint32_t> field = BuildCloudField(kCloudFieldSize);
    return field;
}

std::vector<uint32_t> BuildCloudField(uint32_t size)
{
    const size_t texels = static_cast<size_t>(size) * size;
    std::vector<float> body(texels);
    std::vector<float> detail(texels);
    std::vector<float> warpU(texels);
    std::vector<float> warpV(texels);
    const float texelSize = 1.0f / static_cast<float>(size);
    std::vector<std::vector<Puff>> tiers;
    for (const PuffTier& tier : kPuffTiers)
        tiers.push_back(PuffsOf(tier));
    for (uint32_t y = 0; y < size; ++y)
        for (uint32_t x = 0; x < size; ++x)
        {
            const float u = static_cast<float>(x) * texelSize;
            const float v = static_cast<float>(y) * texelSize;
            const size_t at = static_cast<size_t>(y) * size + x;
            warpU[at] = LayeredNoise(u, v, kWarpCells, 3, kCommonGain, 400);
            warpV[at] = LayeredNoise(u, v, kWarpCells, 3, kCommonGain, 500);
            body[at] = BodyAt(Wrapped(u + (warpU[at] - 0.5f) * kWarpReach),
                              Wrapped(v + (warpV[at] - 0.5f) * kWarpReach), tiers);
            detail[at] = BillowsAt(u, v, kDetailCells, kBillowLayers, 600);
        }
    const std::vector<float> spreadBody = EvenlySpread(body);
    const std::vector<float> spreadDetail = NormalisedToRange(detail);
    const std::vector<float> spreadU = EvenlySpread(warpU);
    const std::vector<float> spreadV = EvenlySpread(warpV);
    std::vector<uint32_t> field(texels);
    for (size_t i = 0; i < texels; ++i)
        field[i] = PackTexel(ToByte(spreadBody[i]), ToByte(spreadDetail[i]), ToByte(spreadU[i]), ToByte(spreadV[i]));
    return field;
}

std::vector<uint32_t> ShrinkCloudField(const std::vector<uint32_t>& field, uint32_t size)
{
    const uint32_t half = size / 2;
    std::vector<uint32_t> smaller(static_cast<size_t>(half) * half);
    for (uint32_t y = 0; y < half; ++y)
        for (uint32_t x = 0; x < half; ++x)
        {
            uint32_t channels[4] = {};
            for (uint32_t oy = 0; oy < 2; ++oy)
                for (uint32_t ox = 0; ox < 2; ++ox)
                {
                    const uint32_t texel = field[static_cast<size_t>(y * 2 + oy) * size + x * 2 + ox];
                    for (int c = 0; c < 4; ++c)
                        channels[c] += Channel(texel, c * 8);
                }
            smaller[static_cast<size_t>(y) * half + x] =
                ((channels[3] + 2) / 4) << 24 | ((channels[2] + 2) / 4) << 16 | ((channels[1] + 2) / 4) << 8 |
                ((channels[0] + 2) / 4);
        }
    return smaller;
}
