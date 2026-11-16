#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"
git submodule update --init --recursive

# must match "setup-dir-name" in bitbake-setup/yocto-ros.conf.json
SETUP_DIR="${SCRIPT_DIR}/bitbake-builds/yocto-ros-final-project"

[ -e "${SETUP_DIR}/build/init-build-env" ] || "${SCRIPT_DIR}/setup.sh"
source "${SETUP_DIR}/build/init-build-env"

bitbake aesd-lidar-image