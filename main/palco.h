#pragma once

#include "lvgl.h"
#include <stdbool.h>

/* Monta a interface steampunk (preset, status, menus) sobre a tela do palco. */
void palco_montar(lv_obj_t *tela);

/* Atualiza o indicador de WiFi (LED pulsante + texto ONLINE/OFFLINE). */
void palco_set_wifi(bool conectado);

/* Nome do preset atual (compartilhado com a tela 1). */
const char *palco_get_preset(void);

/* Alterna o amp mostrado (um por vez, sem sobreposicao). */
void app_trocar_amp(void);
int app_amp_atual(void);
void app_amp_ir_para(int i);

/* Estado dos 8 loops. */
bool palco_loop_ligado(int i);
void palco_toggle_loop(int i);

/* Registra callback chamado quando um loop muda: a tela 1 acende/apaga
 * o pedal correspondente. Ao registrar, recebe o estado atual de todos. */
void palco_registrar_sync(void (*cb)(int indice, bool ligado));

/* Registra callback para o nome do preset: a tela 1 apenas exibe,
 * a edicao acontece na tela 2. */
void palco_registrar_nome(void (*cb)(const char *nome));

/* Cor das letras escolhida em SISTEMA: mantem as duas telas iguais. */
void palco_registrar_cor(void (*cb)(uint32_t cor));

/* Cor de fundo conforme o canal do amp (verde/amarelo/vermelho/lilas). */
void palco_registrar_canal(void (*cb)(uint32_t cor));

/* Estilo das letras escolhido em SISTEMA: mantem as duas telas iguais. */
void palco_registrar_fonte(void (*cb)(const lv_font_t *fonte));
