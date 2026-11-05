#include "vf_common.hlsli"

float4 cExtinction : register(c9);
float4 cScatter : register(c10);
float4 cLightInWater : register(c11);
float4 cSunLight : register(c12);
float4 cStrengths : register(c13);
float4 cCamera : register(c14);

sampler2D sDepth : register(s0);
sampler2D sScene : register(s1);
sampler2D sSceneBeforeSurface : register(s2);

static const float kCausticScale = 0.32;
static const float kSecondCausticScale = 0.55;
static const float kShaftScale = 0.12;
static const float kHashTile = 64;
static const float kCausticFadeDistance = 90;
static const float kShaftReach = 70;
static const int kShaftSteps = 10;
static const float kReflectedBelowShare = 0.75;
static const float kRefractionShift = 0.04;
static const float kWobbleShift = 0.0035;
static const float kSchlickFloor = 0.02;
static const float kTwoPi = 6.2831853;
static const float kSelfLitGlow = 0.6;
static const float kShaftWaterTint = 0.6;
static const float kShallowestLight = 0.2;

float3 Extinction()
{
    return cExtinction.rgb;
}

float SurfaceHeight()
{
    return cExtinction.w;
}

float3 ScatterColour()
{
    return cScatter.rgb;
}

float DepthDarkening()
{
    return cScatter.w;
}

float3 LightInWater()
{
    return cLightInWater.xyz;
}

float Seconds()
{
    return cLightInWater.w;
}

float3 SunLight()
{
    return cSunLight.rgb;
}

float CausticStrength()
{
    return cSunLight.w;
}

float ShaftStrength()
{
    return cStrengths.x;
}

float SurfaceStrength()
{
    return cStrengths.y;
}

float WobbleStrength()
{
    return cStrengths.z;
}

float SelfLit()
{
    return cStrengths.w;
}

float CameraDepth()
{
    return cCamera.x;
}

bool HasSceneBeforeSurface()
{
    return cCamera.y > 0.5;
}

float RefractiveIndex()
{
    return cCamera.z;
}

float2 WrappedCell(float2 cell)
{
    return cell - floor(cell / kHashTile) * kHashTile;
}

float2 CellHash(float2 cell)
{
    float2 wrapped = WrappedCell(cell);
    float2 q = float2(dot(wrapped, float2(12.9898, 78.233)), dot(wrapped, float2(39.3468, 11.1353)));
    return frac(sin(q) * 43758.5453);
}

float CellEdgeDistance(float2 p, float seconds)
{
    float2 cell = floor(p);
    float2 inCell = p - cell;
    float nearest = 8;
    float second = 8;
    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            float2 offset = float2(x, y);
            float2 h = CellHash(cell + offset);
            float2 site = offset + 0.5 + 0.38 * sin(seconds * (0.6 + 0.5 * h) + kTwoPi * h);
            float d = length(site - inCell);
            second = min(second, max(nearest, d));
            nearest = min(nearest, d);
        }
    }
    return second - nearest;
}

float Caustics(float2 xy, float seconds)
{
    float first = 1 - smoothstep(0.0, 0.16, CellEdgeDistance(xy * kCausticScale, seconds));
    float second = 1 - smoothstep(0.0, 0.2, CellEdgeDistance(xy * kSecondCausticScale + 17.3, seconds * 1.3));
    return saturate(first * second * 1.6 + 0.3 * (first + second));
}

float ShaftPattern(float2 xy, float seconds)
{
    return 1 - smoothstep(0.0, 0.35, CellEdgeDistance(xy * kShaftScale, seconds * 0.7));
}

float2 WaveSlope(float2 xy, float2 direction, float frequency, float speed, float amplitude, float seconds)
{
    float2 k = direction * frequency;
    return k * amplitude * cos(dot(k, xy) - speed * seconds);
}

float3 SurfaceNormal(float2 xy, float seconds)
{
    float2 slope = WaveSlope(xy, float2(0.8, 0.6), 0.9, 1.3, 0.06, seconds);
    slope += WaveSlope(xy, float2(-0.5, 0.86), 1.7, 1.9, 0.035, seconds);
    slope += WaveSlope(xy, float2(0.97, -0.24), 3.1, 2.7, 0.018, seconds);
    slope += WaveSlope(xy, float2(-0.3, -0.95), 5.3, 3.6, 0.009, seconds);
    return normalize(float3(-slope, 1));
}

float2 Wobble(float2 uv, float seconds)
{
    float2 shift = float2(sin(uv.y * 55 + seconds * 2.1) + 0.5 * sin(uv.x * 31 - seconds * 1.4),
                          cos(uv.x * 47 + seconds * 1.8) + 0.5 * cos(uv.y * 29 + seconds * 1.2));
    return shift * kWobbleShift * WobbleStrength();
}

float TransmittedFromBelow(float3 rayUp, float3 normal)
{
    float cosineIn = saturate(dot(rayUp, normal));
    float sineOutSquared = RefractiveIndex() * RefractiveIndex() * (1 - cosineIn * cosineIn);
    if (sineOutSquared >= 1)
        return 0;
    float cosineOut = sqrt(1 - sineOutSquared);
    float reflected = kSchlickFloor + (1 - kSchlickFloor) * pow(1 - cosineOut, 5);
    return 1 - reflected;
}

