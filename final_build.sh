#!/bin/bash
# Build script using native Python
set -e

cd /home/e/esp/esp32_p4_jc4880p433c_bsp

# Toolchains
XTENSA=/home/e/.espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin
RISCV=/home/e/.espressif/tools/riscv32-esp-elf/esp-14.2.0_20251107/riscv32-esp-elf/bin

export IDF_PATH=/home/e/esp/esp-idf
export PATH="$RISCV:$XTENSA:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin"

# Use system Python
/usr/bin/python3 -c "
import sys, os
sys.path.insert(0, '$IDF_PATH/tools')
os.chdir('$PWD')
exec(open('$IDF_PATH/tools/idf.py').read())
" build "$@"

echo "Build complete"