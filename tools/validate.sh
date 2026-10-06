#!/usr/bin/env bash
set -euo pipefail

mode="${1:---all}"
dev_ble_faults=false
if [[ "${2:-}" == "--dev-ble-faults" && "$#" -le 2 ]]; then
    dev_ble_faults=true
elif [[ "$#" -gt 1 ]]; then
    echo "ERROR: unknown validation variant" >&2
    exit 2
fi
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
penalty_version_file="${repo_root}/penalty/VERSION"
python_bin="${HOST_PYTHON:-python3}"

usage() {
    echo "Usage: $0 [--all|--static|--firmware] [--dev-ble-faults]" >&2
}

read_penalty_version() {
    local version

    if [[ ! -f "${penalty_version_file}" ]]; then
        echo "ERROR: missing Penalty version file: ${penalty_version_file}" >&2
        return 1
    fi
    version="$(<"${penalty_version_file}")"
    if [[ ! "${version}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        echo "ERROR: Penalty version must be MAJOR.MINOR.PATCH, got: ${version}" >&2
        return 1
    fi
    printf '%s' "${version}"
}

penalty_firmware_name() {
    local version

    version="$(read_penalty_version)"
    if [[ "${dev_ble_faults}" == true ]]; then
        printf 'FoloToy-AI-Passport-Penalty-v%s-dev-ble-faults-full.bin' "${version}"
        return
    fi
    printf 'FoloToy-AI-Passport-Penalty-v%s-full.bin' "${version}"
}

run_static_checks() {
    local actionlint_bin
    local test_dir

    echo "Penalty firmware artifact: $(penalty_firmware_name)"

    "${python_bin}" tools/check_repo.py

    actionlint_bin="${ACTIONLINT_BIN:-}"
    if [[ -z "${actionlint_bin}" ]]; then
        actionlint_bin="$(command -v actionlint || true)"
    fi
    if [[ -z "${actionlint_bin}" || ! -x "${actionlint_bin}" ]]; then
        actionlint_bin="$(./tools/install-actionlint.sh)"
    fi
    "${actionlint_bin}" -color .github/workflows/*.yml

    test_dir="$(mktemp -d /tmp/ai-passport-host-tests.XXXXXX)"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_ui_pixel_math.c main/ui_pixel_math.c \
        -o "${test_dir}/test_ui_pixel_math"
    "${test_dir}/test_ui_pixel_math"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_demo_navigation.c main/demo_navigation.c \
        -o "${test_dir}/test_demo_navigation"
    "${test_dir}/test_demo_navigation"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_penalty_model.c main/penalty_model.c \
        -o "${test_dir}/test_penalty_model"
    "${test_dir}/test_penalty_model"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_penalty_duel_model.c main/penalty_duel_model.c main/penalty_model.c \
        -o "${test_dir}/test_penalty_duel_model"
    "${test_dir}/test_penalty_duel_model"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_penalty_ble_protocol.c main/penalty_ble_protocol.c \
        -o "${test_dir}/test_penalty_ble_protocol"
    "${test_dir}/test_penalty_ble_protocol"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_penalty_ble_session_core.c \
        main/penalty_ble_session_core.c main/penalty_ble_protocol.c \
        -o "${test_dir}/test_penalty_ble_session_core"
    "${test_dir}/test_penalty_ble_session_core"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_penalty_multiplayer_core.c main/penalty_multiplayer_core.c \
        main/penalty_duel_model.c main/penalty_model.c \
        -o "${test_dir}/test_penalty_multiplayer_core"
    "${test_dir}/test_penalty_multiplayer_core"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_penalty_multiplayer_recovery.c main/penalty_multiplayer_core.c \
        main/penalty_duel_model.c main/penalty_model.c main/penalty_ble_protocol.c main/penalty_ble_reliable.c main/penalty_ble_fault.c \
        -o "${test_dir}/test_penalty_multiplayer_recovery"
    "${test_dir}/test_penalty_multiplayer_recovery"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_penalty_ble_reliable.c main/penalty_ble_reliable.c \
        -o "${test_dir}/test_penalty_ble_reliable"
    "${test_dir}/test_penalty_ble_reliable"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_penalty_ble_fault.c main/penalty_ble_fault.c \
        -o "${test_dir}/test_penalty_ble_fault"
    "${test_dir}/test_penalty_ble_fault"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_penalty_m5_resources.c -o "${test_dir}/test_penalty_m5_resources"
    "${test_dir}/test_penalty_m5_resources"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_penalty_scene.c main/penalty_scene.c main/penalty_model.c \
        -o "${test_dir}/test_penalty_scene"
    "${test_dir}/test_penalty_scene"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_penalty_audio.c main/penalty_audio.c main/penalty_model.c \
        -o "${test_dir}/test_penalty_audio"
    "${test_dir}/test_penalty_audio"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_penalty_copy.c main/penalty_copy.c main/penalty_model.c \
        -o "${test_dir}/test_penalty_copy"
    "${test_dir}/test_penalty_copy"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_penalty_i18n.c main/penalty_i18n.c main/penalty_copy.c main/penalty_model.c \
        -o "${test_dir}/test_penalty_i18n"
    "${test_dir}/test_penalty_i18n"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_penalty_preferences.c main/penalty_preferences.c \
        -o "${test_dir}/test_penalty_preferences"
    "${test_dir}/test_penalty_preferences"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icomponents/bsp/src \
        tests/test_bsp_display_rounding.c components/bsp/src/bsp_display_rounding.c \
        -o "${test_dir}/test_bsp_display_rounding"
    "${test_dir}/test_bsp_display_rounding"
    PYTHONDONTWRITEBYTECODE=1 "${python_bin}" tests/test_deep_sleep_contract.py
    PYTHONDONTWRITEBYTECODE=1 "${python_bin}" tests/test_penalty_shell_contract.py
    PYTHONDONTWRITEBYTECODE=1 "${python_bin}" tests/test_penalty_ble_fault_contract.py
    PYTHONDONTWRITEBYTECODE=1 "${python_bin}" tests/test_penalty_m5_contract.py
    PYTHONDONTWRITEBYTECODE=1 "${python_bin}" tests/test_penalty_font_contract.py
    PYTHONDONTWRITEBYTECODE=1 "${python_bin}" tests/test_penalty_art_contract.py
    PYTHONDONTWRITEBYTECODE=1 "${python_bin}" tools/generate-penalty-sfx.py --check
    PYTHONDONTWRITEBYTECODE=1 "${python_bin}" tests/test_verify_firmware.py
    rm -rf "${test_dir}"
    echo "Host tests: PASS"
}

run_firmware_checks() (
    local versioned_name
    local validation_build_dir
    local config_defaults="${repo_root}/sdkconfig.defaults"
    local delivery_dir="${repo_root}/build"

    if ! command -v idf.py >/dev/null 2>&1; then
        echo "ERROR: idf.py is not available; activate ESP-IDF 5.5.3 first." >&2
        return 1
    fi

    validation_build_dir="$(mktemp -d /tmp/ai-passport-firmware.XXXXXX)"
    trap 'case "${validation_build_dir}" in /tmp/ai-passport-firmware.*) rm -rf -- "${validation_build_dir}" ;; esac' EXIT

    if [[ "${dev_ble_faults}" == true ]]; then
        config_defaults="${config_defaults};${repo_root}/sdkconfig.dev-ble-faults"
        delivery_dir="${repo_root}/build/dev-ble-faults"
        echo "Development-only BLE fault variant: ENABLED"
    fi
    SDKCONFIG_DEFAULTS="${config_defaults}" \
        idf.py -B "${validation_build_dir}" \
        -D "SDKCONFIG=${validation_build_dir}/sdkconfig" build
    if [[ "${dev_ble_faults}" == true ]]; then
        rg -q '^CONFIG_PENALTY_DEV_BLE_FAULTS=y$' "${validation_build_dir}/sdkconfig"
    elif rg -q '^CONFIG_PENALTY_DEV_BLE_FAULTS=y$' "${validation_build_dir}/sdkconfig"; then
        echo "ERROR: development BLE fault hooks enabled in normal build" >&2
        return 1
    fi
    idf.py -B "${validation_build_dir}" merge-bin \
        -o "${validation_build_dir}/FoloToy-AI-Passport-full.bin"
    "${python_bin}" tools/verify_firmware.py "${validation_build_dir}"
    # Render both gameplay paths with the resolved, locked LVGL sources.
    ./tools/preview-penalty.sh "${repo_root}/managed_components/lvgl__lvgl" \
        "${delivery_dir}/penalty-preview"
    idf.py -B "${validation_build_dir}" size
    "${python_bin}" tools/archive_firmware.py create "${validation_build_dir}" \
        --archive-root "${repo_root}/build/firmware"
    mkdir -p "${delivery_dir}"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport-full.bin" \
        "${delivery_dir}/FoloToy-AI-Passport-full.bin"
    versioned_name="$(penalty_firmware_name)"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport-full.bin" \
        "${delivery_dir}/${versioned_name}"
    cmp \
        "${delivery_dir}/FoloToy-AI-Passport-full.bin" \
        "${delivery_dir}/${versioned_name}"
    echo "Delivery artifact: ${delivery_dir}/${versioned_name}"
    echo "Firmware build: PASS"
)

cd "${repo_root}"
case "${mode}" in
    --all)
        run_static_checks
        run_firmware_checks
        ;;
    --static)
        run_static_checks
        ;;
    --firmware)
        run_firmware_checks
        ;;
    *)
        usage
        exit 2
        ;;
esac
