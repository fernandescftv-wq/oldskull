// wifi.c - WiFi AP + UDP server para comunicação TX→RX
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "lwip/err.h"
#include <string.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <arpa/inet.h>

static const char *TAG = "wifi_ap";

#define WIFI_SSID     "old_skull"
#define WIFI_CHANNEL  1
#define UDP_PORT_TX   9999
#define UDP_PORT_RX   9998

static int udp_server_sock = -1;

// Forward declarations
static void udp_server_task(void *pvParameters);
static void start_udp_server(void);

// ===== WiFi AP =====
void wifi_init_ap(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
            .pmf_cfg = {.required = false},
        },
    };
    
    if (strlen((char*)ap_config.ap.ssid) == 0) {
        ap_config.ap.ssid_len = 0;
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi AP started, SSID: %s, channel: %d", WIFI_SSID, WIFI_CHANNEL);
    
    start_udp_server();
}

// ===== UDP Server =====
static void start_udp_server(void)
{
    udp_server_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_server_sock < 0) {
        ESP_LOGE(TAG, "Falha ao criar socket UDP server");
        return;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(UDP_PORT_TX);
    
    if (bind(udp_server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Falha ao bind UDP server na porta %d", UDP_PORT_TX);
        close(udp_server_sock);
        udp_server_sock = -1;
        return;
    }
    
    int flags = fcntl(udp_server_sock, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(udp_server_sock, F_SETFL, flags | O_NONBLOCK);
    }
    
    ESP_LOGI(TAG, "UDP server listening on port %d", UDP_PORT_TX);
    
    xTaskCreate(udp_server_task, "udp_server_task", 4096, NULL, 5, NULL);
}

static void udp_server_task(void *pvParameters)
{
    uint8_t rx_buffer[128];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (1) {
        int bytes_received = recvfrom(udp_server_sock, rx_buffer, sizeof(rx_buffer)-1, 0,
                                       (struct sockaddr*)&client_addr, &client_len);
        if (bytes_received > 0) {
            rx_buffer[bytes_received] = '\0';
            ESP_LOGI(TAG, "RX→TX: %d bytes de %s:%d", bytes_received,
                     inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            // Aqui você pode processar a resposta do RX
        } else if (bytes_received < 0) {
            // EAGAIN/EWOULDBLOCK é esperado em socket non-blocking
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ESP_LOGE(TAG, "recvfrom error: %d", errno);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ===== Enviar UDP =====
esp_err_t wifi_send_udp(const char *rx_ip, uint16_t port, const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0 || rx_ip == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Falha ao criar socket UDP para envio");
        return ESP_FAIL;
    }
    
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    
    int ret = inet_pton(AF_INET, rx_ip, &dest_addr.sin_addr);
    if (ret <= 0) {
        ESP_LOGE(TAG, "IP invalido: %s (inet_pton ret=%d)", rx_ip, ret);
        close(sock);
        return ESP_ERR_INVALID_ARG;
    }
    
    int sent = sendto(sock, data, len, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    close(sock);
    
    if (sent < 0) {
        ESP_LOGE(TAG, "Falha ao enviar UDP: errno=%d", errno);
        return ESP_FAIL;
    }
    
    if ((size_t)sent != len) {
        ESP_LOGW(TAG, "UDP enviado apenas %d/%d bytes", sent, len);
    } else {
        ESP_LOGI(TAG, "UDP enviado: %d bytes para %s:%d", sent, rx_ip, port);
    }
    
    return ESP_OK;
}
