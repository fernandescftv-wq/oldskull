cmake_minimum_required(VERSION 3.16)

set(FLASH_SIZE "0x1000000" CACHE STRING "Flash size in hex")
set(PARTITION_TABLE_OFFSET "0x8000" CACHE STRING "Partition table offset")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_lvgl_oldskull)

set(PARTITION_TABLE_CUSTOM_FILENAME "partitions_oldskull.csv" CACHE STRING "")
