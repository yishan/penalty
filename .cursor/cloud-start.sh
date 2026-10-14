#!/usr/bin/env bash
# Re-apply the checked-out Penalty layer onto the pinned AI Passport baseline.
set -euo pipefail

if [[ ! -f /workspace/apply.sh ]]; then
  echo "penalty checkout must be at /workspace" >&2
  exit 1
fi

BASE=/opt/ai-passport
PIN=ccd3576e304f7e17d9a0d4c12a2d05db379b14c0
if [[ ! -d "${BASE}/.git" ]]; then
  echo "AI Passport baseline is missing at ${BASE}" >&2
  exit 1
fi
if ! git -C "${BASE}" rev-parse --verify --quiet "${PIN}^{commit}" >/dev/null; then
  git -C "${BASE}" fetch --depth 1 origin "${PIN}"
fi
git -C "${BASE}" reset --hard "${PIN}"
git -C "${BASE}" clean -fd
/workspace/apply.sh "${BASE}"
echo "Penalty overlay applied to ${BASE} at baseline ${PIN}"
