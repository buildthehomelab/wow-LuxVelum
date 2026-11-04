#include "lightning_bolt.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegrees = kPi / 180.0f;
constexpr float kLowestTopDegrees = 16.0f;
constexpr float kHighestTopDegrees = 32.0f;
constexpr float kBottomDegrees = -4.0f;
constexpr float kFootFadesWithin = 0.12f;
constexpr float kFootDriftDegrees = 6.0f;
constexpr float kChannelJagPerDrop = 0.17f;
constexpr int kChannelDoublings = 6;
constexpr int kBranchDoublings = 4;
constexpr int kFewestBranches = 2;
constexpr int kMostBranches = 6;
constexpr float kBranchStartsWithin = 0.72f;
constexpr float kShortestBranchShare = 0.12f;
constexpr float kLongestBranchShare = 0.32f;
constexpr float kBranchSpreadPerDrop = 0.45f;
constexpr float kBranchJagPerDrop = 0.14f;
constexpr float kBranchWidth = 0.55f;
constexpr float kBranchTipWidth = 0.2f;
constexpr float kBranchBrightness = 0.85f;
constexpr float kEmergesWithin = 0.14f;
constexpr float kFootWidth = 0.7f;
constexpr float kShareOfStrikesWithBolt = 0.75f;
constexpr float kShareOfBoltsAheadOfCamera = 0.7f;
constexpr float kAheadOfCameraDegrees = 45.0f;
constexpr float kInFrontOfCamera = 0.02f;

class BoltRandom
{
public:
    explicit BoltRandom(float seed) : m_state(static_cast<uint32_t>(seed * 4294967040.0f) ^ 0x9E3779B9u)
    {
        if (!m_state)
            m_state = 0x51F15EEDu;
        Unit();
        Unit();
    }

    float Unit()
    {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        return static_cast<float>(m_state & 0xFFFFFFu) / 16777216.0f;
    }

    float Between(float low, float high) { return low + (high - low) * Unit(); }
    float Sign() { return Unit() < 0.5f ? -1.0f : 1.0f; }

private:
    uint32_t m_state;
};

struct SkySpot
{
    float azimuth;
    float elevation;
};

void Direction(const SkySpot& spot, float* out)
{
    const float flat = std::cos(spot.elevation);
    out[0] = flat * std::cos(spot.azimuth);
    out[1] = flat * std::sin(spot.azimuth);
    out[2] = std::sin(spot.elevation);
}

std::vector<SkySpot> JaggedLine(const SkySpot& from, const SkySpot& to, int doublings, float jag, BoltRandom& random)
{
    std::vector<SkySpot> line = {from, to};
    float reach = std::fabs(from.elevation - to.elevation) * jag;
    for (int doubling = 0; doubling < doublings; ++doubling)
    {
        std::vector<SkySpot> finer;
        finer.reserve(line.size() * 2);
        for (size_t i = 0; i + 1 < line.size(); ++i)
        {
            finer.push_back(line[i]);
            finer.push_back({(line[i].azimuth + line[i + 1].azimuth) * 0.5f + random.Between(-reach, reach),
                             (line[i].elevation + line[i + 1].elevation) * 0.5f});
        }
        finer.push_back(line.back());
        line.swap(finer);
        reach *= 0.55f;
    }
    return line;
}

float Interpolated(float from, float to, float share)
{
    return from + (to - from) * share;
}

BoltStrand StrandAlong(const std::vector<SkySpot>& line, float widthFrom, float widthTo, float brightnessFrom,
                       float brightnessTo, float emergesWithin, float fadesWithin)
{
    BoltStrand strand;
    strand.points.reserve(line.size());
    const float last = static_cast<float>(line.size() - 1);
    for (size_t i = 0; i < line.size(); ++i)
    {
        const float share = static_cast<float>(i) / last;
        BoltPoint point = {};
        Direction(line[i], point.direction);
        point.width = Interpolated(widthFrom, widthTo, share);
        point.brightness = Interpolated(brightnessFrom, brightnessTo, share);
        if (emergesWithin > 0.0f)
            point.brightness *= std::min(share / emergesWithin, 1.0f);
        if (fadesWithin > 0.0f)
            point.brightness *= std::min((1.0f - share) / fadesWithin, 1.0f);
        strand.points.push_back(point);
    }
    return strand;
}

float ViewFacing(const BoltPoint& point, const BoltCamera& camera, float* view)
{
    const float* m = camera.worldToViewRotation;
    for (int j = 0; j < 3; ++j)
        view[j] = point.direction[0] * m[0 * 4 + j] + point.direction[1] * m[1 * 4 + j] +
                  point.direction[2] * m[2 * 4 + j];
    return view[2];
}

