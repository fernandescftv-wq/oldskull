/* c6_ota.c - grava o firmware do C6 (bridge ESP-NOW) embutido no app do P4.
 *
 * O C6 vem junto do P4 no board (jc4880p443c) e é o dono do rádio WiFi.
 * O P4 (host) grava o C6 via CP-OTA do esp_hosted (RPC begin/write/end).
 * O binário `c6_fw.bin` é embutido pelo CMake (EMBED_FILES) e este módulo
 * faz o flash UMA VEZ (flag NVS), na sequência do wifi_init_ap.
 */
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "eh_host_cp_ota.h"

static const char *TAG = "c6_ota";

/* Símbolos gerados pelo EMBED_FILES main/c6_fw.bin */
extern const uint8_t c6_fw_bin_start[] asm("_binary_c6_fw_bin_start");
extern const uint8_t c6_fw_bin_end[]   asm("_binary_c6_fw_bin_end");

#define OTA_CHUNK 1500   /* <= EH_RPC_OTA_CHUNK_MAX (1536) */
#define NVS_KEY   "c6_fw_v14"  /* v14: força OTA - confirma RX_MAC EC:E3:34:14:8B:C4 no C6 */
#define ESP_SKIP  2      /* convenienca local: ya estaba actualizado */

/* retorna ESP_OK se flasheou o C6, ESP_SKIP se já estava atualizado */
esp_err_t c6_ota_update_once(void)
{
    uint32_t len = (uint32_t)(c6_fw_bin_end - c6_fw_bin_start);
    if (len < 64) {
        ESP_LOGE(TAG, "c6_fw.bin inválido (len=%lu)", (unsigned long)len);
        return ESP_FAIL;
    }

    /* flag NVS: só flashea uma vez. A NVS precisa estar ativa — o c6_ota
       roda ANTES do wifi_init_ap (que era quem a inicializava). */
    nvs_flash_init();
    nvs_handle_t h;
    if (nvs_open("c6ota", NVS_READWRITE, &h) == ESP_OK) {
        uint8_t done = 0;
        if (nvs_get_u8(h, NVS_KEY, &done) == ESP_OK && done) {
            nvs_close(h);
            ESP_LOGI(TAG, "C6 já atualizado (skip)");
            return ESP_SKIP;
        }
        nvs_close(h);
    }

    ESP_LOGW(TAG, "C6 OTA: begin (len=%lu)...", (unsigned long)len);
    esp_err_t r = eh_host_cp_ota_begin();
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "ota_begin: %s", esp_err_to_name(r));
        return r;
    }

    const uint8_t *p = c6_fw_bin_start;
    uint32_t left = len;
    while (left > 0) {
        uint32_t n = left > OTA_CHUNK ? OTA_CHUNK : left;
        r = eh_host_cp_ota_write(p, n);
        if (r != ESP_OK) {
            ESP_LOGE(TAG, "ota_write @%lu: %s",
                     (unsigned long)(len - left), esp_err_to_name(r));
            return r;
        }
        p += n; left -= n;
        if ((len - left) % (OTA_CHUNK * 20) == 0)
            ESP_LOGI(TAG, "  ... %lu/%lu", (unsigned long)(len - left), (unsigned long)len);
    }

    r = eh_host_cp_ota_end();
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "ota_end: %s", esp_err_to_name(r));
        return r;
    }

    /* activate: requer CP v2.6.0+; se falhar, o C6 ainda aplica no reboot */
    r = eh_host_cp_ota_activate();
    if (r == ESP_OK)
        ESP_LOGI(TAG, "ota_activate OK — C6 reinicia com o bridge ESP-NOW");
    else
        ESP_LOGW(TAG, "ota_activate: %s (ok se CP antigo)", esp_err_to_name(r));

    /* marca como feito apenas se begin/write/end passaram */
    if (nvs_open("c6ota", NVS_READWRITE, &h) == ESP_OK) {
        uint8_t done = 1;
        nvs_set_u8(h, NVS_KEY, done);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGW(TAG, "C6 OTA concluído — reiniciando para o C6 aplicar...");
    return ESP_OK;
}