#!/bin/bash
# Screenshots of the real plug-in interface.
#   tools/make_screenshots.sh           README images (docs/*.png)
#   tools/make_screenshots.sh baseline  write the reference screenshots (tests/reference/screenshots/)
#   tools/make_screenshots.sh check     compare with them; any differing pixel fails
#   tools/make_screenshots.sh check --pixel-tolerance 2   allow up to 2 per channel in any pixel
set -e
cd "$(dirname "$0")/.."
cmake -B build-tools -G Ninja -DCMAKE_BUILD_TYPE=Release -DHHD_NULLTEST=ON -DHHD_SCREENSHOTS=ON > /dev/null
cmake --build build-tools --target HyperDimScreenshots
case "$1" in
    baseline) build-tools/HyperDimScreenshots baseline tests/reference/screenshots ;;
    check)    shift; build-tools/HyperDimScreenshots check tests/reference/screenshots "$@" ;;
    *)        build-tools/HyperDimScreenshots docs ;;
esac
