/* splash.c - splash animado a partir de frames JPEG no SPIFFS,
 * decodificados pelo motor JPEG em hardware do ESP32-P4. */

#include "splash.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"
#include "driver/jpeg_decode.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>

static const char *TAG = "splash";

lv_obj_t *splash_tela = NULL;

/* 512x480: mostra 53% da altura do video, o maximo util antes da imagem ficar
 * estreita demais. Barras laterais de 144px. Ambos multiplos de 16 (32x16 e
 * 30x16), exigido pelo motor JPEG do P4. Sem distorcao. */
#define SPLASH_W        512
#define SPLASH_H        480
/* 24fps: a 30fps o LVGL nao acompanha e o idle sofre (task watchdog). */
#define SPLASH_FPS      24
/* Duracao total do splash em ms: o player respeita este tempo pulando frames
 * se o hardware nao acompanhar, em vez de esticar a animacao. */
#define SPLASH_DUR_MS   10000
#define SPLASH_MAX_JPEG (64 * 1024)
/* folga extra caso o HW arredonde para cima */
#define SPLASH_OUT_SIZE ((SPLASH_W + 16) * (SPLASH_H + 16) * 2)

static jpeg_decoder_handle_t jpeg_dec = NULL;
static uint8_t  *jpeg_buf = NULL;      /* JPEG comprimido (alinhado p/ DMA) */
static uint8_t  *rgb_buf  = NULL;      /* saida RGB565 */
static size_t    rgb_buf_size = 0;
static int       total_frames = 0;
static uint32_t  t_io_us = 0;      /* tempo acumulado de leitura do SPIFFS */

/* Cache dos frames na PSRAM: o fopen/fclose do SPIFFS custava ~120ms por frame
 * (o decode puro leva apenas 2,7ms). Carregando tudo na RAM uma unica vez, a
 * reproducao fica livre de I/O.
 * Limite de 6MB: a placa tem 16MB de PSRAM e o LVGL ja usa ~1,5MB de
 * framebuffers, alem das imagens estaticas das telas. */
#define SPLASH_MAX_CACHE 240
#define SPLASH_CACHE_MAX_BYTES (7 * 1024 * 1024)
static uint8_t  *cache_buf[SPLASH_MAX_CACHE];
static uint32_t  cache_len[SPLASH_MAX_CACHE];
static int       cache_n = 0;

static void carregar_cache(void)
{
    /* Um unico arquivo com indice: o fopen do SPIFFS custa ~180ms, entao abrir
     * 120 arquivos levava 21s. Com um blob unico e uma leitura sequencial,
     * o carregamento cai para o tempo de transferencia. */
    int64_t t0 = esp_timer_get_time();

    FILE *f = fopen("/splash/frames.bin", "rb");
    if (!f) { ESP_LOGE(TAG, "frames.bin ausente"); return; }

    uint32_t n = 0;
    if (fread(&n, 1, 4, f) != 4 || n == 0 || n > SPLASH_MAX_CACHE) {
        ESP_LOGE(TAG, "indice invalido (n=%lu)", (unsigned long)n);
        fclose(f);
        return;
    }

    /* le a tabela de offsets de uma vez */
    uint32_t *idx = malloc(n * 8);
    if (!idx) { fclose(f); return; }
    if (fread(idx, 1, n * 8, f) != n * 8) { free(idx); fclose(f); return; }

    size_t total = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t off = idx[i * 2], len = idx[i * 2 + 1];
        if (len == 0 || len > SPLASH_MAX_JPEG) break;
        if (total + len > SPLASH_CACHE_MAX_BYTES) break;

        uint8_t *b = heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
        if (!b) break;
        if (fseek(f, off, SEEK_SET) != 0 || fread(b, 1, len, f) != len) {
            free(b);
            break;
        }
        cache_buf[i] = b;
        cache_len[i] = len;
        cache_n = i + 1;
        total += len;

        if ((i & 15) == 15) vTaskDelay(1);   /* cede CPU periodicamente */
    }
    free(idx);
    fclose(f);

    ESP_LOGI(TAG, "cache: %d/%lu frames, %u KB em %lldms",
             cache_n, (unsigned long)n, (unsigned)(total / 1024),
             (esp_timer_get_time() - t0) / 1000);
}

