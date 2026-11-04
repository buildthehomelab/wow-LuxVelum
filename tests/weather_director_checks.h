#pragma once

#include "water_rain.h"
#include "weather_director.h"

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <limits>

namespace weather_director_checks
{
constexpr int32_t kUnknownWeather = 4;
constexpr float kAllowedError = 1e-4f;
constexpr double kStart = 100.0;

bool Near(float a, float b)
{
    return std::fabs(a - b) < kAllowedError;
}

WeatherTuning TestTuning()
{
    WeatherTuning tuning;
    tuning.profiles[kWeatherRain].sun = 0.4f;
    tuning.profiles[kWeatherRain].rays = 0.0f;
    tuning.profiles[kWeatherRain].density = 1.5f;
    tuning.profiles[kWeatherSnow].exposure = 1.2f;
    tuning.profiles[kWeatherSand].tint = 0.6f;
    tuning.easeSeconds = 30.0f;
    tuning.wetSeconds = 100.0f;
    tuning.dryingSeconds = 200.0f;
    tuning.afterRainMist = 0.5f;
    tuning.lowSunRays = 0.6f;
    tuning.clearingRays = 1.0f;
    return tuning;
}

void CheckClientFields()
{
    Check(LiveClientWeatherType(kClientWeatherNoRequest, true, false, false) == kClientWeatherRain,
          "steady rain has no pending request, so its type comes from the live effect");
    Check(LiveClientWeatherType(kClientWeatherNoRequest, false, true, false) == kClientWeatherSnow &&
              LiveClientWeatherType(kClientWeatherNoRequest, false, false, true) == kClientWeatherSand,
          "the same goes for snow and sand");
    Check(LiveClientWeatherType(kClientWeatherSnow, true, false, false) == kClientWeatherSnow,
          "a request for new weather wins while the old effect drains");
    Check(LiveClientWeatherType(kClientWeatherNone, true, false, false) == kClientWeatherRain,
          "a clear or fog request keeps the live effect until it has drained");
    Check(LiveClientWeatherType(kClientWeatherNoRequest, false, false, false) == kClientWeatherNone,
          "no request and no effect is type 0");
    Check(ClientWeatherIsAsh(kClientWeatherSnow, 1.0f, "textures\\weather\\ASH01.blp"),
          "a snow row with an ash texture is ash fall");
    Check(!ClientWeatherIsAsh(kClientWeatherSnow, 1.0f, "textures\\Weather\\SnowFlake01.blp") &&
              !ClientWeatherIsAsh(kClientWeatherSnow, 1.0f, ""),
          "ordinary snow isn't");
    Check(ClientWeatherIsAsh(kClientWeatherSand, 0.0f, "") && !ClientWeatherIsAsh(kClientWeatherSand, 1.0f, ""),
          "black snow has a sand type and no transition; a sandstorm has one");
    Check(!ClientWeatherIsAsh(kClientWeatherRain, 0.0f, "ash"), "rain is never ash");
}

void CheckKinds()
{
    Check(WeatherKindOf(kClientWeatherRain, 0.3f, false) == kWeatherRain &&
              WeatherKindOf(kClientWeatherSnow, 0.9f, false) == kWeatherSnow &&
              WeatherKindOf(kClientWeatherSand, 0.6f, false) == kWeatherSand,
          "the client's weather types name rain, snow and sand");
    Check(WeatherKindOf(kClientWeatherSnow, 0.9f, true) == kWeatherAsh &&
              WeatherKindOf(kClientWeatherSand, 0.9f, true) == kWeatherAsh,
          "ash gets its own look");
    Check(WeatherKindOf(kClientWeatherNone, 0.6f, false) == kWeatherFog,
          "fog has no particle type in the client's table, so type 0 with a real grade is fog");
    Check(WeatherKindOf(kClientWeatherNone, 0.2f, false) == kNoWeatherKind &&
              WeatherKindOf(kClientWeatherNone, 0.0f, false) == kNoWeatherKind,
          "clear weather stays clear, even when the server sends it a low grade");
    Check(WeatherKindOf(kUnknownWeather, 0.5f, false) == kNoWeatherKind &&
              WeatherKindOf(kClientWeatherRain, std::numeric_limits<float>::quiet_NaN(), false) == kNoWeatherKind &&
              WeatherKindOf(kClientWeatherRain, 0.0f, false) == kNoWeatherKind,
          "an unknown type or a bad intensity counts as clear");
}

void CheckClearIsNeutral()
{
    WeatherDirector director;
    WeatherTuning tuning = TestTuning();
    tuning.lowSunRays = 0.0f;
    tuning.clearingRays = 0.0f;
    const WeatherLook look = director.Step({}, tuning, kStart);
    Check(look.stormBlend == 0.0f && look.sun == 1.0f && look.exposure == 1.0f && look.density == 1.0f &&
              look.tint == 0.0f && look.waves == 1.0f && WeatherGodRayStrength(0.0f, look, 0.1f) == 0.0f,
          "clear weather leaves every setting alone");
    Check(WeatherGodRayStrength(0.8f, look, 0.9f) == 0.8f, "clear weather keeps the player's own god rays");
}

void CheckEase()
{
    WeatherDirector director;
    const WeatherTuning tuning = TestTuning();
    WeatherReading rain;
    rain.type = kClientWeatherRain;
    rain.intensity = 0.8f;
    WeatherLook look = director.Step(rain, tuning, kStart);
    Check(Near(look.stormBlend, 0.8f) && Near(look.sun, 1.0f + 0.8f * (0.4f - 1.0f)),
          "the first reading applies at once, so a login during rain isn't clear");
    look = director.Step({}, tuning, kStart + tuning.easeSeconds / 2.0);
    Check(Near(look.stormBlend, 0.3f), "weather eases away, half the time taking half the range");
    look = director.Step({}, tuning, kStart + tuning.easeSeconds * 2.0);
    Check(Near(look.stormBlend, 0.0f), "the ease stops at its target");
    look = director.Step(rain, tuning, kStart);
    Check(Near(look.stormBlend, 0.0f), "a clock that runs backwards doesn't move the blend");
}

void CheckKindsCrossFade()
{
    WeatherDirector director;
    const WeatherTuning tuning = TestTuning();
    WeatherReading rain;
    rain.type = kClientWeatherRain;
    rain.intensity = 1.0f;
    director.Step(rain, tuning, kStart);
    WeatherReading snow;
    snow.type = kClientWeatherSnow;
    snow.intensity = 1.0f;
    director.Step(snow, tuning, kStart + tuning.easeSeconds / 2.0);
    Check(Near(director.KindBlend(kWeatherRain), 0.5f) && Near(director.KindBlend(kWeatherSnow), 0.5f),
          "rain turning to snow fades one out while the other comes in");
}

void CheckStrength()
{
    WeatherDirector director;
    WeatherTuning tuning = TestTuning();
    tuning.strength = 0.5f;
    WeatherReading rain;
    rain.type = kClientWeatherRain;
    rain.intensity = 1.0f;
    const WeatherLook look = director.Step(rain, tuning, kStart);
    Check(Near(look.sun, 0.7f) && Near(look.stormBlend, 1.0f),
          "the strength scales the look but not the area's own storm fog");
    tuning.strength = 2.0f;
    tuning.profiles[kWeatherRain].sun = 0.2f;
    WeatherDirector strong;
    Check(strong.Step(rain, tuning, kStart).sun == 0.0f, "a strong setting can't push a value below zero");
}

void CheckAfterRain()
{
    WeatherDirector director;
    const WeatherTuning tuning = TestTuning();
    WeatherReading rain;
    rain.type = kClientWeatherRain;
    rain.intensity = 1.0f;
    double now = kStart;
    director.Step(rain, tuning, now);
    now += tuning.wetSeconds;
    WeatherLook look = director.Step(rain, tuning, now);
    Check(Near(look.wetness, 1.0f) && Near(look.clearingRays, 0.0f),
          "the ground gets wet while it rains, with no clearing rays under the rain");
    now += tuning.easeSeconds;
    look = director.Step({}, tuning, now);
    const float wetness = 1.0f - tuning.easeSeconds / tuning.dryingSeconds;
    Check(Near(look.stormBlend, 0.0f) && Near(look.wetness, wetness) && Near(look.density, 1.0f + wetness * 0.5f) &&
              Near(look.clearingRays, wetness * wetness),
          "when the rain ends, mist and rays through the clearing follow how wet it still is");
    now += tuning.dryingSeconds;
    look = director.Step({}, tuning, now);
    Check(Near(look.wetness, 0.0f) && Near(look.density, 1.0f) && Near(look.clearingRays, 0.0f),
          "once dry, the after-rain look has gone");
}

void CheckWetGroundStaysPut()
{
    const WeatherTuning tuning = TestTuning();
    WeatherReading rain;
    rain.type = kClientWeatherRain;
    rain.intensity = 1.0f;
    rain.position[0] = 100.0f;
    WeatherDirector director;
    double now = kStart;
    director.Step(rain, tuning, now);
    now += tuning.wetSeconds;
    director.Step(rain, tuning, now);
    WeatherReading clear = rain;
    clear.type = kClientWeatherNone;
    clear.intensity = 0.0f;
    now += tuning.easeSeconds;
    Check(director.Step(clear, tuning, now).wetness > 0.5f, "it's still wet where it rained");
    clear.position[0] += 600.0f;
    now += 1.0;
    Check(director.Step(clear, tuning, now).wetness == 0.0f, "flying far away leaves the wet ground behind");
    clear.position[0] = rain.position[0];
    now += 1.0;
    Check(director.Step(clear, tuning, now).wetness > 0.5f, "coming back finds it still wet");
    clear.mapId = 1;
    now += 1.0;
    Check(director.Step(clear, tuning, now).wetness == 0.0f, "another map isn't wet");
    clear.mapId = 0;
    now += 1.0;
    Check(director.Step(clear, tuning, now).wetness == 0.0f, "and changing map dries the place for good");
}

void CheckNewRainElsewhere()
{
    const WeatherTuning tuning = TestTuning();
    WeatherReading rain;
    rain.type = kClientWeatherRain;
    rain.intensity = 1.0f;
    WeatherDirector director;
    double now = kStart;
    director.Step(rain, tuning, now);
    now += tuning.wetSeconds;
    director.Step(rain, tuning, now);
    WeatherReading clear = rain;
    clear.type = kClientWeatherNone;
    clear.intensity = 0.0f;
    now += tuning.easeSeconds;
    director.Step(clear, tuning, now);
    WeatherReading farRain = rain;
    farRain.position[0] = 800.0f;
    farRain.intensity = 0.3f;
    now += 1.0 / 30.0;
    const WeatherLook arriving = director.Step(farRain, tuning, now);
    Check(arriving.wetness < 0.01f && arriving.density < 1.01f,
          "new rain far away starts dry, instead of bringing the old place's wet ground");
}

void CheckMapChangeAppliesAtOnce()
{
    const WeatherTuning tuning = TestTuning();
    WeatherReading rain;
    rain.type = kClientWeatherRain;
    rain.intensity = 1.0f;
    WeatherDirector director;
    director.Step(rain, tuning, kStart);
    WeatherReading elsewhere;
    elsewhere.mapId = 1;
    Check(director.Step(elsewhere, tuning, kStart + 1.0).stormBlend == 0.0f,
          "a new map shows its own weather straight away");
}

void CheckTintColour()
{
    WeatherDirector director;
    WeatherTuning tuning = TestTuning();
    tuning.tintColors[kWeatherSand][0] = 0.8f;
    tuning.tintColors[kWeatherSand][2] = 0.4f;
    WeatherReading sand;
    sand.type = kClientWeatherSand;
    sand.intensity = 1.0f;
    const WeatherLook look = director.Step(sand, tuning, kStart);
    Check(Near(look.tint, 0.6f) && Near(look.tintColor[0], 0.8f) && Near(look.tintColor[2], 0.4f),
          "a sandstorm tints the fog its own colour");
    const WeatherLook fading = director.Step({}, tuning, kStart + 1.0);
    Check(Near(fading.tintColor[0], 0.8f), "the colour holds while the sand fades, rather than turning white");
}

void CheckGodRays()
{
    WeatherLook look;
    look.lowSunRays = 0.6f;
    Check(Near(WeatherGodRayStrength(0.0f, look, 0.1f), 0.6f), "a low sun in clear weather brings rays");
    Check(Near(WeatherGodRayStrength(0.0f, look, 0.9f), 0.0f), "a high sun doesn't");
    look.raysGain = 0.0f;
    look.lowSunRays = 0.0f;
    Check(WeatherGodRayStrength(1.0f, look, 0.1f) == 0.0f, "rain can take away even the player's own god rays");
    look.clearingRays = 0.5f;
    Check(Near(WeatherGodRayStrength(0.0f, look, 0.9f), 0.5f), "clearing rays come at any sun height");
}

void CheckIndoors()
{
    WeatherLook look;
    look.sun = 0.4f;
    look.exposure = 0.8f;
    look.tint = 0.5f;
    look.lowSunRays = 1.0f;
    look.stormBlend = 0.7f;
    const WeatherLook indoors = NeutralWeatherLook(look, 0.0f);
    Check(indoors.sun == 1.0f && indoors.exposure == 1.0f && indoors.tint == 0.0f && indoors.lowSunRays == 0.0f &&
              indoors.stormBlend == 0.7f,
          "indoors the look goes neutral; the interior fade already handles the storm layers");
    const WeatherLook outdoors = NeutralWeatherLook(look, 1.0f);
    Check(Near(outdoors.sun, look.sun) && Near(outdoors.exposure, look.exposure) && Near(outdoors.tint, look.tint),
          "outdoors it's untouched");
}

void CheckRainDrops()
{
    WeatherDirector director;
    WeatherTuning tuning = TestTuning();
    tuning.rainRings = 1.0f;
    WeatherReading rain;
    rain.type = kClientWeatherRain;
    rain.intensity = 1.0f;
    const float heavy = director.Step(rain, tuning, kStart).rainDropsPerSecond;
    Check(heavy > 0.0f, "rain makes drops");
    WeatherDirector light;
    rain.intensity = 0.5f;
    Check(Near(light.Step(rain, tuning, kStart).rainDropsPerSecond, heavy * 0.5f), "lighter rain makes fewer");
    WeatherDirector snow;
    WeatherReading snowing;
    snowing.type = kClientWeatherSnow;
    snowing.intensity = 1.0f;
    Check(snow.Step(snowing, tuning, kStart).rainDropsPerSecond == 0.0f, "snow makes no rings");
}

void CheckLightning()
{
    Check(WeatherLightningEnvelope(0.0) == 0.0f && WeatherLightningEnvelope(0.03) > 0.5f &&
              WeatherLightningEnvelope(0.1) < WeatherLightningEnvelope(0.03),
          "a strike rises quickly, not instantly, and falls");
    Check(WeatherLightningEnvelope(0.21) > WeatherLightningEnvelope(0.17), "a strike has a second stroke");
    Check(WeatherLightningEnvelope(0.181) < WeatherLightningEnvelope(0.21), "the second stroke rises too");
    Check(WeatherLightningEnvelope(-0.1) == 0.0f && WeatherLightningEnvelope(2.0) == 0.0f, "a flash ends");

    WeatherTuning tuning = TestTuning();
    tuning.lightning = 1.0f;
    WeatherDirector storm;
    WeatherReading heavy;
    heavy.type = kClientWeatherRain;
    heavy.intensity = 1.0f;
    int strikes = 0;
    bool lit = false;
    constexpr double kFrame = 1.0 / 30.0;
    constexpr int kFrames = 30 * 300;
    for (int frame = 0; frame < kFrames; ++frame)
    {
        const bool flashing = storm.Step(heavy, tuning, kStart + frame * kFrame).flash > 0.3f;
        if (flashing && !lit)
            ++strikes;
        lit = flashing;
    }
    std::printf("     lightning: %d strikes in 5 minutes of storm\n", strikes);
    Check(strikes >= 8 && strikes <= 40, "without an overcast, heavy rain strikes every 8 to 35 seconds");

    WeatherDirector drizzle;
    heavy.intensity = 0.4f;
    bool anyFlash = false;
    for (int frame = 0; frame < kFrames; ++frame)
        anyFlash = anyFlash || drizzle.Step(heavy, tuning, kStart + frame * kFrame).flash > 0.0f;
    Check(!anyFlash, "light rain has no lightning");

    const auto strikesIn = [&](WeatherReading reading) {
        WeatherDirector director;
        int seen = 0;
        bool wasLit = false;
        for (int frame = 0; frame < kFrames; ++frame)
        {
            const bool flashing = director.Step(reading, tuning, kStart + frame * kFrame).flash > 0.3f;
            if (flashing && !wasLit)
                ++seen;
            wasLit = flashing;
        }
        return seen;
    };
    WeatherReading thunderOverSand;
    thunderOverSand.type = kClientWeatherSand;
    thunderOverSand.intensity = 0.5f;
    thunderOverSand.overcastWeight = 0.8f;
    thunderOverSand.thunderOvercast = 0.8f;
    Check(strikesIn(thunderOverSand) >= 8, "the thunder palette strikes whatever is falling, as in Tanaris");
    WeatherReading thunderOnly;
    thunderOnly.overcastWeight = 0.8f;
    thunderOnly.thunderOvercast = 0.8f;
    Check(strikesIn(thunderOnly) >= 8, "an overcast shown with no weather packet still strikes");
    WeatherReading heavyRainOvercast;
    heavyRainOvercast.type = kClientWeatherRain;
    heavyRainOvercast.intensity = 1.0f;
    heavyRainOvercast.overcastWeight = 0.8f;
    Check(strikesIn(heavyRainOvercast) == 0, "heavy rain under the rain palette has no lightning");
    WeatherReading faintThunder = thunderOnly;
    faintThunder.overcastWeight = 0.2f;
    faintThunder.thunderOvercast = 0.2f;
    Check(strikesIn(faintThunder) == 0, "a faint storm palette has no lightning");
    WeatherReading ash = thunderOnly;
    ash.ash = true;
    Check(strikesIn(ash) == 0, "falling ash shares the storm palette but has no lightning");

    WeatherDirector afterAsh;
    WeatherReading ashEnding = ash;
    double clock = kStart;
    for (int frame = 0; frame < 300; ++frame, clock += kFrame)
        afterAsh.Step(ashEnding, tuning, clock);
    ashEnding.ash = false;
    bool flashedAfterAsh = false;
    for (int frame = 0; frame < 300; ++frame, clock += kFrame)
        flashedAfterAsh = flashedAfterAsh || afterAsh.Step(ashEnding, tuning, clock).flash > 0.0f;
    Check(!flashedAfterAsh, "the storm palette fading out after ash has no lightning");

    WeatherDirector flapping;
    double lastStrike = -1000.0;
    double shortestGap = 1000.0;
    float seedBefore = 0.0f;
    for (int frame = 0; frame < kFrames; ++frame)
    {
        WeatherReading flapped = thunderOnly;
        flapped.thunderOvercast = (frame / 15) % 2 ? 0.41f : 0.39f;
        const double now = kStart + frame * kFrame;
        const float seedNow = flapping.Step(flapped, tuning, now).strikeSeed;
        if (seedNow != seedBefore)
        {
            shortestGap = std::min(shortestGap, now - lastStrike);
            lastStrike = now;
        }
        seedBefore = seedNow;
    }
    Check(shortestGap >= 7.9, "a storm at the edge of the threshold keeps strikes at least 8 seconds apart");

    WeatherDirector seeded;
    float seedDuringFlash = -1.0f;
    bool seedHeld = true;
    float firstSeed = -1.0f;
    bool seedMoved = false;
    for (int frame = 0; frame < kFrames; ++frame)
    {
        const WeatherLook look = seeded.Step(thunderOnly, tuning, kStart + frame * kFrame);
        if (look.flash > 0.0f)
        {
            seedHeld = seedHeld && (seedDuringFlash < 0.0f || look.strikeSeed == seedDuringFlash);
            seedDuringFlash = look.strikeSeed;
            firstSeed = firstSeed < 0.0f ? look.strikeSeed : firstSeed;
            seedMoved = seedMoved || look.strikeSeed != firstSeed;
        }
        else
            seedDuringFlash = -1.0f;
    }
    Check(seedHeld, "a strike's place holds while it flashes");
    Check(seedMoved, "each strike lights another place");

    WeatherDirector asked;
    WeatherReading clearSky;
    asked.Step(clearSky, tuning, kStart);
    WeatherReading button = clearSky;
    button.strikeRequested = true;
    asked.Step(button, tuning, kStart + kFrame);
    const WeatherLook struck = asked.Step(clearSky, tuning, kStart + 2 * kFrame);
    Check(struck.flash > 0.5f && struck.strikeAsked, "a strike asked for fires at once, even under a clear sky");

    WeatherDirector stormy;
    bool naturalStrikeTooSoon = false;
    for (int press = 0; press < 40; ++press)
    {
        double now = kStart + 1000.0 * press;
        for (int frame = 0; frame < 30 * (20 + press); ++frame, now += kFrame)
            stormy.Step(thunderOnly, tuning, now);
        WeatherReading pressed = thunderOnly;
        pressed.strikeRequested = true;
        const float pressedSeed = stormy.Step(pressed, tuning, now).strikeSeed;
        for (int frame = 1; frame < 30 * 8 - 1; ++frame)
        {
            const WeatherLook later = stormy.Step(thunderOnly, tuning, now + frame * kFrame);
            naturalStrikeTooSoon = naturalStrikeTooSoon || later.strikeSeed != pressedSeed;
        }
    }
    Check(!naturalStrikeTooSoon, "a storm's own strikes wait 8 seconds after a strike asked for");

    WeatherDirector rumbling;
    int thunders = 0;
    int thunderStrikes = 0;
    float thunderSeed = 0.0f;
    double strikeTime = 0.0;
    double longestDelay = 0.0;
    bool clipsInRange = true;
    for (int frame = 0; frame < kFrames; ++frame)
    {
        const double now = kStart + frame * kFrame;
        const WeatherLook look = rumbling.Step(thunderOnly, tuning, now);
        if (look.strikeSeed != thunderSeed)
        {
            thunderSeed = look.strikeSeed;
            strikeTime = now;
            ++thunderStrikes;
        }
        if (look.thunderClip != kNoThunderClip)
        {
            ++thunders;
            longestDelay = std::max(longestDelay, now - strikeTime);
            clipsInRange = clipsInRange && look.thunderClip >= 0 && look.thunderClip < kThunderClips;
        }
    }
    Check(thunders >= thunderStrikes - 1 && thunders <= thunderStrikes, "every strike is followed by one thunder");
    Check(longestDelay <= 6.1, "thunder follows within 6 seconds");
    Check(clipsInRange, "thunder names a clip that exists");

    WeatherDirector askedThunder;
    WeatherReading quiet;
    askedThunder.Step(button, tuning, kStart);
    double askedHeardAfter = -1.0;
    for (int frame = 1; frame < 60 && askedHeardAfter < 0.0; ++frame)
        if (askedThunder.Step(quiet, tuning, kStart + frame * kFrame).thunderClip != kNoThunderClip)
            askedHeardAfter = frame * kFrame;
    Check(askedHeardAfter >= 0.1 && askedHeardAfter <= 0.9,
          "a strike asked for is close, so it's heard almost at once");

    tuning.lightning = 0.0f;
    WeatherDirector askedWhenOff;
    askedWhenOff.Step(button, tuning, kStart);
    Check(askedWhenOff.Step(clearSky, tuning, kStart + kFrame).flash == 0.0f, "lightning at 0 ignores the button too");
    WeatherDirector off;
    heavy.intensity = 1.0f;
    anyFlash = false;
    for (int frame = 0; frame < kFrames; ++frame)
        anyFlash = anyFlash || off.Step(heavy, tuning, kStart + frame * kFrame).flash > 0.0f;
    Check(!anyFlash, "lightning can be turned off");
}

void CheckDawnMist()
{
    Check(WeatherDawnWeight(6.5f / 24.0f) > 0.99f, "dawn mist peaks soon after sunrise");
    Check(WeatherDawnWeight(0.5f) == 0.0f && WeatherDawnWeight(0.0f) == 0.0f,
          "there's no dawn mist at noon or midnight");
    WeatherDirector director;
    WeatherTuning tuning = TestTuning();
    tuning.dawnMist = 0.4f;
    WeatherReading dawn;
    dawn.dayFraction = 6.5f / 24.0f;
    Check(Near(director.Step(dawn, tuning, kStart).density, 1.4f), "clear dawns get mist");
    WeatherDirector rainy;
    dawn.type = kClientWeatherRain;
    dawn.intensity = 1.0f;
    Check(Near(rainy.Step(dawn, tuning, kStart).density, tuning.profiles[kWeatherRain].density),
          "weather replaces the dawn mist");
}

void CheckRainRings()
{
    WaterRainDrops drops;
    const float centre[2] = {100.0f, 200.0f};
    WaterRippleDisturbance out[kMaxWaterRainDropsPerStep];
    Check(drops.Take(0.0f, centre, 20.0f, out, kMaxWaterRainDropsPerStep) == 0, "no rain, no rings");
    uint32_t total = 0;
    bool inside = true;
    for (int step = 0; step < 300; ++step)
    {
        const uint32_t count = drops.Take(3.0f, centre, 20.0f, out, kMaxWaterRainDropsPerStep);
        total += count;
        for (uint32_t i = 0; i < count; ++i)
            inside = inside && std::fabs(out[i].from[0] - centre[0]) <= 20.0f &&
                     std::fabs(out[i].to[1] - centre[1]) <= 20.0f && out[i].amplitude < 0.0f;
    }
    Check(total == 900 && inside, "drops fall at the asked rate within the window around the camera");
    uint32_t sparse = 0;
    for (int step = 0; step < 100; ++step)
        sparse += drops.Take(0.25f, centre, 20.0f, out, kMaxWaterRainDropsPerStep);
    Check(sparse == 25, "light rain builds up drops across steps");
    Check(drops.Take(50.0f, centre, 20.0f, out, 2) == 2, "drops never exceed the free disturbances");
}

void Run()
{
    CheckClientFields();
    CheckKinds();
    CheckClearIsNeutral();
    CheckEase();
    CheckKindsCrossFade();
    CheckStrength();
    CheckAfterRain();
    CheckWetGroundStaysPut();
    CheckNewRainElsewhere();
    CheckMapChangeAppliesAtOnce();
    CheckTintColour();
    CheckGodRays();
    CheckIndoors();
    CheckRainDrops();
    CheckLightning();
    CheckDawnMist();
    CheckRainRings();
}
}
