<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Penalty

Penalty is an offline, portrait penalty-kick game for the [FoloToy AI Passport](https://github.com/folotoy/ai-passport). Keep the device upright and use its three buttons to play either side of a five-attempt shootout:

- **Shoot Challenge** — choose one of six goal cells, stop the timing meter, and beat the black-clad keeper.
- **Keeper Challenge** — read the blue striker's body and foot pose, choose the black keeper's dive, and time the save.

The game includes Easy, Normal, and Hard difficulties, English and Simplified Chinese, persistent language selection, short sound cues, pixel artwork, and a five-attempt summary.

## Download

Installable firmware is published on the [Releases page](https://github.com/yishan/penalty/releases). Download the file ending in `-full.bin`; GitHub's automatically generated source archives are not flashable firmware.

| Version | Status | Firmware SHA-256 |
| --- | --- | --- |
| [v1.4.2](https://github.com/yishan/penalty/releases/tag/v1.4.2) | Latest; build and host tests passed, device acceptance pending | `c7f93051605164d6e26c479a63164bdb04e1c26d215bd9ea9e1637a494df4037` |
| [v1.1.0](https://github.com/yishan/penalty/releases/tag/v1.1.0) | Historical device-verified visual refresh | `98aeea498634309fe4d10248347c513dd7b212e3d9ce02d651a7400c5ba66607` |

Each release also includes `SHA256SUMS.txt`. Verify the download before flashing:

```bash
shasum -a 256 FoloToy-AI-Passport-Penalty-v1.4.2-full.bin
```

Flash the merged image at offset `0x0`. It contains the bootloader, partition table, and application; writing it may reset stored settings such as the saved language.

## How to play

Press OK on the portrait cover. UP and DOWN select Shoot, Keep, Settings, or Help; OK confirms.

Both play modes use the same six-cell order. Every attempt starts at top-center. DOWN cycles:

```text
top-center → bottom-center → top-left → bottom-left
           → top-right → bottom-right → top-center
```

UP follows the reverse order. This keeps vertical movement aligned with the physical buttons while changing columns only after visiting both cells.

### Shoot Challenge

1. Select a target and press OK.
2. Press OK again to stop the timing meter and shoot.

The two-value dark line has a 90% average goal rate, the rest of the green band 65%, and normal power outside green 25%. Very weak and excessive power fail. Easy, Normal, and Hard narrow the green band from 20 to 11 to 6 values.

### Keeper Challenge

1. Watch the blue striker's truthful opening pose. The cue remains visible for 900 / 600 / 350 ms on Easy / Normal / Hard.
2. Choose the black keeper's dive and press OK.
3. Press OK again to stop the timing meter and dive.

Reading the exact cell gives 90% / 65% / 25% save rates on the dark line / green band / outside green. Choosing the other row in the same column gives 35% / 20% / 5%; another column cannot save the shot.

Five attempts lead to a goals or saves summary. Long-press OK to leave a session and return to the cover.

## Source package

This repository is an application-layer export, not a standalone ESP-IDF project. It is generated from authoritative Penalty source commit `d5f08e7a1d345f9d9cac5552167eaf4166c8f5d0` against AI Passport base `ccd3576e304f7e17d9a0d4c12a2d05db379b14c0`.

Apply it to that exact baseline:

```bash
git clone https://github.com/folotoy/ai-passport.git
cd ai-passport
git checkout ccd3576e304f7e17d9a0d4c12a2d05db379b14c0
/path/to/penalty/apply.sh --dry-run .
/path/to/penalty/apply.sh .
```

Then build with ESP-IDF 5.5.3:

```bash
source /path/to/esp-idf-v5.5.3/export.sh
./tools/validate.sh --static
./tools/validate.sh --firmware
```

`MANIFEST.md` records the base, source commit, launcher contract, and every added or replaced path. The current package's full validation passed on 2026-09-23; the exact v1.4.2 GitHub firmware still needs on-device verification of the default Chinese startup and the shorter English Keeper title.

## Licence

MIT, retaining the FoloToy baseline copyright notice in [LICENSE](LICENSE). The Chinese font subset uses Source Han Sans SC under the bundled SIL Open Font License; the sound effects are CC0; artwork source and generation notes are recorded in [`assets/README.md`](assets/README.md).
