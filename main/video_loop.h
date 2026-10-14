/* video_loop.h - interface do video_loop.c */
#pragma once

#include "lvgl.h"

/* Cria um player de video em loop na tela indicada: decodifica os frames
 * JPEG do SPIFFS (motor JPEG em hardware do P4) continuamente e os mostra
 * fullscreen (800x480). A tela pai deve estar criada.
 * Retorna 0 em sucesso, -1 em falha. */
int video_loop_start(lv_obj_t *tela);

/* ajusta a velocidade do video em loop (fps). 1=lento, 24=normal, 60=rapido. */
void video_set_fps(int fps);

/* congela/retoma o video (toggle): segura o frame atual quando congelado. */
void video_toggle_pause(void);

/* mostra/esconde o video de fundo (usado pelo SKIN de fundo estatico). */
void video_hide(bool hid);
