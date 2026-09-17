#!/usr/bin/env python3
"""Apply the approved v1.1 Penalty palette/layout edits to source artwork."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont


ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "assets/images/penalty"
GAMEPLAY = ART / "gameplay-v2"
FONT = ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"
CREAM = (247, 239, 205, 255)
SHADOW = (7, 32, 34, 255)


def is_grass(pixel: tuple[int, int, int, int]) -> bool:
    red, green, blue, alpha = pixel
    return (alpha > 128 and green > 50 and green > red * 1.18 and
            green > blue * 1.12 and red < 115 and blue < 105)


def horizontal_grass(image: Image.Image, y0: int, y1: int, band: int,
                     dark: tuple[int, int, int], light: tuple[int, int, int]) -> None:
    pixels = image.load()
    for y in range(max(y0, 0), min(y1, image.height)):
        color = light if ((y - y0) // band) % 2 == 0 else dark
        for x in range(image.width):
            if is_grass(pixels[x, y]):
                pixels[x, y] = (*color, pixels[x, y][3])


def is_uniform_blue(pixel: tuple[int, int, int, int]) -> bool:
    red, green, blue, alpha = pixel
    return (alpha > 32 and blue > 85 and blue > red * 1.45 and
            blue > green * 1.10)


def black_kit(image: Image.Image, box: tuple[int, int, int, int] | None = None) -> None:
    x0, y0, x1, y1 = box or (0, 0, image.width, image.height)
    pixels = image.load()
    for y in range(max(y0, 0), min(y1, image.height)):
        for x in range(max(x0, 0), min(x1, image.width)):
            pixel = pixels[x, y]
            if not is_uniform_blue(pixel):
                continue
            # Retain the original highlight ordering without leaving saturated blue.
            value = 20 + min(38, max(0, (pixel[2] - 80) * 38 // 175))
            pixels[x, y] = (value, value + 2, value + 1, pixel[3])


def draw_chinese_mark(image: Image.Image, y: int, font_size: int,
                      stroke: int) -> None:
    font = ImageFont.truetype(str(FONT), font_size)
    text = "点球决胜"
    probe = ImageDraw.Draw(Image.new("L", (1, 1)))
    box = probe.textbbox((0, 0), text, font=font)
    width = box[2] - box[0]
    x = (image.width - width) // 2 - box[0]
    mask = Image.new("L", image.size)
    ImageDraw.Draw(mask).text((x, y), text, font=font, fill=255)
    mask = mask.filter(ImageFilter.MaxFilter(5))
    outline = mask.filter(ImageFilter.MaxFilter(stroke * 2 + 1))
    shadow = Image.new("L", image.size)
    shadow.paste(outline, (font_size // 9, font_size // 8))
    image.paste(SHADOW, (0, 0, image.width, image.height), shadow)
    image.paste(SHADOW, (0, 0, image.width, image.height), outline)
    image.paste(CREAM, (0, 0, image.width, image.height), mask)


def style_cover() -> None:
    base = Image.open(ART / "cover-source-v1.0.png").convert("RGBA")
    horizontal_grass(base, 500, 1051, 82, (27, 116, 49), (42, 146, 59))
    black_kit(base, (420, 360, 665, 595))

    # Reflow the existing rotation diagram and instruction within the same footer.
    original = base.copy()
    for y in range(1050, base.height):
        color = original.getpixel((45, y))
        ImageDraw.Draw(base).line((0, y, base.width, y), fill=color)
    icons = original.crop((249, 1059, 837, 1236)).resize((470, 113), Image.Resampling.NEAREST)
    base.paste(icons, (308, 1140))
    instructions = original.crop((190, 1244, 905, 1416))
    base.paste(instructions, (190, 1267))
    draw_chinese_mark(base, 1029, 82, 5)
    base.convert("RGB").save(ART / "cover-source.png")
    native = base.resize((240, 320), Image.Resampling.NEAREST)
    native.convert("RGB").save(ART / "cover-240x320.png")
    native.crop((56, 231, 184, 251)).save(ART / "cover-tagline-128x20.png")


def style_background() -> None:
    image = Image.open(GAMEPLAY / "background-source-v1.0.png").convert("RGBA")
    horizontal_grass(image, 430, 827, 73, (22, 102, 48), (37, 137, 58))
    image.convert("RGB").save(GAMEPLAY / "background-source.png")
    image.resize((320, 240), Image.Resampling.NEAREST).convert("RGB").save(
        GAMEPLAY / "background-320x240.png")


def style_keeper() -> None:
    sheet = Image.open(GAMEPLAY / "keeper-sheet-source-v1.0.png").convert("RGBA")
    black_kit(sheet)
    sheet.save(GAMEPLAY / "keeper-sheet-source.png")
    for index in range(4):
        image = Image.open(GAMEPLAY / f"keeper-source-{index}-v1.0.png").convert("RGBA")
        black_kit(image)
        image.save(GAMEPLAY / f"keeper-source-{index}.png")
        image.resize((56, 56), Image.Resampling.NEAREST).save(
            GAMEPLAY / f"keeper-{index}-56x56.png")


def style_community_cover() -> None:
    image = Image.open(ART / "community-cover-v0.4-no-label.png").convert("RGBA")
    horizontal_grass(image, 515, image.height, 118, (27, 116, 49), (42, 146, 59))
    black_kit(image, (420, 350, 665, 590))
    draw_chinese_mark(image, 1330, 82, 5)
    image.convert("RGB").save(ART / "community-cover-v1.1-horizontal.png")


def main() -> None:
    style_cover()
    style_background()
    style_keeper()
    style_community_cover()
    print("Penalty art style: horizontal grass, black keeper kit, Chinese cover mark")


if __name__ == "__main__":
    main()
