#!/usr/bin/env bash
set -e

cd /home/e/esp/esp32_p4_jc4880p433c_bsp

# Use RISC-V toolchain for ESP32-P4
export PATH="/home/e/.espressif/tools/riscv32-esp-elf/esp-14.2.0_20251107/riscv32-esp-elf/bin:$PATH"
export IDF_PATH=/home/e/esp/esp-idf

# Clean build directory
rm -rf build

# Build using clean ESP-IDF venv
/tmp/idf_venv/bin/python /home/e/esp/esp-idf/tools/idf.py build 2>&1
echo "Build OK!"