static void liberar_cache(void)
{
    for (int i = 0; i < cache_n; i++) {
        if (cache_buf[i]) { free(cache_buf[i]); cache_buf[i] = NULL; }
    }
    cache_n = 0;
}

static bool montar_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/splash",
        .partition_label = "splash",
        .max_files = 4,
        .format_if_mount_failed = false,
    };
    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS falhou: %s", esp_err_to_name(err));
        return false;
    }
    size_t total = 0, usado = 0;
    esp_spiffs_info("splash", &total, &usado);
    ESP_LOGI(TAG, "SPIFFS: %u KB usados de %u KB", usado / 1024, total / 1024);

    /* conta os frames pelo indice do blob (evita varrer o diretorio) */
    FILE *fi = fopen("/splash/frames.bin", "rb");
    if (fi) {
        uint32_t n = 0;
        if (fread(&n, 1, 4, fi) == 4 && n > 0 && n <= SPLASH_MAX_CACHE) {
            total_frames = (int)n;
        }
        fclose(fi);
    }
    ESP_LOGI(TAG, "frames no blob: %d", total_frames);
    return total_frames > 0;
}

static bool iniciar_jpeg(void)
{
    jpeg_decode_engine_cfg_t eng = {
        .intr_priority = 0,
        .timeout_ms = 400,
    };
    if (jpeg_new_decoder_engine(&eng, &jpeg_dec) != ESP_OK) {
        ESP_LOGE(TAG, "engine JPEG falhou");
        return false;
    }

    jpeg_decode_memory_alloc_cfg_t tx_cfg = { .buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER };
    jpeg_decode_memory_alloc_cfg_t rx_cfg = { .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER };
    size_t tx_size = 0;

    jpeg_buf = (uint8_t *)jpeg_alloc_decoder_mem(SPLASH_MAX_JPEG, &tx_cfg, &tx_size);
    rgb_buf  = (uint8_t *)jpeg_alloc_decoder_mem(SPLASH_OUT_SIZE, &rx_cfg, &rgb_buf_size);

    if (!jpeg_buf || !rgb_buf) {
        ESP_LOGE(TAG, "sem memoria para buffers JPEG");
        return false;
    }
    ESP_LOGI(TAG, "buffers ok (in %u / out %u)", tx_size, rgb_buf_size);
    return true;
}

static void liberar(void)
{
    liberar_cache();
    if (jpeg_dec) { jpeg_del_decoder_engine(jpeg_dec); jpeg_dec = NULL; }
    if (jpeg_buf) { free(jpeg_buf); jpeg_buf = NULL; }
    if (rgb_buf)  { free(rgb_buf);  rgb_buf  = NULL; }
    esp_vfs_spiffs_unregister("splash");
}

/* decodifica um frame para rgb_buf (a partir do cache na PSRAM) */
static bool decodificar_frame(int n)
{
    int i = n - 1;
    if (i < 0 || i >= cache_n || !cache_buf[i]) return false;

    /* copia para o buffer alinhado do decoder (exigencia de DMA) */
    memcpy(jpeg_buf, cache_buf[i], cache_len[i]);

    jpeg_decode_cfg_t cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
        /* O painel/LVGL consomem RGB565 em byte order little-endian (foi o que
         * acertou as imagens estaticas). O decoder em modo RGB entrega na ordem
         * oposta, deixando as cores trocadas; BGR compensa. */
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
    };
    uint32_t saida = 0;
    esp_err_t err = jpeg_decoder_process(jpeg_dec, &cfg, jpeg_buf, cache_len[i],
                                         rgb_buf, rgb_buf_size, &saida);
    return (err == ESP_OK && saida > 0);
}

