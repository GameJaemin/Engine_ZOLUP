"""Generate constant emissive textures in sRGB from linear warm/cool lamp colors."""
from pathlib import Path
import json
import math
import struct
import zlib


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def srgb(linear):
    encoded = linear * 12.92 if linear <= 0.0031308 else 1.055 * math.pow(linear, 1 / 2.4) - 0.055
    return round(encoded * 255)


def main():
    folder = Path(__file__).resolve().parents[2] / "Game/Content/BabyDoll/Materials"
    folder.mkdir(parents=True, exist_ok=True)
    base = {"baseColor": [0.07, 0.075, 0.08], "metallic": 0, "roughness": 0.55}
    (folder / "LampOff.material.json").write_text(json.dumps(base, indent=2) + "\n", encoding="utf-8")
    for color_name, color in (("Warm", (1, 0.5, 0.35)), ("Cool", (0.3, 0.8, 1))):
        for state, intensity in (("On", 1), ("Dim", 0.12)):
            name = f"Lamp{color_name}{state}"
            pixel = bytes(srgb(channel * intensity) for channel in color)
            scanlines = (b"\x00" + pixel * 4) * 4
            png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 2, 0, 0, 0))
            png += chunk(b"IDAT", zlib.compress(scanlines)) + chunk(b"IEND", b"")
            (folder / f"{name}.png").write_bytes(png)
            material = dict(base, emissiveTexture=f"Game/Content/BabyDoll/Materials/{name}.png")
            (folder / f"{name}.material.json").write_text(json.dumps(material, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
