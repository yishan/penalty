<p align="right">
  <a href="localization.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Penalty — Bilingual UI Design

The game supports English and Simplified Chinese. English is the safe default;
the selected language is stored in NVS and restored after leaving the game or
restarting the device. A missing, invalid, or unavailable preference falls back
to English without blocking play.

The title screen contains Play, Settings, and Help. Settings contains
Difficulty, Sound, Language, and Back. Changing the language refreshes the
current screen immediately and queues a non-blocking persistence write outside
the LVGL/input path.

English keeps the existing Montserrat fonts. Chinese uses generated 14 px and
20 px subsets of the repository-pinned Source Han Sans SC font under SIL OFL
1.1. The subsets contain only the glyphs used by the game. Host rendering must
verify that every localized code point exists and that every visible line fits
its LVGL label width.

The portrait cover has fixed English and Chinese game marks.
They are part of the artwork and do not change with the selected UI language.
When Chinese is selected, an opaque localized orientation prompt replaces the
baked English instruction; the full artwork is not duplicated in Flash.
