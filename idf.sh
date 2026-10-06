#!/usr/bin/env sh
set -e
cd /home/e/esp/esp32_p4_jc4880p433c_bsp

# Remove old venv hermes paths
export PATH="/home/e/.espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin"

# Build
python3 /home/e/esp/esp-idf/tools/idf.py build "$@"
# Flash
python3 /home/e/esp/esp-idf/tools/idf.py --port /dev/ttyACM0 flash "$@"