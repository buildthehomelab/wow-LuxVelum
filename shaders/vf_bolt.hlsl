float4 cBoltColour : register(c0);

static const float kCoreWidth = 0.14;
static const float kCoreSoftness = 0.09;
static const float kGlowFalloff = 9;
static const float kGlowShare = 0.32;
static const float kHazeFalloff = 2.2;
static const float kHazeShare = 0.1;

float4 main(float2 acrossAndBrightness : TEXCOORD0) : COLOR0
{
    float across = abs(acrossAndBrightness.x);
    float core = 1 - smoothstep(kCoreWidth, kCoreWidth + kCoreSoftness, across);
    float edge = saturate(1 - across);
    float glow = exp(-across * across * kGlowFalloff) * kGlowShare + exp(-across * across * kHazeFalloff) * kHazeShare;
    float light = (core + glow) * edge * edge * acrossAndBrightness.y * cBoltColour.a;
    return float4(cBoltColour.rgb * light, 1);
}
