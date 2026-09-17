#!/usr/bin/env python3
from pathlib import Path
from statistics import pstdev

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "assets/images/penalty"


def is_green(pixel: tuple[int, int, int, int]) -> bool:
    red, green, blue, alpha = pixel
    return alpha > 128 and green > 50 and green > red * 1.15 and green > blue * 1.05


def stripe_ratio(path: Path, box: tuple[int, int, int, int]) -> float:
    image = Image.open(path).convert("RGBA").crop(box)
    rows = []
    for y in range(image.height):
        values = [sum(image.getpixel((x, y))[:3]) / 3 for x in range(image.width)
                  if is_green(image.getpixel((x, y)))]
        if values:
            rows.append(sum(values) / len(values))
    columns = []
    for x in range(image.width):
        values = [sum(image.getpixel((x, y))[:3]) / 3 for y in range(image.height)
                  if is_green(image.getpixel((x, y)))]
        if values:
            columns.append(sum(values) / len(values))
    assert rows and columns
    return pstdev(rows) / max(pstdev(columns), 0.01)


def is_blue_uniform(pixel: tuple[int, int, int, int]) -> bool:
    red, green, blue, alpha = pixel
    return alpha > 128 and blue > 90 and blue > red * 1.5 and blue > green * 1.15


cover = Image.open(ART / "cover-240x320.png").convert("RGBA")
background = Image.open(ART / "gameplay-v2/background-320x240.png").convert("RGBA")
tagline = Image.open(ART / "cover-tagline-128x20.png").convert("RGBA")
community = Image.open(ART / "community-cover-v1.1-horizontal.png").convert("RGBA")

assert cover.size == (240, 320)
assert background.size == (320, 240)
assert tagline.size == (128, 20)
assert community.size == (1086, 1448)

# Grass bands must vary primarily by row, not by column.
assert stripe_ratio(ART / "cover-240x320.png", (0, 112, 240, 226)) > 1.35
assert stripe_ratio(ART / "gameplay-v2/background-320x240.png", (0, 100, 320, 176)) > 1.35
assert stripe_ratio(ART / "community-cover-v1.1-horizontal.png", (0, 530, 1086, 1390)) > 1.35

# The exact pre-rendered Chinese mark is composited at the top of the cover footer.
for y in range(tagline.height):
    for x in range(tagline.width):
        pixel = tagline.getpixel((x, y))
        if pixel[3] > 128:
            assert cover.getpixel((56 + x, 231 + y))[:3] == pixel[:3]

# Runtime keeper sprites and the keeper visible on the cover no longer use a blue kit.
for frame in range(4):
    image = Image.open(ART / f"gameplay-v2/keeper-{frame}-56x56.png").convert("RGBA")
    assert sum(is_blue_uniform(pixel) for pixel in image.get_flattened_data()) == 0

cover_keeper = cover.crop((95, 82, 145, 128))
assert sum(is_blue_uniform(pixel) for pixel in cover_keeper.get_flattened_data()) < 100

print("Penalty art contract: PASS")
