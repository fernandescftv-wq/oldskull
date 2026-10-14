/* sd_fundos.c - fundos de palco carregados do SD card (JC4880P433C)
 * Reusa o decoder JPEG em hardware do ESP32-P4 (driver/jpeg_decode.h).
 * Os pinos do SD estao em sd_fundos.h (preencher antes de gravar).
 */
#include "sd_fundos.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_ldo_regulator.h"
#include "esp_heap_caps.h"
#include "driver/jpeg_decode.h"
#include "video_loop.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

static const char *TAG = "sd_fundos";

#define FUN_W      800
#define FUN_H      480
#define FUN_JPEG_MAX   (256 * 1024)
#define FUN_OUT_SIZE   ((FUN_W + 16) * (FUN_H + 16) * 2)

static jpeg_decoder_handle_t jpeg_dec = NULL;
static uint8_t  *jpeg_buf = NULL;
static uint8_t  *rgb_buf  = NULL;
static size_t    rgb_buf_size = 0;
static esp_ldo_channel_handle_t g_ldo_chan = NULL;  /* LDO4 -> VDD_IO_5 (pinos 39-48 do SD) */

/* prototipo (definida abaixo de sd_fundos_mount) */
static void copiar_fundo_spiffs_sd(const char *nome);

static lv_obj_t *g_fundo_img = NULL;
static bool      g_mounted = false;
static fundo_t   g_atual = FUNDO_VIDEO;

static const char *caminho_fundo(fundo_t t)
{
    /* as 3 imagens ficam no SPIFFS (gravadas junto do firmware em 0x410000) */
    switch (t) {
        case FUNDO_SD_1: return "/spiffs/images/steampunk_bg.jpg";
        case FUNDO_SD_2: return "/spiffs/images/moldura_bg.jpg";
        case FUNDO_SD_3: return "/spiffs/images/painel_bg.jpg";
        default: return NULL;
    }
}

esp_err_t sd_fundos_mount(void)
{
    if (g_mounted) return ESP_OK;

    /* LDO4 -> VDD_IO_5 alimenta os pinos 39-48 (slot SD). Vem DESLIGADO de
     * fabrica: precisa ligar explicitamente (3.3V) ou o SD nao responde (0x107). */
    if (!g_ldo_chan) {
        esp_ldo_channel_config_t ldo = {
            .chan_id = 4,
            .voltage_mv = 3300,
        };
        if (esp_ldo_acquire_channel(&ldo, &g_ldo_chan) != ESP_OK) {
            ESP_LOGE(TAG, "LDO4 (VDD_IO_5) falhou ao ligar");
        } else {
            ESP_LOGI(TAG, "LDO4 ligado: VDD_IO_5 = 3.3V (pinos 39-48)");
        }
    }

    /* Pinos do SD (GUITION JC4880P433C, slot 0 SDMMC 4-bit nativo):
     * CLK=43 CMD=44 D0=39 D1=40 D2=41 D3=42 (confirmado via doc do fabricante).
     * Habilita pull-up nos sinais (o slot pode nao ter interno forte). */
    gpio_pullup_en(GPIO_NUM_44);  /* CMD */
    gpio_pullup_en(GPIO_NUM_39);  /* D0 */
    gpio_pullup_en(GPIO_NUM_40);  /* D1 */
    gpio_pullup_en(GPIO_NUM_41);  /* D2 */
    gpio_pullup_en(GPIO_NUM_42);  /* D3 */

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    /* 4-bit (padrao da placa). host.flags=0 deixa 4-bit. */
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.clk = GPIO_NUM_43;
    slot.cmd = GPIO_NUM_44;
    slot.d0  = GPIO_NUM_39;
    slot.d1  = GPIO_NUM_40;
    slot.d2  = GPIO_NUM_41;
    slot.d3  = GPIO_NUM_42;
    slot.width = 4;

    esp_vfs_fat_sdmmc_mount_config_t mnt = {
        .format_if_mount_failed = true,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_card_t *card = NULL;
    esp_err_t err = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot, &mnt, &card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SD falhou (4-bit CMD=44 CLK=43 D0=39): %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "SD montado: %s", card->cid.name);
    g_mounted = true;

    /* copia fundos da SPIFFS pro SD (se ainda nao existirem) */
    copiar_fundo_spiffs_sd("fundo1.jpg");
    copiar_fundo_spiffs_sd("fundo2.jpg");
    return ESP_OK;
}

