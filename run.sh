#!/usr/bin/env bash
cd /home/e/esp/esp32_p4_jc4880p433c_bsp
export HOME=/home/e
export PYTHONPATH=""
export PATH="/home/e/.espressif/tools/riscv32-esp-elf/esp-14.2.0_20251107/riscv32-esp-elf/bin:/home/e/.espressif/python_env/idf5.5_py3.11_env/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

# Build
python3 /home/e/esp/esp-idf/tools/idf.py build "$@"