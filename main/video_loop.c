/* video_loop.c - video em loop fullscreen (800x480) a partir de frames JPEG
 * no SPIFFS, decodificados pelo motor JPEG em hardware do ESP32-P4.
 * Inspirado no splash animado, porem em loop continuo e exibido numa tela
 * do app (tela 2), nao numa tela propria de boot. */

#include "video_loop.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"
#include "driver/jpeg_decode.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "video";

/* 800x480 = tela inteira. Ambos multiplos de 16 (50x16 e 30x16), exigido
 * pelo motor JPEG do P4. O video 1440x1440 e esticado para 800x480. */
#define VID_W           800
#define VID_H           480
#define VID_FPS         24
#define VID_MAX_JPEG    (64 * 1024)
#define VID_OUT_SIZE    ((VID_W + 16) * (VID_H + 16) * 2)

/* Cache dos frames na PSRAM (leitura unica do SPIFFS, sem I/O no loop). */
#define VID_MAX_CACHE   240
#define VID_CACHE_MAX_BYTES (8 * 1024 * 1024)

static jpeg_decoder_handle_t jpeg_dec = NULL;
static uint8_t  *jpeg_buf = NULL;
static uint8_t  *rgb_buf  = NULL;
static size_t    rgb_buf_size = 0;
static int       cache_n = 0;
static uint8_t  *cache_buf[VID_MAX_CACHE];
static uint32_t  cache_len[VID_MAX_CACHE];

static lv_obj_t *g_img = NULL;          /* lv_img da tela */
static lv_img_dsc_t g_frame_dsc;        /* descreve rgb_buf */
static bool     g_rodando = false;
static bool     g_pausado  = false;   /* true = segura o frame atual (congelado) */
static int      g_fps = VID_FPS;        /* velocidade ajustavel pelo menu */

/* ajusta a velocidade do video (fps). quanto menor, mais lento. */
void video_set_fps(int fps)
{
    if (fps < 1) fps = 1;
    if (fps > 60) fps = 60;
    g_fps = fps;
    ESP_LOGI(TAG, "video fps -> %d", g_fps);
}

/* congela (true) ou retoma (false) o video, segurando o frame atual */
void video_toggle_pause(void)
{
    g_pausado = !g_pausado;
    ESP_LOGI(TAG, "video %s", g_pausado ? "congelado" : "retomado");
}

static void carregar_cache(void)
{
    FILE *f = fopen("/spiffs/frames.bin", "rb");
    if (!f) { ESP_LOGE(TAG, "frames.bin ausente"); return; }
    uint32_t n = 0;
    if (fread(&n, 1, 4, f) != 4 || n == 0 || n > VID_MAX_CACHE) {
        ESP_LOGE(TAG, "indice invalido (n=%lu)", (unsigned long)n);
        fclose(f); return;
    }
    uint32_t *idx = malloc(n * 8);
    if (!idx) { fclose(f); return; }
    if (fread(idx, 1, n * 8, f) != n * 8) { free(idx); fclose(f); return; }

    size_t total = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t off = idx[i * 2], len = idx[i * 2 + 1];
        if (len == 0 || len > VID_MAX_JPEG) break;
        if (total + len > VID_CACHE_MAX_BYTES) break;
        uint8_t *b = heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
        if (!b) break;
        if (fseek(f, off, SEEK_SET) != 0 || fread(b, 1, len, f) != len) {
            free(b); break;
        }
        cache_buf[i] = b;
        cache_len[i] = len;
        cache_n = i + 1;
        total += len;
        if ((i & 15) == 15) vTaskDelay(1);
    }
    free(idx);
    fclose(f);
    ESP_LOGI(TAG, "cache: %d/%lu frames, %u KB", cache_n, (unsigned long)n, (unsigned)(total/1024));
}

