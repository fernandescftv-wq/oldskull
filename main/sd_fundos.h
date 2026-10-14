/* sd_fundos.h - fundos de palco carregados do SD card (JC4880P433C)
 *
 * Tudo de imagem vai pro SD (decisao do usuario 21/08/2026).
 * Os pinos do SD card PRECISAM ser preenchidos conforme o hardware.
 * O BSP NAO documenta os pinos do SD (bsp_sdcard_mount e TODO vazio).
 * Preencha SD_CMD_GPIO / SD_CLK_GPIO / SD_D0_GPIO abaixo antes de gravar,
 * senao o boot pode travar (SDMMC conflitando com display/touch).
 *
 * Fundos disponiveis no SD (copiar de /tmp/fundos_sd/):
 *   /sdcard/fundo1.jpg  (steampunk 1, 800x480)
 *   /sdcard/fundo2.jpg  (steampool 2, 800x480)
 */

#pragma once
#include "lvgl.h"
#include "esp_err.h"
#include "driver/sdmmc_defs.h"

/* === PINOS DO SD CARD (SDMMC slot 0, 1-bit) ===
 * Do ESP32-P4 Datasheet v0.7 sec 4.2.2.14 / Pin Assignment:
 *   SD1 (slot 0) multiplexado com GPIO39-GPIO48.
 *   CMD = GPIO44 (SD1_CCMD_PAD)
 *   CLK = GPIO43 (SD1_CCLK_PAD)
 *   D0  = GPIO39 (SD1_CDATA0_PAD)
 * GPIO48 e' I2S_DSIN (nao usar). 39/43/44 estao livres no BSP. */
#define SD_CMD_GPIO     GPIO_NUM_44
#define SD_CLK_GPIO     GPIO_NUM_43
#define SD_D0_GPIO      GPIO_NUM_39

#define SD_MOUNT_POINT  "/sdcard"

/* tipos de fundo */
typedef enum {
    FUNDO_VIDEO = 0,   /* video em loop (atual, do SPIFFS) */
    FUNDO_SD_1  = 1,   /* steampunk_bg.jpg (SPIFFS) */
    FUNDO_SD_2  = 2,   /* moldura_bg.jpg (SPIFFS) */
    FUNDO_SD_3  = 3,   /* painel_bg.jpg (SPIFFS) */
    FUNDO_MAX
} fundo_t;

/* monta o SD (chamar uma vez no boot). retorna ESP_OK se ok */
esp_err_t sd_fundos_mount(void);

/* carrega um fundo do SD e desenha por cima do video (ou esconde video).
 * tela = tela atual (telas[0]).
 * tipo = FUNDO_SD_1 / FUNDO_SD_2.
 * retorna ESP_OK se carregou e desenhou. */
esp_err_t sd_fundos_mostrar(lv_obj_t *tela, fundo_t tipo);

/* volta pro video (esconde fundo do SD) */
void sd_fundos_esconder(void);

/* retorna o fundo atualmente ativo */
fundo_t sd_fundos_atual(void);
