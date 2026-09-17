#!/bin/sh
# Lay this application layer over a FoloToy AI Passport baseline checkout.
#
# This repository is not buildable on its own. It holds only the files the
# Penalty game adds to, or changes in, the upstream firmware baseline, so they
# have to be copied into a checkout of that baseline before anything can build.
#
#   ./apply.sh /path/to/ai-passport           copy the files
#   ./apply.sh --dry-run /path/to/ai-passport list what would change
#
# Files this layer changes are overwritten in the target. MANIFEST.md lists
# which baseline files those are; the four under components/bsp are the ones
# that matter if you are tracking upstream changes.

set -eu

dry_run=false
if [ "${1:-}" = "--dry-run" ] || [ "${1:-}" = "-n" ]; then
    dry_run=true
    shift
fi

target=${1:-}
if [ -z "$target" ]; then
    echo "usage: $0 [--dry-run] <path-to-ai-passport-checkout>" >&2
    exit 2
fi
if [ ! -d "$target" ]; then
    echo "$0: not a directory: $target" >&2
    exit 2
fi
if [ ! -f "$target/components/bsp/include/bsp_pins.h" ] ||
   ! grep -q 'project(FoloToy-AI-Passport)' "$target/CMakeLists.txt" 2>/dev/null; then
    echo "$0: $target does not look like a FoloToy AI Passport checkout" >&2
    echo "    expected project(FoloToy-AI-Passport) in CMakeLists.txt" >&2
    echo "    and components/bsp/include/bsp_pins.h" >&2
    exit 2
fi

here=$(cd "$(dirname "$0")" && pwd)

list=$(mktemp)
cleanup() { rm -f "$list"; }
trap cleanup EXIT INT TERM
(cd "$here" && find . -type f -not -path './.git/*') | sed 's|^\./||' | sort > "$list"

added=0
changed=0
while IFS= read -r f; do
    case "$f" in
        apply.sh | README.md | README.zh_CN.md | LICENSE | MANIFEST.md) continue ;;
    esac
    if [ -e "$target/$f" ]; then
        if [ "$dry_run" = true ]; then
            echo "would replace  $f"
        else
            mkdir -p "$target/$(dirname "$f")"
            cp -p "$here/$f" "$target/$f"
        fi
        changed=$((changed + 1))
    else
        if [ "$dry_run" = true ]; then
            echo "would add      $f"
        else
            mkdir -p "$target/$(dirname "$f")"
            cp -p "$here/$f" "$target/$f"
        fi
        added=$((added + 1))
    fi
done < "$list"

if [ "$dry_run" = true ]; then
    echo
    echo "dry run: would add $added and replace $changed files in $target"
    exit 0
fi

echo
echo "applied $added new and $changed updated files to $target"
echo
echo "next:"
echo "  cd $target"
echo "  source /path/to/esp-idf-5.5.3/export.sh"
echo "  ./tools/validate.sh --static     # repository checks and host tests"
echo "  ./tools/validate.sh --firmware   # ESP-IDF build and merged-image check"
