#!/bin/bash
# Null test: renders fixed test audio through the processor and compares it with tests/reference/.
#   tools/null_test.sh                      check, any difference fails
#   tools/null_test.sh --tolerance-db <dB>  check, differences up to <dB> pass (set just above a measured difference)
#   tools/null_test.sh write                write new references (only when the sound changes on purpose)
set -e
cd "$(dirname "$0")/.."
cmake -B build-tools -G Ninja -DCMAKE_BUILD_TYPE=Release -DHHD_NULLTEST=ON -DHHD_SCREENSHOTS=ON > /dev/null
cmake --build build-tools --target HyperDimNullTest
if [ "$1" = "write" ]; then
    build-tools/HyperDimNullTest write tests/reference
else
    build-tools/HyperDimNullTest check tests/reference "$@"
fi
