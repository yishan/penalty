#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCES = [ROOT / "main/penalty_i18n.c", ROOT / "main/penalty_copy.c"]
FONT_DIR = ROOT / "assets/fonts/penalty"


def chinese_characters(text: str) -> set[str]:
    return {character for character in text if ord(character) > 0x7F}


required = set()
for source in SOURCES:
    required |= chinese_characters(source.read_text(encoding="utf-8"))

assert required, "localized sources must contain non-ASCII characters"
for size in (14, 20):
    generated = FONT_DIR / f"penalty_font_zh_{size}.c"
    assert generated.is_file(), f"missing generated font: {generated.relative_to(ROOT)}"
    header = "\n".join(generated.read_text(encoding="utf-8").splitlines()[:12])
    missing = required - chinese_characters(header)
    assert not missing, f"{generated.name} misses localized glyphs: {''.join(sorted(missing))}"

license_file = FONT_DIR / "LICENSE-SourceHanSansSC.txt"
assert license_file.is_file(), "the redistributed font subset needs its OFL license"
license_text = license_file.read_text(encoding="utf-8")
assert "SIL OPEN FONT LICENSE Version 1.1" in license_text

print(f"Penalty font contract: PASS ({len(required)} localized glyphs)")

