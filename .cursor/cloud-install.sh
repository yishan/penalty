#!/usr/bin/env bash
# Cloud Agent install: host packages, ESP-IDF 5.5.3, and the pinned AI Passport baseline.
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  git wget curl flex bison gperf \
  python3 python3-pip python3-venv python3-pil \
  cmake ninja-build ccache \
  libffi-dev libssl-dev dfu-util libusb-1.0-0 \
  build-essential ca-certificates xz-utils unzip pkg-config \
  ripgrep
# Host image contracts call Image.get_flattened_data(), added in Pillow 12.1.
sudo /usr/bin/python3 -m pip install --break-system-packages 'Pillow>=12.1,<13'

sudo mkdir -p /opt/esp
sudo chown "$(id -u):$(id -g)" /opt/esp

IDF_ROOT=/opt/esp/esp-idf-v5.5.3
if [[ ! -f "${IDF_ROOT}/export.sh" ]]; then
  git clone --branch v5.5.3 --recursive --depth 1 --shallow-submodules \
    https://github.com/espressif/esp-idf.git "${IDF_ROOT}"
fi
"${IDF_ROOT}/install.sh" esp32c3
# shellcheck disable=SC1091
source "${IDF_ROOT}/export.sh" >/dev/null
python -m pip install 'Pillow>=12.1,<13'

sudo tee /usr/local/bin/idf.py >/dev/null <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
export IDF_PATH=/opt/esp/esp-idf-v5.5.3
# shellcheck disable=SC1091
source "${IDF_PATH}/export.sh" >/dev/null
exec "${IDF_PATH}/tools/idf.py" "$@"
EOF
sudo chmod 755 /usr/local/bin/idf.py

if [[ ! -f /workspace/apply.sh ]]; then
  echo "penalty checkout must be at /workspace" >&2
  exit 1
fi

BASE=/opt/ai-passport
PIN=ccd3576e304f7e17d9a0d4c12a2d05db379b14c0
if [[ ! -d "${BASE}/.git" ]]; then
  git clone https://github.com/folotoy/ai-passport.git "${BASE}"
fi
if ! git -C "${BASE}" rev-parse --verify --quiet "${PIN}^{commit}" >/dev/null; then
  git -C "${BASE}" fetch --depth 1 origin "${PIN}"
fi
git -C "${BASE}" reset --hard "${PIN}"
git -C "${BASE}" clean -fd
/workspace/apply.sh "${BASE}"

prefetch="$(mktemp -d /tmp/penalty-idf-prefetch.XXXXXX)"
idf.py -C "${BASE}" -B "${prefetch}" -D "SDKCONFIG=${prefetch}/sdkconfig" reconfigure
rm -rf "${prefetch}"

version="$(idf.py --version)"
printf '%s\n' "${version}"
case "${version}" in
  *"ESP-IDF v5.5.3"*) ;;
  *)
    echo "expected ESP-IDF v5.5.3, got: ${version}" >&2
    exit 1
    ;;
esac
