"""Generate deterministic fixtures and validate decoded FastCGI responses."""

import io
import json
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageStat


def generate(directory):
    directory.mkdir(parents=True, exist_ok=True)
    for name, width, height, color in (
        ("sample", 1024, 768, False),
        ("virtual", 2048, 1536, False),
        ("color", 512, 384, True),
    ):
        channels = 3 if color else 1
        pixels = bytearray(width * height * channels)
        for y in range(height):
            for x in range(width):
                offset = (y * width + x) * channels
                red = x * 255 // (width - 1)
                green = y * 255 // (height - 1)
                if color:
                    pixels[offset:offset + 3] = bytes((red, green, (red + green) // 2))
                else:
                    pixels[offset] = (red + green) // 2
        image = Image.frombytes("RGB" if color else "L", (width, height), bytes(pixels))
        image.save(directory / f"{name}.pnm")

    width, height = 512, 384
    with (directory / "gray16.pgm").open("wb") as output:
        output.write(f"P5\n{width} {height}\n65535\n".encode())
        for y in range(height):
            output.write(b"".join((x * 65535 // (width - 1)).to_bytes(2, "big") for x in range(width)))


def read_image(response):
    headers, body = response.read_bytes().split(b"\r\n\r\n", 1)
    assert b"content-type: image/jpeg" in headers.lower(), (response.name, headers)
    assert b"status: 4" not in headers.lower() and b"status: 5" not in headers.lower()
    image = Image.open(io.BytesIO(body))
    image.load()  # Decode all entropy-coded data, rather than checking only the JPEG header.
    assert image.format == "JPEG"
    return image


def validate(directory):
    cases = {
        "cvt": ((333, 250), "L"),
        "jtl": ((256, 256), "L"),
        "virtual": ((256, 192), "L"),
        "color": ((300, 225), "RGB"),
        "edge": ((256, 128), "RGB"),
        "gray16": ((200, 150), "L"),
        "contrast": ((300, 225), "RGB"),
        "threads1": ((128, 96), "RGB"),
        "threads0": ((256, 256), "L"),
    }
    result = {}
    for name, (size, mode) in cases.items():
        image = read_image(directory / f"{name}.response")
        assert image.size == size and image.mode == mode, (name, image.size, image.mode)
        result[name] = {"size": image.size, "mode": image.mode}

    # Independently compare RGB values with the source, not just channel count.
    actual = read_image(directory / "color.response")
    expected = Image.open(directory / "color.pnm").resize(actual.size, Image.Resampling.BILINEAR)
    error = ImageStat.Stat(ImageChops.difference(actual, expected)).mean
    assert max(error) < 3.0, error
    result["color"]["mean_absolute_error"] = error

    gray = read_image(directory / "gray16.response")
    assert gray.getpixel((10, 70)) < gray.getpixel((190, 70))
    assert gray.getextrema()[1] - gray.getextrema()[0] > 200

    for number in range(12):
        image = read_image(directory / f"concurrent-{number}.response")
        assert image.size == (160, 120), image.size
    result["concurrent_requests"] = 12

    failed = (directory / "missing.response").read_bytes()
    assert b"image/jpeg" not in failed.split(b"\r\n\r\n", 1)[0].lower()
    read_image(directory / "recovery.response")
    result["error_recovery"] = "passed"
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    operation, path = sys.argv[1:]
    {"generate": generate, "validate": validate}[operation](Path(path))
