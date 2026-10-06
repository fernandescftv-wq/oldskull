#pragma once

#include "lvgl.h"

/* Tela usada pelo splash — o app apaga depois de trocar de tela. */
extern lv_obj_t *splash_tela;

/* Toca a animacao de abertura (frames JPEG do SPIFFS, decodificados por HW).
 * Bloqueante: retorna quando a animacao termina. */
void splash_play(void);