static bool iniciar_jpeg(void)
{
    jpeg_decode_engine_cfg_t eng = { .intr_priority = 0, .timeout_ms = 400 };
    if (jpeg_new_decoder_engine(&eng, &jpeg_dec) != ESP_OK) {
        ESP_LOGE(TAG, "engine JPEG falhou"); return false;
    }
    jpeg_decode_memory_alloc_cfg_t tx = { .buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER };
    jpeg_decode_memory_alloc_cfg_t rx = { .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER };
    size_t txs = 0;
    jpeg_buf = jpeg_alloc_decoder_mem(VID_MAX_JPEG, &tx, &txs);
    rgb_buf  = jpeg_alloc_decoder_mem(VID_OUT_SIZE, &rx, &rgb_buf_size);
    if (!jpeg_buf || !rgb_buf) { ESP_LOGE(TAG, "sem memoria JPEG"); return false; }
    return true;
}

static bool decodificar_frame(int n)
{
    int i = n - 1;
    if (i < 0 || i >= cache_n || !cache_buf[i]) return false;
    memcpy(jpeg_buf, cache_buf[i], cache_len[i]);
    jpeg_decode_cfg_t cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
    };
    uint32_t saida = 0;
    esp_err_t err = jpeg_decoder_process(jpeg_dec, &cfg, jpeg_buf, cache_len[i],
                                         rgb_buf, rgb_buf_size, &saida);
    return (err == ESP_OK && saida > 0);
}

static void video_task(void *arg)
{
    int falhas = 0;
    int frame = 1;   /* 1-based, wraparound */
    while (g_rodando) {
        if (g_pausado) {
            /* congelado: segura o frame atual, so cede CPU */
            vTaskDelay(30 / portTICK_PERIOD_MS);
            continue;
        }
        int64_t frame_us = 1000000 / g_fps;   /* velocidade ajustavel */
        int64_t t0 = esp_timer_get_time();
        bool ok = decodificar_frame(frame);
        if (ok) {
            if (bsp_display_lock(20)) {
                lv_img_set_src(g_img, NULL);
                lv_img_set_src(g_img, &g_frame_dsc);
                lv_obj_invalidate(g_img);
                bsp_display_unlock();
            }
            falhas = 0;
        } else if (++falhas >= 5) {
            ESP_LOGE(TAG, "5 frames falharam, parando video");
            break;
        }
        frame++;
        if (frame > cache_n) frame = 1;   /* loop */
        /* respeita o tempo do frame (cede CPU se decodificou rapido) */
        int64_t g = esp_timer_get_time() - t0;
        int64_t dormir = frame_us - g;
        if (dormir > 0) vTaskDelay(dormir / 1000);
        else vTaskDelay(1);
    }
    g_rodando = false;
    vTaskDelete(NULL);
}

int video_loop_start(lv_obj_t *tela)
{
    if (g_rodando) return 0;

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs", .partition_label = "spiffs",
        .max_files = 4, .format_if_mount_failed = false,
    };
    if (esp_vfs_spiffs_register(&conf) != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS falhou"); return -1;
    }
    if (!iniciar_jpeg()) { esp_vfs_spiffs_unregister("splash"); return -1; }

    carregar_cache();
    if (cache_n == 0) { ESP_LOGE(TAG, "cache vazio"); return -1; }

    g_frame_dsc.header.w  = VID_W;
    g_frame_dsc.header.h  = VID_H;
    g_frame_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    /* stride real do decoder JPEG: linhas alinhadas em (W+16) * 2 bytes.
       Sem isso o LVGL 9 assume stride = W*2 e desenha deslocado (linhas fininhas). */
    g_frame_dsc.data_size = (VID_W + 16) * (VID_H + 16) * 2;
    g_frame_dsc.data      = rgb_buf;

    g_img = lv_img_create(tela);
    lv_obj_clear_flag(g_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(g_img, VID_W, VID_H);
    lv_obj_align(g_img, LV_ALIGN_CENTER, 0, 0);
    lv_img_set_pivot(g_img, 0, 0);

    g_rodando = true;
    if (xTaskCreate(video_task, "video", 6 * 1024, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "falha criar task video");
        g_rodando = false;
        return -1;
    }
    ESP_LOGI(TAG, "video em loop: %d frames @ %d fps", cache_n, VID_FPS);
    return 0;
}
