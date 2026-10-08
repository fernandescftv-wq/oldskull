#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t web_server_init(void);
esp_err_t web_server_stop(void);
esp_err_t web_server_enable(bool on);

#endif // WEB_SERVER_H