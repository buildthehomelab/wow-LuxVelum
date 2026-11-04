#pragma once

#include "lightning_bolt.h"

#include <cmath>
#include <vector>

namespace lightning_bolt_checks
{
constexpr float kSeed = 0.37f;
constexpr float kFacingNorth = 0.0f;
constexpr float kSameDirection = 1e-5f;

float Elevation(const BoltPoint& point)
{
    return std::asin(point.direction[2]);
}

float Azimuth(const BoltPoint& point)
{
    return std::atan2(point.direction[1], point.direction[0]);
}

void CheckGeometry()
{
    const LightningBolt bolt = BuildLightningBolt(kSeed, kFacingNorth, true);
    Check(bolt.visible && bolt.strands.size() >= 3, "a strike has a channel and branches");
    const std::vector<BoltPoint>& channel = bolt.strands[0].points;
    Check(channel.size() == 65, "the channel has enough points to look jagged");
    Check(Elevation(channel.front()) > 0.25f, "it starts high in the clouds");
    Check(Elevation(channel.back()) < 0.0f, "it reaches below the horizon, so the land covers its foot");
    bool descends = true;
    bool ahead = true;
    for (size_t i = 0; i + 1 < channel.size(); ++i)
    {
        descends = descends && Elevation(channel[i + 1]) < Elevation(channel[i]);
        ahead = ahead && std::fabs(Azimuth(channel[i])) < 0.5f;
    }
    Check(descends, "the channel only goes down");
    Check(ahead, "the channel stays near the azimuth it was given");
    Check(channel.front().brightness == 0.0f && channel[32].brightness == 1.0f, "it emerges from the clouds");
    Check(channel.back().brightness == 0.0f, "its foot fades out, so it doesn't end square when seen from high up");

    bool branchesJoin = true;
    bool branchesFade = true;
    for (size_t s = 1; s < bolt.strands.size(); ++s)
    {
        const std::vector<BoltPoint>& branch = bolt.strands[s].points;
        bool joined = false;
        for (const BoltPoint& point : channel)
            joined = joined || (std::fabs(point.direction[0] - branch[0].direction[0]) < kSameDirection &&
                                std::fabs(point.direction[1] - branch[0].direction[1]) < kSameDirection &&
                                std::fabs(point.direction[2] - branch[0].direction[2]) < kSameDirection);
        branchesJoin = branchesJoin && joined;
        branchesFade = branchesFade && branch.back().brightness == 0.0f && branch.front().brightness > 0.0f;
    }
    Check(branchesJoin, "every branch leaves from the channel");
    Check(branchesFade, "branches fade out at the tip");

    const LightningBolt again = BuildLightningBolt(kSeed, kFacingNorth, true);
    Check(again.strands[0].points[10].direction[1] == channel[10].direction[1], "the same seed gives the same bolt");
    const LightningBolt other = BuildLightningBolt(kSeed + 0.2f, kFacingNorth, true);
    Check(other.strands[0].points[10].direction[1] != channel[10].direction[1], "another seed gives another bolt");

    int withBolt = 0;
    constexpr int kStrikes = 1000;
    for (int strike = 0; strike < kStrikes; ++strike)
        withBolt += BuildLightningBolt((strike + 0.5f) / kStrikes, kFacingNorth, false).visible ? 1 : 0;
    Check(withBolt > 600 && withBolt < 900, "some strikes only light up the clouds");
}

void CheckAzimuth()
{
    const float north[3] = {1.0f, 0.0f, 0.0f};
    bool inView = true;
    int ahead = 0;
    constexpr int kStrikes = 1000;
    for (int strike = 0; strike < kStrikes; ++strike)
    {
        const float seed = (strike + 0.5f) / kStrikes;
        inView = inView && std::fabs(BoltAzimuthFacing(north, seed, true)) < 0.4f;
        ahead += std::fabs(BoltAzimuthFacing(north, seed, false)) < 0.8f ? 1 : 0;
    }
    Check(inView, "a strike asked for lands where the camera looks");
    Check(ahead > 600, "most strikes land ahead of the camera");
}

void CheckRibbons()
{
    const float lookingNorth[16] = {0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                                    0.0f, 1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f};
    BoltCamera camera = {lookingNorth, {1.0f, 1.0f, 0.0f, 0.0f}, {960.0f, 540.0f}, 10.0f};
    const LightningBolt ahead = BuildLightningBolt(kSeed, kFacingNorth, true);
    std::vector<BoltVertex> vertices;
    AddBoltRibbons(ahead, camera, vertices);
    size_t expected = 0;
    for (const BoltStrand& strand : ahead.strands)
        expected += (strand.points.size() - 1) * 6;
    Check(vertices.size() == expected, "a bolt ahead of the camera draws two triangles per segment");
    bool onScreen = true;
    bool widthRight = true;
    for (size_t i = 0; i + 1 < 6 * 64; i += 6)
    {
        onScreen = onScreen && std::fabs(vertices[i].x) < 1.0f && vertices[i].y < 1.0f && vertices[i].w == 1.0f;
        const float dx = (vertices[i + 1].x - vertices[i].x) * camera.pixelsPerNdc[0];
        const float dy = (vertices[i + 1].y - vertices[i].y) * camera.pixelsPerNdc[1];
        const float across = std::sqrt(dx * dx + dy * dy);
        widthRight = widthRight && across > 13.0f && across < 20.5f;
        widthRight = widthRight && vertices[i].across == -1.0f && vertices[i + 1].across == 1.0f;
    }
    Check(onScreen, "it lands on the screen");
    Check(widthRight, "the ribbon keeps its width in pixels whichever way the bolt runs");

    const LightningBolt behind = BuildLightningBolt(kSeed, 3.14159f, true);
    vertices.clear();
    AddBoltRibbons(behind, camera, vertices);
    Check(vertices.empty(), "a bolt behind the camera draws nothing");
}

void Run()
{
    CheckGeometry();
    CheckAzimuth();
    CheckRibbons();
}
}