float3 SurfaceFromBelow(float2 uv, float3 ray, float3 hit, float seconds)
{
    float3 normal = SurfaceNormal(hit.xy, seconds);
    float transmitted = TransmittedFromBelow(ray, normal);
    float2 refractedUv = uv + normal.xy * kRefractionShift;
    float3 above = HasSceneBeforeSurface() ? tex2Dlod(sSceneBeforeSurface, float4(refractedUv, 0, 0)).rgb
                                           : tex2Dlod(sScene, float4(refractedUv, 0, 0)).rgb;
    float3 reflected = ScatterColour() * kReflectedBelowShare;
    return lerp(reflected, above, transmitted);
}

float3 LitByCaustics(float3 scene, float3 position, float3 normal, float sceneDistance, float seconds)
{
    float depthBelow = SurfaceHeight() - position.z;
    if (depthBelow <= 0)
        return scene;
    float3 light = LightInWater();
    float2 atSurface = position.xy + light.xy / max(light.z, kShallowestLight) * depthBelow;
    float facing = saturate(dot(normal, light));
    float3 reaching = exp(-Extinction() * depthBelow / max(light.z, kShallowestLight)) * SunLight();
    float fade = saturate(1 - sceneDistance / kCausticFadeDistance);
    return scene * (1 + Caustics(atSurface, seconds) * facing * fade * CausticStrength() * reaching);
}

float3 DarkenedByDepth(float3 scene, float3 position)
{
    float deeper = max(0, SurfaceHeight() - position.z - CameraDepth());
    return scene * exp(-Extinction() * deeper * DepthDarkening());
}

float3 WaterTint()
{
    float3 scatter = ScatterColour();
    float brightest = max(max(scatter.r, scatter.g), max(scatter.b, 1e-3));
    return lerp(1, scatter / brightest, kShaftWaterTint);
}

float3 Glow()
{
    return ScatterColour() * (1 + kSelfLitGlow * SelfLit());
}

float3 LightShafts(float3 camera, float3 ray, float reach, float jitter, float seconds)
{
    float3 light = LightInWater();
    float stepLength = min(reach, kShaftReach) / kShaftSteps;
    float3 gathered = 0;
    [loop] for (int i = 0; i < kShaftSteps; ++i)
    {
        float travelled = (i + jitter) * stepLength;
        float3 probe = camera + ray * travelled;
        float depthBelow = max(0, SurfaceHeight() - probe.z);
        float lightPath = depthBelow / max(light.z, kShallowestLight);
        float2 atSurface = probe.xy + light.xy / max(light.z, kShallowestLight) * depthBelow;
        gathered += ShaftPattern(atSurface, seconds) * exp(-Extinction() * (lightPath + travelled));
    }
    float forward = 0.35 + 0.65 * pow(saturate(dot(ray, light)), 4);
    return gathered * stepLength * Extinction() * forward * SunLight() * WaterTint() * ShaftStrength();
}

float4 main(float2 pixelIndex : VPOS) : COLOR0
{
    float2 pixel = pixelIndex + 0.5;
    float2 uv = (pixel - ViewportOrigin()) / ViewportSize();
    float seconds = Seconds();
    float rawDepth = SampleDepth(sDepth, pixel);
    float3 viewRay = ViewRayAtUnitDepth(pixel);
    float3 ray = normalize(ViewToWorldDirection(viewRay));
    float sceneDistance = LinearDepth(rawDepth) * length(viewRay);
    float3 camera = CameraPositionWorld();
    float3 position = camera + ray * sceneDistance;
    float3 across = cross(ddx(position), ddy(position));
    float3 normal = dot(across, across) > 1e-12 ? normalize(across) : -ray;
    normal = dot(normal, ray) > 0 ? -normal : normal;

    float toSurface = ray.z > 1e-4 ? (SurfaceHeight() - camera.z) / ray.z : sceneDistance + 1;
    bool throughSurface = toSurface > 0 && toSurface < sceneDistance;
    float inWater = throughSurface ? toSurface : sceneDistance;

    float3 scene = tex2Dlod(sScene, float4(uv + Wobble(uv, seconds), 0, 0)).rgb;
    [branch] if (throughSurface)
    {
        [branch] if (SurfaceStrength() > 0)
        {
            float3 below = SurfaceFromBelow(uv, ray, camera + ray * toSurface, seconds);
            scene = lerp(scene, below, SurfaceStrength());
        }
    }
    else if (!IsSky(rawDepth))
    {
        scene = DarkenedByDepth(scene, position);
        [branch] if (CausticStrength() > 0 && any(SunLight() > 0))
            scene = LitByCaustics(scene, position, normal, sceneDistance, seconds);
    }

    float3 transmittance = exp(-Extinction() * inWater);
    float3 colour = scene * transmittance + Glow() * (1 - transmittance);
    [branch] if (ShaftStrength() > 0 && any(SunLight() > 0))
        colour += LightShafts(camera, ray, inWater, InterleavedGradientNoise(pixel + FrameIndex() * 5.588), seconds);
    return float4(colour, 1);
}
