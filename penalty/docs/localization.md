<p align="right">
  <a href="localization.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Penalty — Bilingual UI Design

The game supports English and Simplified Chinese. Simplified Chinese is the safe default;
the selected language is stored in NVS and restored after leaving the game or
restarting the device. A missing, invalid, or unavailable preference falls back
to Simplified Chinese without blocking play. A valid saved English preference
continues to restore English.

The English title screen contains Shooting Challenge, Keeper Challenge,
Settings, and Help. Settings contains
Difficulty, Sound, Language, and Back. Changing the language refreshes the
current screen immediately and queues a non-blocking persistence write outside
the LVGL/input path.

Both roles are localized. Shooting reports goals; goalkeeper mode reports saves
and distinguishes caught, parried, wrong-way, correct-read miss, early, and late
outcomes. Keeper instructions are intentionally short enough for the portrait
footer, and Easy or the first attempt carries the teaching prompt.

English keeps the existing Montserrat fonts. Chinese uses generated 14 px and
20 px subsets of the repository-pinned Source Han Sans SC font under SIL OFL
1.1. The subsets contain only the glyphs used by the game. Host rendering must
verify that every localized code point exists and that every visible line fits
its LVGL label width. The current generated subset covers 191 localized glyphs
across both roles.

The portrait cover has fixed English and Chinese game marks.
They are part of the artwork and do not change with the selected UI language.
The artwork contains one empty start panel. Runtime renders the concise
`PRESS OK` label or its localized Chinese equivalent with the same font metrics,
width, and vertical position, so switching language cannot expose stale text or
shift the optical centre. The full artwork is not duplicated in Flash.
