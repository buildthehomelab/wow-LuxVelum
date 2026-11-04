#include "vf_common.hlsli"

float4 cCloudShape : register(c80);
float4 cCloudDrift : register(c81);
float4 cCloudPlace : register(c82);
float4 cCloudLit : register(c83);
float4 cCloudDark : register(c84);
float4 cCloudToSun : register(c85);
float4 cCloudSunColour : register(c86);
float4 cCloudFlash : register(c87);
float4 cCloudStrike : register(c88);

sampler2D sCloudField : register(s0);

static const float kHorizonStart = 0.01;
static const float kHorizonFull = 0.09;
static const float kFieldSize = 512;
static const float kWarpRepeats = 0.5;
static const float kWarpReach = 0.05;
static const float kBillowRepeats = 1.2;
static const float kBillowAmount = 0.55;
static const float kFineRepeats = 4.8;
static const float kFineAmount = 0.19;
static const float kBillowMean = BILLOW_MEAN;
static const float kDeckDepth = 0.2;
static const float kShallowDepthShare = 0.25;
static const float kShallowRayEnd = 0.3;
static const float kShallowRayStart = 0.02;
static const float kEdgeBelow = -0.05;
static const float kEdgeAbove = 0.08;
static const float kNormalReach = 0.006;
static const float kNormalSteepness = 0.012;
static const float kSunFromBelowShare = 0.35;
static const float kWrappedLight = 0.5;
static const float kThickShadow = 0.6;
static const float kBulgeContrast = 2.4;
static const float kBulgeFloor = 0.5;
static const float kBulgeGain = 0.75;
static const float kHollowMips = 3;
static const float kMassMips = 4;
static const float kMassContrast = 3;
static const float kMassShadow = 0.75;
static const float kMassLight = 1.25;
static const float kSunGlowPower = 8;
static const float kFlashRepeats = 0.25;
static const float kStrikeEdge = 0.45;
static const float kStrikeSharpness = 3;
static const float kFlashEverywhere = 0.025;
static const float kFlashArea = 0.08;
static const float kStrikeFalloff = 16;
static const float kStrikeFacingOffset = 0.45;
static const float kStrikeFacingGain = 0.9;
static const float kStrikeNear = 0.12;
static const float kStrikeFacing = 1.1;
static const float kStrikeThrough = 0.7;
static const float3 kFlashColour = float3(0.75, 0.82, 1.0);

float2 PlaneOffset(float3 ray)
{
    return ray.xy / (ray.z + cCloudPlace.w) * cCloudPlace.z;
}

float MarchDensity(float2 place, float2 dx, float2 dy)
{
    float body = tex2Dgrad(sCloudField, place, dx, dy).r;
    float billow = tex2Dgrad(sCloudField, place * kBillowRepeats, dx * kBillowRepeats, dy * kBillowRepeats).g;
    return (body + (billow - kBillowMean) * kBillowAmount - cCloudShape.x) / cCloudShape.y;
}

float4 main(float2 pixel : VPOS) : COLOR0
{
    float3 ray = normalize(ViewToWorldDirection(ViewRayAtUnitDepth(pixel + 0.5)));
    float horizon = smoothstep(kHorizonStart, kHorizonFull, ray.z);
    clip(horizon - 0.001);

    float2 plane = PlaneOffset(ray);
    float2 place = plane + cCloudPlace.xy + cCloudDrift.xy;
    float2 warp = tex2D(sCloudField, place * kWarpRepeats + cCloudDrift.zw).ba - 0.5;
    float2 base = place + warp * kWarpReach;
    float2 dx = ddx(base);
    float2 dy = ddy(base);
    float lod = log2(max(max(length(dx), length(dy)) * kFieldSize, 0.000001));

    float depthShare = lerp(kShallowDepthShare, 1, smoothstep(kShallowRayStart, kShallowRayEnd, ray.z));
    float2 climb = plane * kDeckDepth * depthShare;

    float best = -9;
    float hitShare = 1;
    float hit = 0;
    float overBefore = -1;
    float shareBefore = 0;
    for (int step = 1; step <= STEPS; ++step)
    {
        float share = float(step) / STEPS;
        float over = MarchDensity(base + climb * share, dx, dy) + share - 1;
        if (hit == 0 && over >= 0)
        {
            hitShare = shareBefore + (share - shareBefore) * saturate(-overBefore / max(over - overBefore, 0.0001));
            hit = 1;
        }
        best = max(best, over);
        overBefore = over;
        shareBefore = share;
    }

    float2 surface = base + climb * hitShare;
    float atSurface = MarchDensity(surface, dx, dy);
    float fine = tex2Dgrad(sCloudField, surface * kFineRepeats, dx * kFineRepeats, dy * kFineRepeats).g;
    float density = atSurface + (fine - kBillowMean) * kFineAmount / cCloudShape.y;
    float2 slope = float2(MarchDensity(surface + float2(kNormalReach, 0), dx, dy) - atSurface,
                          MarchDensity(surface + float2(0, kNormalReach), dx, dy) - atSurface) / kNormalReach;
    float3 normal = normalize(float3(-slope * kNormalSteepness, -1));

    float sunlit =
        saturate(dot(normal.xy, cCloudToSun.xy) + normal.z * cCloudToSun.z * kSunFromBelowShare + kWrappedLight);
    float thick = saturate(density);
    float body = tex2Dgrad(sCloudField, surface, dx, dy).r;
    float coarseLod = max(lod, 0);
    float hollow = tex2Dlod(sCloudField, float4(surface, 0, coarseLod + kHollowMips)).r;
    float mass = tex2Dlod(sCloudField, float4(surface, 0, coarseLod + kMassMips)).r;
    float bulge = saturate((body - hollow) * kBulgeContrast + 0.5);
    float masses = saturate((mass - 0.5) * kMassContrast + 0.5);
    float light = saturate(sunlit * (1 - kThickShadow * thick) * (kBulgeFloor + kBulgeGain * bulge) *
                           (kMassLight - kMassShadow * masses));
    float3 colour = lerp(cCloudDark.rgb, cCloudLit.rgb, light);

    float sunGlow = pow(saturate(dot(ray, cCloudToSun.xyz)), kSunGlowPower);
    colour += cCloudSunColour.rgb * sunGlow * (1 - thick) * cCloudSunColour.a;

    float strikeArea = saturate((tex2D(sCloudField, place * kFlashRepeats + cCloudFlash.xy).r - kStrikeEdge) *
                                kStrikeSharpness);
    float2 toStrike = PlaneOffset(cCloudStrike.xyz) + cCloudPlace.xy + cCloudDrift.xy - surface;
    float strikeReach = length(toStrike) + 0.0001;
    float falloff = strikeReach * kStrikeFalloff;
    float nearStrike = cCloudStrike.w / (1 + falloff * falloff);
    float facing = saturate(dot(normal.xy, toStrike) / strikeReach * kStrikeFacingGain + kStrikeFacingOffset);
    float through = (1 - thick) * (1 - thick);
    colour += kFlashColour * cCloudFlash.z *
              (kFlashEverywhere + strikeArea * strikeArea * kFlashArea +
               nearStrike * (kStrikeNear + kStrikeFacing * facing + kStrikeThrough * through));

    return float4(saturate(colour), smoothstep(kEdgeBelow, kEdgeAbove, best) * horizon * cCloudLit.a);
}
