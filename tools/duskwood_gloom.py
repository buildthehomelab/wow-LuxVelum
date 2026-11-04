import argparse
import struct
import sys

DESCRIPTION = "Turn Duskwood's cyan fog in a converted fogdata.bin into a dim, cold grey-green."
FORMAT_VERSION = 4
FILE_MAGIC = b"VFD1"
HEADER = "<4s8I"
LIGHT_SIZE = 60
LIGHT_PARAMS_OFFSET = 28
LIGHT_PARAMS_SLOTS = 8
PARAMS_SIZE = 16
KEY_SIZE = 16
LAYER_SIZE = 108
ZONE_LIGHT_SIZE = 28
ZONE_POINT_SIZE = 8
GRADING_CURVE_SIZE = 36
LAYER_DENSITY_OFFSET = 20
LAYER_INTENSITY_OFFSET = 44

DUSKWOOD_PARAMS = 7791
DUSKWOOD_LIGHTS = [2, 15, 18]
CONVERTED_GRADING_CURVES = 8
SCENE_LAYERS = (0, 2)
FAR_LAYER = 2
DESATURATION = 0.85
EMISSIVE_GAIN = 0.38
DIFFUSE_GAIN = 0.50
GLOOM_TINT = (0.85, 1.0, 0.95)
LUMA = (0.2126, 0.7152, 0.0722)
INTENSITY_GAIN = 0.35
MAX_INTENSITY = 1.0
MAX_PHASE_G = 0.75
FAR_DENSITY_GAIN = 1.25


def gloomy_rgb(rgb, gain):
    channels = [((rgb >> shift) & 0xFF) / 255.0 for shift in (16, 8, 0)]
    luminance = sum(channel * weight for channel, weight in zip(channels, LUMA))
    mixed = [(channel * (1.0 - DESATURATION) + luminance * tint * DESATURATION) * gain
             for channel, tint in zip(channels, GLOOM_TINT)]
    red, green, blue = (min(255, round(channel * 255.0)) for channel in mixed)
    return red << 16 | green << 8 | blue


def darken_layer(data, offset, layer_index):
    diffuse, emissive, shadow = struct.unpack_from("<3I", data, offset)
    struct.pack_into("<3I", data, offset, gloomy_rgb(diffuse, DIFFUSE_GAIN), gloomy_rgb(emissive, EMISSIVE_GAIN),
                     gloomy_rgb(shadow, EMISSIVE_GAIN))
    if layer_index == FAR_LAYER:
        density = struct.unpack_from("<f", data, offset + LAYER_DENSITY_OFFSET)[0]
        struct.pack_into("<f", data, offset + LAYER_DENSITY_OFFSET, density * FAR_DENSITY_GAIN)
    intensity, phase_g = struct.unpack_from("<2f", data, offset + LAYER_INTENSITY_OFFSET)
    struct.pack_into("<2f", data, offset + LAYER_INTENSITY_OFFSET, min(MAX_INTENSITY, intensity * INTENSITY_GAIN),
                     min(MAX_PHASE_G, phase_g))


def lights_using(data, first_light, light_count, params_id):
    users = []
    for index in range(light_count):
        offset = first_light + index * LIGHT_SIZE
        slots = struct.unpack_from("<%dI" % LIGHT_PARAMS_SLOTS, data, offset + LIGHT_PARAMS_OFFSET)
        if params_id in slots:
            users.append(struct.unpack_from("<I", data, offset)[0])
    return users


def darken_duskwood(source):
    data = bytearray(source)
    header = list(struct.unpack_from(HEADER, data, 0))
    magic, version, lights, params, keys, layers, zones, points, curves = header
    if magic != FILE_MAGIC or version != FORMAT_VERSION:
        sys.exit("expected fogdata format %d" % FORMAT_VERSION)
    if curves != CONVERTED_GRADING_CURVES:
        sys.exit("expected a freshly converted file with %d grading curves, found %d"
                 % (CONVERTED_GRADING_CURVES, curves))
    first_light = struct.calcsize(HEADER)
    first_params = first_light + lights * LIGHT_SIZE
    first_key = first_params + params * PARAMS_SIZE
    first_layer = first_key + keys * KEY_SIZE
    expected_size = (first_layer + layers * LAYER_SIZE + zones * ZONE_LIGHT_SIZE + points * ZONE_POINT_SIZE
                     + curves * GRADING_CURVE_SIZE)
    if len(data) != expected_size:
        sys.exit("expected %d bytes, found %d" % (expected_size, len(data)))
    users = lights_using(data, first_light, lights, DUSKWOOD_PARAMS)
    if users != DUSKWOOD_LIGHTS:
        sys.exit("light params %d are used by lights %s, not only Duskwood's" % (DUSKWOOD_PARAMS, users))
    for index in range(params):
        params_id, key_start, key_count = struct.unpack_from("<3I", data, first_params + index * PARAMS_SIZE)
        if params_id != DUSKWOOD_PARAMS:
            continue
        for key in range(key_start, key_start + key_count):
            _, layer_count, layer_start = struct.unpack_from("<HHI", data, first_key + key * KEY_SIZE)
            for layer_index in SCENE_LAYERS:
                if layer_index < layer_count:
                    darken_layer(data, first_layer + (layer_start + layer_index) * LAYER_SIZE, layer_index)
    header[-1] = curves + 1
    struct.pack_into(HEADER, data, 0, *header)
    return bytes(data) + bytes(GRADING_CURVE_SIZE)


def main():
    parser = argparse.ArgumentParser(description=DESCRIPTION)
    parser.add_argument("source")
    parser.add_argument("output")
    args = parser.parse_args()
    with open(args.source, "rb") as handle:
        source = handle.read()
    result = darken_duskwood(source)
    with open(args.output, "wb") as handle:
        handle.write(result)
    print("%s: %d bytes" % (args.output, len(result)))


if __name__ == "__main__":
    main()
