#pragma once

#include <cstdint>
#include <vector>

struct BoltPoint
{
    float direction[3];
    float width;
    float brightness;
};

struct BoltStrand
{
    std::vector<BoltPoint> points;
};

struct LightningBolt
{
    bool visible = false;
    float topDirection[3] = {0.0f, 0.0f, 1.0f};
    std::vector<BoltStrand> strands;
};

struct BoltVertex
{
    float x;
    float y;
    float z;
    float w;
    float across;
    float brightness;
};

struct BoltCamera
{
    const float* worldToViewRotation;
    float viewToNdc[4];
    float pixelsPerNdc[2];
    float halfWidthPixels;
    float depth;
};

float BoltAzimuthFacing(const float* forward, float seed, bool mustBeInView);
LightningBolt BuildLightningBolt(float seed, float azimuthRadians, bool mustBeVisible);
void AddBoltRibbons(const LightningBolt& bolt, const BoltCamera& camera, std::vector<BoltVertex>& vertices);
