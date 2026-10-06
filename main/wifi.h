// wifi.h - WiFi AP + UDP para comunicação TX→RX
#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa WiFi AP (SSID: old_skull, canal 1, sem senha)
void wifi_init_ap(void);

// Envia dados via UDP para um IP específico
// rx_ip: IP do receptor (ex: "192.168.4.1")
// port: porta do receptor
// data: buffer com os dados
// len: tamanho dos dados em bytes
esp_err_t wifi_send_udp(const char *rx_ip, uint16_t port, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_H */
