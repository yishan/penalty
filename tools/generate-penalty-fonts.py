#!/usr/bin/env python3
"""Generate the two deterministic Penalty Simplified Chinese LVGL subsets."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
FONT_SOURCE = ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"
FONT_DIR = ROOT / "assets/fonts/penalty"
TEXT_SOURCES = [ROOT / "main/penalty_i18n.c", ROOT / "main/penalty_copy.c"]


def localized_symbols() -> str:
    text = "".join(path.read_text(encoding="utf-8") for path in TEXT_SOURCES)
    return "".join(sorted({character for character in text if ord(character) > 0x7F}))


def main() -> None:
    FONT_DIR.mkdir(parents=True, exist_ok=True)
    symbols = localized_symbols()
    for size in (14, 20):
        subprocess.run(
            [
                "npx", "--yes", "--cache", str(Path(tempfile.gettempdir()) / "penalty-npm-cache"),
                "lv_font_conv@1.5.3",
                "--no-compress", "--no-prefilter", "--bpp", "4",
                "--size", str(size), "--font", str(FONT_SOURCE),
                "-r", "0x20-0x7f", "--symbols", symbols,
                "--format", "lvgl", "--force-fast-kern-format",
                "--lv-include", "lvgl.h",
                "--lv-font-name", f"penalty_font_zh_{size}",
                "-o", str(FONT_DIR / f"penalty_font_zh_{size}.c"),
            ],
            check=True,
        )


if __name__ == "__main__":
    main()
