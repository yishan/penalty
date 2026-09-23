#!/usr/bin/env python3
"""Extract deterministic native Penalty action sprites from approved source sheets."""

import argparse
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets/images/penalty/gameplay-v2"


def cells(path: Path, columns: int, rows: int) -> list[Image.Image]:
    source = Image.open(path).convert("RGBA")
    result = []
    for row in range(rows):
        for column in range(columns):
            left = round(column * source.width / columns)
            right = round((column + 1) * source.width / columns)
            top = round(row * source.height / rows)
            bottom = round((row + 1) * source.height / rows)
            result.append(source.crop((left, top, right, bottom)))
    return result


def fitted(cell: Image.Image, size: int, margin: int) -> Image.Image:
    bbox = cell.getchannel("A").getbbox()
    if not bbox:
        raise ValueError("source cell has no visible sprite")
    sprite = cell.crop(bbox)
    limit = size - margin * 2
    scale = min(limit / sprite.width, limit / sprite.height)
    width = max(1, round(sprite.width * scale))
    height = max(1, round(sprite.height * scale))
    sprite = sprite.resize((width, height), Image.Resampling.NEAREST)
    output = Image.new("RGBA", (size, size))
    output.alpha_composite(sprite, ((size - width) // 2, size - margin - height))
    return output


def generated() -> dict[str, Image.Image]:
    result = {}
    red = cells(ASSETS / "striker-red-5-source.png", 5, 1)
    blue = cells(ASSETS / "striker-blue-9-source.png", 3, 3)
    keeper = cells(ASSETS / "keeper-extra-3-source.png", 3, 1)
    for index, cell in enumerate(red):
        result[f"striker-{index}-88x88.png"] = fitted(cell, 88, 3)
    for index, cell in enumerate(blue):
        result[f"blue-striker-{index}-88x88.png"] = fitted(cell, 88, 3)
    for offset, cell in enumerate(keeper, start=4):
        result[f"keeper-{offset}-56x56.png"] = fitted(cell, 56, 2)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    for name, image in generated().items():
        path = ASSETS / name
        if args.check:
            if not path.exists() or Image.open(path).convert("RGBA").tobytes() != image.tobytes():
                raise SystemExit(f"Outdated generated action sprite: {name}")
        else:
            image.save(path)
    print("Penalty action sprites: reproducible PASS" if args.check else
          "Penalty action sprites: prepared")


if __name__ == "__main__":
    main()
