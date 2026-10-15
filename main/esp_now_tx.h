#ifndef ESP_NOW_TX_H
#define ESP_NOW_TX_H

#include "esp_err.h"

esp_err_t esp_now_tx_init(void);
esp_err_t esp_now_tx_aplicar(int banco, uint8_t fxMask, uint8_t ch);
esp_err_t esp_now_tx_status_req(void);
int esp_now_tx_ultimo_ack(void);

#endif // ESP_NOW_TX_H