#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
lvgl_dir="${1:-${repo_root}/managed_components/lvgl__lvgl}"
output_dir="${2:-${repo_root}/build/penalty-preview}"
if [[ ! -f "${lvgl_dir}/lv_version.h" ]] ||
   ! grep -Eq 'LVGL_VERSION_MINOR[[:space:]]+5' "${lvgl_dir}/lv_version.h" ||
   ! grep -Eq 'LVGL_VERSION_MAJOR[[:space:]]+9' "${lvgl_dir}/lv_version.h" ||
   ! grep -Eq 'LVGL_VERSION_PATCH[[:space:]]+0' "${lvgl_dir}/lv_version.h"; then
    echo "Expected LVGL 9.5.0 sources; pass their directory as the first argument." >&2
    exit 1
fi
mkdir -p "${output_dir}"
cmake -S "${repo_root}/tests/penalty_preview" -B "${output_dir}/host-build" \
    -DLVGL_DIR="${lvgl_dir}" -DCMAKE_BUILD_TYPE=Debug
cmake --build "${output_dir}/host-build" --parallel 6
"${output_dir}/host-build/penalty_preview" "${output_dir}"
echo "Actual LVGL screen captures (PPM): ${output_dir}"
