#!/bin/bash
# Renders the README screenshots (docs/*.png) from the real plug-in interface.
set -e
cd "$(dirname "$0")/.."
cmake -B build-docs -G Ninja -DCMAKE_BUILD_TYPE=Release -DHHD_SCREENSHOTS=ON > /dev/null
cmake --build build-docs --target HyperDimScreenshots
build-docs/HyperDimScreenshots docs
