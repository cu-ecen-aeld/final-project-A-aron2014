#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

bitbake-setup init \
    --non-interactive \
    "${SCRIPT_DIR}/bitbake-setup/yocto-ros.conf.json" \
    yocto-final-project
