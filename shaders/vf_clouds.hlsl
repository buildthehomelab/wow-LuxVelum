#include "vf_common.hlsli"

float4 cCloudShape : register(c100);
float4 cCloudDrift : register(c101);
float4 cCloudPlace : register(c102);
float4 cCloudLit : register(c103);
float4 cCloudDark : register(c104);
float4 cCloudToSun : register(c105);
float4 cCloudSunColour : register(c106);
float4 cCloudFlash : register(c107);

sampler2D sCloudField : register(s0);

static const float kHorizonStart = 0.01;
static const float kHorizonFull = 0.09;
static const float kDetailRepeats = 3;
static const float kDetailAmount = 0.2;
static const float kFineRepeats = 11;
static const float kFineAmount = 0.06;
static const float kBillowShading = 0.3;
static const float kShadeFloor = 0.35;
static const float kWarpRepeats = 0.5;
static const float kWarpReach = 0.05;
static const float kSunReach = 0.03;
static const float kSunSideShading = 1.6;
static const float kFlashRepeats = 0.25;
static const float kSunGlowPower = 8;

float2 CloudPlanePoint(float3 ray)
{
    return ray.xy / (ray.z + cCloudPlace.w) * cCloudPlace.z + cCloudPlace.xy;
}

float4 main(float2 pixel : VPOS) : COLOR0
{
    float3 ray = normalize(ViewToWorldDirection(ViewRayAtUnitDepth(pixel + 0.5)));
    float horizon = smoothstep(kHorizonStart, kHorizonFull, ray.z);
    clip(horizon - 0.001);

    float2 place = CloudPlanePoint(ray) + cCloudDrift.xy;
    float2 warp = tex2D(sCloudField, place * kWarpRepeats + cCloudDrift.zw).ba - 0.5;
    float2 shaped = place + warp * kWarpReach;

    float body = tex2D(sCloudField, shaped).r;
    float detail = tex2D(sCloudField, shaped * kDetailRepeats).g - 0.5;
    float fine = tex2D(sCloudField, shaped * kFineRepeats).g - 0.5;
    float density = body + detail * kDetailAmount + fine * kFineAmount;

    float threshold = cCloudShape.x;
    float alpha = smoothstep(threshold - cCloudShape.y, threshold + cCloudShape.y, density);
    float thick = saturate((density - threshold) / cCloudShape.z);

    float2 towardSun = cCloudToSun.xy / (abs(cCloudToSun.z) + cCloudPlace.w) * kSunReach;
    float sunSide = (tex2D(sCloudField, shaped + towardSun).r - body) * kSunSideShading;
    float shade = saturate(kShadeFloor + thick + sunSide + detail * kBillowShading * saturate(thick * 3));
    float3 colour = lerp(cCloudLit.rgb, cCloudDark.rgb, shade);

    float sunGlow = pow(saturate(dot(ray, cCloudToSun.xyz)), kSunGlowPower);
    colour += cCloudSunColour.rgb * sunGlow * (1 - thick) * cCloudSunColour.a;

    float flash = tex2D(sCloudField, place * kFlashRepeats + cCloudFlash.xy).r;
    colour += cCloudFlash.z * flash * flash * (0.35 + 0.65 * thick);

    return float4(saturate(colour), alpha * horizon * cCloudLit.a);
}