/* copia <nome> de /spiffs/<nome> para /sdcard/<nome> se nao existir no SD */
static void copiar_fundo_spiffs_sd(const char *nome)
{
    char dst[64];
    snprintf(dst, sizeof(dst), SD_MOUNT_POINT "/%s", nome);
    struct stat st;
    if (stat(dst, &st) == 0) return;   /* ja existe no SD */

    char src[48];
    snprintf(src, sizeof(src), "/spiffs/%s", nome);
    FILE *f = fopen(src, "rb");
    if (!f) return;   /* nao tem na spiffs */
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return; }

    uint8_t *buf = malloc(sz);
    if (!buf) { fclose(f); return; }
    if (fread(buf, 1, sz, f) != (size_t)sz) { fclose(f); free(buf); return; }
    fclose(f);

    FILE *o = fopen(dst, "wb");
    if (o) {
        fwrite(buf, 1, sz, o);
        fclose(o);
        ESP_LOGI(TAG, "copiado %s -> SD (%ld bytes)", nome, sz);
    }
    free(buf);
}

static bool iniciar_jpeg(void)
{
    if (jpeg_dec) return true;
    jpeg_decode_engine_cfg_t eng = { .intr_priority = 0, .timeout_ms = 400 };
    if (jpeg_new_decoder_engine(&eng, &jpeg_dec) != ESP_OK) {
        ESP_LOGE(TAG, "engine JPEG falhou"); return false;
    }
    jpeg_decode_memory_alloc_cfg_t tx = { .buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER };
    jpeg_decode_memory_alloc_cfg_t rx = { .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER };
    size_t txs = 0;
    jpeg_buf = jpeg_alloc_decoder_mem(FUN_JPEG_MAX, &tx, &txs);
    rgb_buf  = jpeg_alloc_decoder_mem(FUN_OUT_SIZE, &rx, &rgb_buf_size);
    if (!jpeg_buf || !rgb_buf) { ESP_LOGE(TAG, "sem memoria JPEG"); return false; }
    return true;
}

/* helper: tamanho do buffer de saida (RGB565 + margem 16) */
esp_err_t sd_fundos_mostrar(lv_obj_t *tela, fundo_t tipo)
{
    const char *path = caminho_fundo(tipo);
    if (!path) return ESP_ERR_INVALID_ARG;
    /* caminho_fundo ja decide SD vs SPIFFS conforme disponibilidade */
    FILE *f = fopen(path, "rb");
    if (!f) { ESP_LOGE(TAG, "fundo ausente: %s", path); return ESP_ERR_NOT_FOUND; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > FUN_JPEG_MAX) { fclose(f); return ESP_ERR_INVALID_SIZE; }
    if (!iniciar_jpeg()) { fclose(f); return ESP_ERR_NO_MEM; }

    if (fread(jpeg_buf, 1, sz, f) != (size_t)sz) { fclose(f); return ESP_ERR_INVALID_SIZE; }
    fclose(f);

    jpeg_decode_cfg_t cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
    };
    uint32_t saida = 0;
    esp_err_t err = jpeg_decoder_process(jpeg_dec, &cfg, jpeg_buf, (uint32_t)sz,
                                         rgb_buf, rgb_buf_size, &saida);
    if (err != ESP_OK || saida == 0) {
        ESP_LOGE(TAG, "decode JPEG falhou: %s", esp_err_to_name(err));
        return err;
    }

    /* cria/atualiza o lv_img do fundo */
    static lv_img_dsc_t dsc;
    dsc.header.w = FUN_W;
    dsc.header.h = FUN_H;
    dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    dsc.data_size = FUN_W * FUN_H * 2;
    dsc.data = rgb_buf;

    if (!g_fundo_img) {
        g_fundo_img = lv_img_create(tela);
        lv_obj_clear_flag(g_fundo_img, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(g_fundo_img, LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_img_set_src(g_fundo_img, &dsc);
    lv_obj_set_size(g_fundo_img, FUN_W, FUN_H);
    lv_obj_align(g_fundo_img, LV_ALIGN_CENTER, 0, 0);
    /* fundo ATRAS da UI (menus/amp/pedais) e atras do video: a UI fica visivel */
    lv_obj_move_background(g_fundo_img);
    lv_obj_clear_flag(g_fundo_img, LV_OBJ_FLAG_HIDDEN);
    video_hide(true);   /* nao desenha o video por cima; loop segue processando */
    g_atual = tipo;
    ESP_LOGI(TAG, "fundo SD mostrado: %s", path);
    return ESP_OK;
}

void sd_fundos_esconder(void)
{
    if (g_fundo_img) lv_obj_add_flag(g_fundo_img, LV_OBJ_FLAG_HIDDEN);
    video_hide(false);   /* video de volta; nao pausou, so estava escondido */
    g_atual = FUNDO_VIDEO;
}

fundo_t sd_fundos_atual(void) { return g_atual; }
