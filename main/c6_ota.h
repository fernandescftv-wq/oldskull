#pragma once
#include "esp_err.h"

/* Grava o firmware embutido do C6 (bridge ESP-NOW) via CP-OTA, uma vez. */
esp_err_t c6_ota_update_once(void);