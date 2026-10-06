<p align="right">
  <a href="build-and-test.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Build and Test

Use ESP-IDF 5.5.3. On a clean machine or when the toolchain is missing, follow
the [environment bootstrap](environment-setup.md) first.

> Prefer `./tools/validate.sh --firmware` for firmware builds. Flash its
> verified `build/FoloToy-AI-Passport-full.bin` at offset `0x0` for a blank
> device or an intentional complete refresh. The merged image may reset NVS;
> use segmented `idf.py flash` when existing NVS state must be preserved. Treat
> `idf.py build` and `idf.py flash` as incremental development commands, not the
> default delivery path.

```bash
source <path-to-esp-idf-v5.5.3>/export.sh
idf.py --version             # must report ESP-IDF v5.5.3
./tools/validate.sh --firmware # preferred: build and verify merged 0x0 image
idf.py set-target esp32c3     # fresh checkout or changed target
idf.py build                  # optional incremental application build
idf.py flash monitor          # optional incremental application flash
idf.py fullclean              # remove stale generated build state only
```

`idf.py fullclean` does not fully synchronize an existing `sdkconfig` with
changed defaults. Preserve intentional local settings, then run
`idf.py set-target esp32c3` when the target or tracked defaults must be
regenerated.

The tracked `dependencies.lock` pins Managed Component resolution. After changing an `idf_component.yml`, regenerate the lock with ESP-IDF 5.5.3, review version changes, and commit it with the manifest. An ordinary build must not leave an unexplained lock-file diff.

Firmware validation uses a fresh temporary build directory and an isolated `sdkconfig` generated from the tracked defaults. It does not consume or overwrite a developer's root `sdkconfig`. It preserves the verified canonical merged image at `build/FoloToy-AI-Passport-full.bin` and, for this project, creates the byte-identical delivery copy `build/FoloToy-AI-Passport-Penalty-v<MAJOR.MINOR.PATCH>-full.bin` from `penalty/VERSION`. The gate also validates the [configured firmware layout](firmware-layout.md): image offsets from `flash_args`, partition-table MD5, bounds and non-overlap, and an application that starts in and fits its configured app partition. User-defined partition layouts are allowed.

The gate also retains matching ELF, MAP, merged/application/bootloader/partition-table images and `flash_args` under `build/firmware/<full-bin-sha256>/`. The application descriptor's embedded ELF SHA256 must match the retained ELF. The immutable bundle binds debugging files to the checked firmware, not to an arbitrary later rebuild. A repeated archive of identical firmware reuses the first verified MAP because temporary build paths can change the MAP without changing firmware. Verify a bundle before device handoff:

```bash
python3 tools/archive_firmware.py verify build/firmware/<full-bin-sha256>
```

The archive does not sanitize firmware contents. Keep binaries and debug bundles out of commits. For a device with a different partition layout, first confirm compatibility and write only the approved app range; do not replace its layout with the merged image by default.

The baseline also has a hardware-independent logic test:

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_ui_pixel_math.c main/ui_pixel_math.c \
  -o /tmp/test_ui_pixel_math
/tmp/test_ui_pixel_math
```

Use the unified validation entry point:

```bash
./tools/validate.sh --static    # repository checks, workflows, links, secrets, host tests
./tools/validate.sh --firmware  # build, merge-bin, offsets, and configured layout
./tools/validate.sh             # complete gate; requires an activated ESP-IDF environment
```

CI calls the same script. Fix the shared script or environment if local and CI behavior differs; do not duplicate command sequences in workflows.

For the Penalty M4 controlled-disconnect development variant, run
`./tools/validate.sh --all --dev-ble-faults`. The gate adds the tracked
`sdkconfig.dev-ble-faults` overlay and verifies `CONFIG_PENALTY_DEV_BLE_FAULTS=y`.
Normal builds default to disabled and reject an enabled hook. Development
delivery copies stay under `build/dev-ble-faults/`, with a
`-dev-ble-faults-full.bin` suffix; they do not replace normal delivery copies.
Both variants retain content-addressed debugging bundles in `build/firmware/`.
Do not publish this development variant. Device flashing remains separately
authorized and app-only for the two existing compatible factory layouts.
When IDF's Python lacks host-only image-test dependencies, set `HOST_PYTHON` to
an existing Python with those dependencies; IDF still uses its own interpreter.

Hardware-affecting changes must also run the applicable on-device checklist in the hardware guide. Report compilation separately from physical-device validation.

Penalty M5 firmware/full gates also run both actual LVGL 9.5.0 desktop
renderers and `idf.py size` before archiving. The single-player renderer covers
all three modes; the multiplayer renderer covers bilingual lobby/security,
difficulty, coin, locked waiting, recovery and summaries. Both check 50
entry/exit cycles in the 24 KiB pool. These generated captures are desktop
evidence, not device screenshots or runtime-resource measurements. Static-only
validation checks fixed-state budgets and lifecycle contracts but does not run
the CMake renderers; run `tools/preview-penalty.sh` for a separate render check.

Never upload the app-only `build/FoloToy-AI-Passport.bin` to the community. Use
the project-and-version delivery copy for handoff; it is byte-identical to the
validated canonical `build/FoloToy-AI-Passport-full.bin`, which contains the
complete checked firmware layout.