void splash_play(void)
{
    if (!montar_spiffs()) return;
    if (!iniciar_jpeg())  { liberar(); return; }

    /* Desenha via LVGL: o painel fisico e 480x800 (portrait) e a rotacao de 90
     * graus e aplicada pelo LVGL. Escrever direto no painel com largura 800
     * estourava a linha de 480 e embaralhava a imagem. */
    lv_obj_t *tela = lv_obj_create(NULL);
    lv_obj_clear_flag(tela, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(tela, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tela, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tela, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(tela, 0, LV_PART_MAIN);

    static lv_img_dsc_t frame_dsc;
    frame_dsc.header.always_zero = 0;
    frame_dsc.header.w  = SPLASH_W;
    frame_dsc.header.h  = SPLASH_H;
    frame_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    frame_dsc.data_size = SPLASH_W * SPLASH_H * 2;
    frame_dsc.data      = rgb_buf;

    lv_obj_t *img = lv_img_create(tela);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(img);
    lv_scr_load(tela);

    ESP_LOGI(TAG, "tocando %d frames em %dms (pula se atrasar)", total_frames, SPLASH_DUR_MS);

    /* pre-carrega tudo na PSRAM: sem I/O durante a reproducao */
    carregar_cache();
    if (cache_n == 0) { ESP_LOGE(TAG, "cache vazio"); liberar(); return; }

    /* Duracao fixa: em vez de esperar por cada frame (o que esticava o video
     * quando o hardware nao acompanhava), avanca pelo TEMPO decorrido e pula
     * frames se preciso. O splash sempre dura SPLASH_DUR_MS. */
    const int64_t t_zero = esp_timer_get_time();
    int falhas = 0, desenhados = 0, pulados = 0, ultimo = -1;
    uint32_t soma_dec = 0, soma_draw = 0, amostras = 0;

    while (1) {
        int64_t decorrido = esp_timer_get_time() - t_zero;
        if (decorrido >= (int64_t)SPLASH_DUR_MS * 1000) break;

        /* frame correspondente ao instante atual (dentro do que foi cacheado) */
        int n = 1 + (int)((decorrido * cache_n) / ((int64_t)SPLASH_DUR_MS * 1000));
        if (n > cache_n) break;
        if (n == ultimo) { vTaskDelay(1); continue; }   /* nada novo: cede CPU */
        if (ultimo >= 0 && n > ultimo + 1) pulados += n - ultimo - 1;
        ultimo = n;

        int64_t m0 = esp_timer_get_time();
        bool ok = decodificar_frame(n);
        int64_t m1 = esp_timer_get_time();

        if (ok) {
            if (bsp_display_lock(20)) {
                lv_img_set_src(img, NULL);      /* invalida cache do decoder */
                lv_img_set_src(img, &frame_dsc);
                lv_obj_invalidate(img);
                bsp_display_unlock();
                desenhados++;
            }
        } else if (++falhas >= 5) {
            ESP_LOGE(TAG, "5 frames falharam, abortando splash");
            break;
        }
        int64_t m2 = esp_timer_get_time();
        soma_dec  += (uint32_t)(m1 - m0) / 1000;
        soma_draw += (uint32_t)(m2 - m1) / 1000;
        amostras++;

        vTaskDelay(1);   /* cede CPU ao idle */
    }

    uint32_t ms = (uint32_t)((esp_timer_get_time() - t_zero) / 1000);
    ESP_LOGI(TAG, "splash: %dms | %d desenhados, %d pulados | decode %lums (io %lums) draw %lums",
             (int)ms, desenhados, pulados,
             (unsigned long)(amostras ? soma_dec / amostras : 0),
             (unsigned long)(amostras ? (t_io_us / 1000) / amostras : 0),
             (unsigned long)(amostras ? soma_draw / amostras : 0));
    splash_tela = tela;
    liberar();
}
