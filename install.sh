#!/bin/bash
# Installs the freshly built plugins system-wide into /Library/Audio/Plug-Ins
# (needs your admin password once) and ad-hoc signs them so Apple Silicon hosts load them.
set -e
cd "$(dirname "$0")"

VST3_DST="/Library/Audio/Plug-Ins/VST3"
AU_DST="/Library/Audio/Plug-Ins/Components"

if [ ! -d build ]; then
    echo "No build folder found. Build first:"
    echo "  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build"
    exit 1
fi

sudo mkdir -p "$VST3_DST" "$AU_DST"

install_bundle () {   # $1 = bundle path, $2 = destination folder
    local name
    name="$(basename "$1")"
    echo "Installing $name -> $2"
    sudo rm -rf "$2/$name"
    sudo ditto "$1" "$2/$name"
    sudo codesign --force --deep -s - "$2/$name"
}

count=0
while IFS= read -r -d '' b; do
    install_bundle "$b" "$VST3_DST"; count=$((count+1))
done < <(find build -name "*.vst3" -type d -print0)

while IFS= read -r -d '' b; do
    install_bundle "$b" "$AU_DST"; count=$((count+1))
done < <(find build -name "*.component" -type d -print0)

if [ "$count" -eq 0 ]; then
    echo "No .vst3 / .component bundles found in ./build - did the build finish?"
    exit 1
fi

killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Done. Rescan plugins in your DAW."
