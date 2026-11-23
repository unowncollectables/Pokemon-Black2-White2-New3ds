#!/bin/sh
# Build both New 3DS shells and compile both games' C. No ROM.
# A normal setup is devkitPro at /opt/devkitpro. This script does not
# download a ROM and does not build a CIA.
set -e
cd "$(dirname "$0")/.."
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITARM="${DEVKITARM:-$DEVKITPRO/devkitARM}"
if [ -d "$DEVKITPRO/tools/bin" ]; then
    export PATH="$DEVKITPRO/tools/bin:$PATH"
fi
make -C n3ds "$@"