void AddRibbon(const std::vector<BoltPoint>& points, const BoltCamera& camera, std::vector<BoltVertex>& vertices)
{
    struct Projected
    {
        float x;
        float y;
    };
    std::vector<Projected> projected(points.size());
    for (size_t i = 0; i < points.size(); ++i)
    {
        float view[3];
        ViewFacing(points[i], camera, view);
        projected[i] = {view[0] / view[2] * camera.viewToNdc[0] + camera.viewToNdc[2],
                        view[1] / view[2] * camera.viewToNdc[1] + camera.viewToNdc[3]};
    }
    const auto offsetAt = [&](size_t i, float* ndcOffset) {
        const Projected& a = projected[i ? i - 1 : 0];
        const Projected& b = projected[i + 1 < projected.size() ? i + 1 : i];
        float dx = (b.x - a.x) * camera.pixelsPerNdc[0];
        float dy = (b.y - a.y) * camera.pixelsPerNdc[1];
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.0f)
        {
            dx = 1.0f;
            dy = 0.0f;
        }
        else
        {
            dx /= length;
            dy /= length;
        }
        const float halfWidth = camera.halfWidthPixels * points[i].width;
        ndcOffset[0] = -dy * halfWidth / camera.pixelsPerNdc[0];
        ndcOffset[1] = dx * halfWidth / camera.pixelsPerNdc[1];
    };
    for (size_t i = 0; i + 1 < points.size(); ++i)
    {
        float offsetA[2];
        float offsetB[2];
        offsetAt(i, offsetA);
        offsetAt(i + 1, offsetB);
        const Projected& a = projected[i];
        const Projected& b = projected[i + 1];
        const BoltVertex a0 = {a.x - offsetA[0], a.y - offsetA[1], 0.0f, 1.0f, -1.0f, points[i].brightness};
        const BoltVertex a1 = {a.x + offsetA[0], a.y + offsetA[1], 0.0f, 1.0f, 1.0f, points[i].brightness};
        const BoltVertex b0 = {b.x - offsetB[0], b.y - offsetB[1], 0.0f, 1.0f, -1.0f, points[i + 1].brightness};
        const BoltVertex b1 = {b.x + offsetB[0], b.y + offsetB[1], 0.0f, 1.0f, 1.0f, points[i + 1].brightness};
        vertices.push_back(a0);
        vertices.push_back(a1);
        vertices.push_back(b0);
        vertices.push_back(b0);
        vertices.push_back(a1);
        vertices.push_back(b1);
    }
}
}

float BoltAzimuthFacing(const float* forward, float seed, bool mustBeInView)
{
    BoltRandom random(seed * 0.5f + 0.25f);
    const float ahead = std::atan2(forward[1], forward[0]);
    if (mustBeInView || random.Unit() < kShareOfBoltsAheadOfCamera)
        return ahead + random.Between(-kAheadOfCameraDegrees, kAheadOfCameraDegrees) * kDegrees *
                           (mustBeInView ? 0.5f : 1.0f);
    return random.Between(-kPi, kPi);
}

LightningBolt BuildLightningBolt(float seed, float azimuthRadians, bool mustBeVisible)
{
    BoltRandom random(seed);
    LightningBolt bolt;
    const float visibleRoll = random.Unit();
    bolt.visible = mustBeVisible || visibleRoll < kShareOfStrikesWithBolt;
    const SkySpot top = {azimuthRadians, random.Between(kLowestTopDegrees, kHighestTopDegrees) * kDegrees};
    const SkySpot foot = {azimuthRadians + random.Between(-kFootDriftDegrees, kFootDriftDegrees) * kDegrees,
                          kBottomDegrees * kDegrees};
    Direction(top, bolt.topDirection);
    if (!bolt.visible)
        return bolt;

    const std::vector<SkySpot> channel = JaggedLine(top, foot, kChannelDoublings, kChannelJagPerDrop, random);
    bolt.strands.push_back(StrandAlong(channel, 1.0f, kFootWidth, 1.0f, 1.0f, kEmergesWithin, kFootFadesWithin));
    const int branches = kFewestBranches + static_cast<int>(random.Unit() * (kMostBranches - kFewestBranches + 1));
    for (int branch = 0; branch < branches; ++branch)
    {
        const size_t from = static_cast<size_t>(random.Between(kEmergesWithin, kBranchStartsWithin) *
                                                static_cast<float>(channel.size() - 1));
        const SkySpot& start = channel[from];
        const float drop =
            (start.elevation - foot.elevation) * random.Between(kShortestBranchShare, kLongestBranchShare);
        const SkySpot tip = {start.azimuth + random.Sign() * drop * kBranchSpreadPerDrop *
                                                 random.Between(0.5f, 1.5f),
                             start.elevation - drop};
        const std::vector<SkySpot> line = JaggedLine(start, tip, kBranchDoublings, kBranchJagPerDrop, random);
        bolt.strands.push_back(StrandAlong(line, kBranchWidth, kBranchTipWidth, kBranchBrightness, 0.0f, 0.0f, 0.0f));
    }
    return bolt;
}

void AddBoltRibbons(const LightningBolt& bolt, const BoltCamera& camera, std::vector<BoltVertex>& vertices)
{
    for (const BoltStrand& strand : bolt.strands)
    {
        std::vector<BoltPoint> run;
        run.reserve(strand.points.size());
        for (const BoltPoint& point : strand.points)
        {
            float view[3];
            if (ViewFacing(point, camera, view) > kInFrontOfCamera)
            {
                run.push_back(point);
                continue;
            }
            if (run.size() > 1)
                AddRibbon(run, camera, vertices);
            run.clear();
        }
        if (run.size() > 1)
            AddRibbon(run, camera, vertices);
    }
}
