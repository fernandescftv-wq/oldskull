/* palco.c - tela principal estilo tablet steampunk sobre o cenario OLDSKULL.
 * Layout mapeado sobre a imagem de fundo (800x480):
 *   banner OLDSKULL  y  10..90    -> nome do preset
 *   medidores        y 145, x445/555 (r=50)
 *   plataforma livre y 260..440   -> menus e submenus
 *   laterais         x 0..100 e 700..800 -> botoes
 */

#include "palco.h"
#include "bsp/esp-bsp.h"
#include "video_loop.h"
#include "sd_fundos.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "palco";

/* ---- paleta steampunk (latao, cobre, ambar) ---- */
#define OURO_CLARO  0xF3C969   /* texto principal */
#define OURO        0xC9922B   /* bordas, molduras */
#define OURO_ESCURO 0x8A6420   /* detalhes, sombra */
#define FUNDO_PAN   0x120C06   /* painel translucido */
#define VERDE_LED   0x4CD964   /* status conectado */
#define VERMELHO    0xE04A3F   /* status desconectado */
#define AMBAR       0xFFA030   /* destaque ativo */

/* paleta copiada da pagina web aprovada (oldskull_web/index.html) */
#define WEB_OURO        0xF3C969   /* --ouro: bordas e destaques */
#define WEB_OURO_CLARO  0xF7DD9A   /* --ouro-claro: textos */
#define WEB_K           0xE6D3A8   /* item .k */
#define WEB_HINT        0xB8A898   /* .hint */
#define WEB_PAINEL      0x0E0B08   /* --painel rgba(14,11,8,.94) */

/* nome do preset compartilhado com o resto do app */
static char  preset_nome[24] = "PRESET";
static bool  wifi_ok = false;

static lv_obj_t *lbl_preset  = NULL;
static lv_obj_t *lbl_wifi    = NULL;
static lv_obj_t *led_wifi    = NULL;
static lv_obj_t *painel_menu = NULL;
static lv_obj_t *teclado_box = NULL;
static lv_obj_t *ta_nome     = NULL;

/* grupo com toda a interface: a engrenagem mostra/oculta para deixar o
 * cenario limpo, sem destruir os objetos. */
static lv_obj_t *grupo_ui    = NULL;
static bool      ui_visivel  = false;  /* menus ocultos ate tocar a engrenagem */

/* fontes vitorianas: Cinzel (romana classica) nos menus,
 * Playfair Display (didone sec. XIX) no nome do preset */
LV_FONT_DECLARE(fonte_cinzel_16);
LV_FONT_DECLARE(fonte_cinzel_20);
LV_FONT_DECLARE(fonte_cinzel_22);
LV_FONT_DECLARE(fonte_cinzel_26);
LV_FONT_DECLARE(fonte_playfair_30);
LV_FONT_DECLARE(fonte_playfair_40);
/* versoes Black (mais gordinhas) usadas na interface */
LV_FONT_DECLARE(fonte_preset_42);
LV_FONT_DECLARE(fonte_menu_22);
LV_FONT_DECLARE(fonte_menu_26);
/* trio selecionavel em SISTEMA -> Estilo das letras */
LV_FONT_DECLARE(fonte_manu_48);     /* Italianno manuscrita */
LV_FONT_DECLARE(fonte_manu_28);
LV_FONT_DECLARE(fonte_play_42);     /* Playfair Display */
LV_FONT_DECLARE(fonte_play_24);
LV_FONT_DECLARE(fonte_cinz_40);     /* Cinzel */

/* estilos de letra disponiveis: um par (titulo grande, item de menu) */
typedef struct {
    const char      *nome;
    const lv_font_t *grande;     /* nome do preset */
    const lv_font_t *media;      /* titulos e itens de submenu */
} estilo_letra_t;

static const estilo_letra_t estilos[] = {
    { "Manuscrita", &fonte_manu_48,   &fonte_manu_28  },
    { "Playfair",   &fonte_play_42,   &fonte_play_24  },
    { "Cinzel",     &fonte_cinz_40,   &fonte_menu_22  },
    { "Robusta",    &fonte_preset_42, &fonte_menu_22  },
};
#define QTD_ESTILOS (sizeof(estilos) / sizeof(estilos[0]))
static int estilo_atual = 3;        /* comeca na Robusta (peso que agradou) */

/* cores de fundo da barra do preset conforme o canal do amp.
 * Tons saturados: a barra usa opacidade alta para a cor aparecer. */
static const uint32_t cor_canal[4] = {
    0x1F7A34,   /* canal 1: verde vivo */
    0xB8901A,   /* canal 2: amarelo dourado */
    0xA82418,   /* canal 3: vermelho forte */
    0x7A3FA8,   /* boost:   lilas */
};

static uint32_t cor_atual = 0xF3C969;   /* cor das letras, escolhida em SISTEMA */

/* ---- helpers de estilo ---- */

static void estilo_painel(lv_obj_t *o)
{
    lv_obj_set_style_bg_color(o, lv_color_hex(FUNDO_PAN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(o, lv_color_hex(OURO), LV_PART_MAIN);
    lv_obj_set_style_border_opa(o, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_radius(o, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

/* ---- estilo web (replica .painel/.item/.hint da pagina aprovada) ---- */

static lv_obj_t *texto(lv_obj_t *pai, const char *txt,
                       const lv_font_t *fonte, uint32_t cor);   /* prototipo */

/* painel: fundo --painel 94%, borda 2px --ouro, raio 12, sombra 40 */
static void estilo_painel_web(lv_obj_t *o)
{
    lv_obj_set_style_bg_color(o, lv_color_hex(WEB_PAINEL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, 242, LV_PART_MAIN);   /* .94 */
    lv_obj_set_style_border_width(o, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(o, lv_color_hex(WEB_OURO), LV_PART_MAIN);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(o, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(o, 0, LV_PART_MAIN);   /* sem sombra: shadow_40 + sw_rotate esgotava o refresh */
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

/* item .item: linha separadora rgba(243,201,105,.15) entre k e v */
static lv_obj_t *item_web(lv_obj_t *pai, int y, int largura,
                          const char *k, const char *v)
{
    lv_obj_t *it = lv_obj_create(pai);
    lv_obj_remove_style_all(it);
    lv_obj_set_size(it, largura, 34);
    lv_obj_set_pos(it, 14, y);
    lv_obj_set_style_border_width(it, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(it, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(it, lv_color_hex(OURO_CLARO), LV_PART_MAIN);
    lv_obj_set_style_border_opa(it, LV_OPA_30, LV_PART_MAIN);
    lv_obj_add_flag(it, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(it, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lk = texto(it, k, &fonte_cinzel_16, WEB_K);
    lv_obj_align(lk, LV_ALIGN_LEFT_MID, 8, 0);
    if (v) {
        lv_obj_t *lv_ = texto(it, v, &fonte_cinzel_16, WEB_OURO);
        lv_obj_align(lv_, LV_ALIGN_RIGHT_MID, -8, 0);
    }
    return it;
}

/* toggle .toggle: pill 38x20, ouro quando on, bolinha marfim */
static lv_obj_t *toggle_web(lv_obj_t *pai, int x, int y, bool on)
{
    lv_obj_t *tg = lv_obj_create(pai);
    lv_obj_remove_style_all(tg);
    lv_obj_set_size(tg, 38, 20);
    lv_obj_set_pos(tg, x, y);
    lv_obj_set_style_radius(tg, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(tg, lv_color_hex(on ? WEB_OURO : 0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(tg, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(tg, lv_color_hex(WEB_OURO), LV_PART_MAIN);
    lv_obj_set_style_border_opa(tg, on ? LV_OPA_COVER : LV_OPA_40, LV_PART_MAIN);
    lv_obj_add_flag(tg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(tg, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bola = lv_obj_create(tg);
    lv_obj_remove_style_all(bola);
    lv_obj_set_size(bola, 16, 16);
    lv_obj_align(bola, LV_ALIGN_LEFT_MID, on ? 19 : 3, 0);
    lv_obj_set_style_radius(bola, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bola, lv_color_hex(0xFAF6EA), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bola, LV_OPA_COVER, LV_PART_MAIN);
    return tg;
}

/* atualiza o visual do toggle sem recriar */
static void toggle_web_set(lv_obj_t *tg, bool on)
{
    lv_obj_set_style_bg_color(tg, lv_color_hex(on ? WEB_OURO : 0x333333), LV_PART_MAIN);
    lv_obj_set_style_border_opa(tg, on ? LV_OPA_COVER : LV_OPA_40, LV_PART_MAIN);
    lv_obj_t *bola = lv_obj_get_child(tg, 0);
    if (bola) lv_obj_align(bola, LV_ALIGN_LEFT_MID, on ? 19 : 3, 0);
}

/* hint .hint: texto pequeno apagado com regua em cima */
static lv_obj_t *hint_web(lv_obj_t *pai, int y, int largura, const char *txt)
{
    lv_obj_t *h = lv_obj_create(pai);
    lv_obj_remove_style_all(h);
    lv_obj_set_size(h, largura, 44);
    lv_obj_set_pos(h, 14, y);
    lv_obj_set_style_border_width(h, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(h, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_border_color(h, lv_color_hex(WEB_OURO), LV_PART_MAIN);
    lv_obj_set_style_border_opa(h, LV_OPA_40, LV_PART_MAIN);
    lv_obj_clear_flag(h, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = lv_label_create(h);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &fonte_cinzel_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(WEB_HINT), LV_PART_MAIN);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, largura - 20);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 4);
    return h;
}

/* botao .mbtn da web: cinzel uppercase, borda ouro, ativo dourado */
static lv_obj_t *btn_mweb(lv_obj_t *pai, const char *txt, const lv_font_t *f,
                          int w, int h)
{
    lv_obj_t *b = lv_obj_create(pai);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x0A0806), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, 199, LV_PART_MAIN);   /* .78 */
    lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(b, lv_color_hex(WEB_OURO), LV_PART_MAIN);
    lv_obj_set_style_radius(b, 6, LV_PART_MAIN);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = texto(b, txt, f, WEB_OURO_CLARO);
    lv_obj_center(l);
    return b;
}

/* marca visual de menu ativo (hover/ativo da web: ouro solido, texto escuro) */
static void btn_mweb_ativo(lv_obj_t *b, bool ativo)
{
    lv_obj_set_style_bg_color(b, lv_color_hex(ativo ? WEB_OURO : 0x0A0806), LV_PART_MAIN);
    lv_obj_t *l = lv_obj_get_child(b, 0);
    if (l) lv_obj_set_style_text_color(l,
        lv_color_hex(ativo ? 0x0A0A08 : WEB_OURO_CLARO), LV_PART_MAIN);
}

static lv_obj_t *texto(lv_obj_t *pai, const char *txt,
                       const lv_font_t *fonte, uint32_t cor)
{
    lv_obj_t *l = lv_label_create(pai);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, fonte, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(cor), LV_PART_MAIN);
    return l;
}

/* ---- barra de status: WiFi + preset ---- */

/* pulso discreto do LED (reservado: animacao de sombra pesa no refresh) */
__attribute__((unused))
static void anim_pulso(void *obj, int32_t v)
{
    lv_obj_set_style_shadow_opa((lv_obj_t *)obj, (lv_opa_t)v, LV_PART_MAIN);
}

__attribute__((unused))
static void iniciar_pulso(lv_obj_t *led)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, led);
    lv_anim_set_exec_cb(&a, anim_pulso);
    lv_anim_set_values(&a, LV_OPA_30, LV_OPA_70);   /* sutil, sem exagero */
    lv_anim_set_time(&a, 1300);
    lv_anim_set_playback_time(&a, 1300);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

void palco_set_wifi(bool conectado)
{
    wifi_ok = conectado;
    if (led_wifi) {
        lv_obj_set_style_bg_color(led_wifi,
            lv_color_hex(conectado ? VERDE_LED : VERMELHO), LV_PART_MAIN);
        lv_obj_set_style_shadow_color(led_wifi,
            lv_color_hex(conectado ? VERDE_LED : VERMELHO), LV_PART_MAIN);
    }
    if (lbl_wifi) {
        lv_label_set_text(lbl_wifi, conectado ? "ONLINE" : "OFFLINE");
        lv_obj_set_style_text_color(lbl_wifi,
            lv_color_hex(conectado ? VERDE_LED : VERMELHO), LV_PART_MAIN);
    }
}

const char *palco_get_preset(void)
{
    return preset_nome;
}

/* ---- modo preset: 3 bancos completos, salvos em NVS ---- */

#include "nvs.h"

#define BANCO_QTD   3
static char   banco_nome[BANCO_QTD][24]   = { "BANCO 1", "BANCO 2", "BANCO 3" };
static bool   banco_pedais[BANCO_QTD][2]  = { { true, false }, { false, true }, { false, false } };
static int    banco_amp[BANCO_QTD]        = { 0, 1, 2 };
static int    banco_ativo                 = 0;   /* banco em edicao/agora */

static lv_obj_t *banco_lidas[3] = { NULL, NULL, NULL };   /* 'BANCO 1/2/3' text */
static lv_obj_t *banco_btn[3]   = { NULL, NULL, NULL };   /* chips no painel preset */
static bool teclado_para_banco = false;   /* teclado edita banco em vez do preset */
static int  banco_edit = 0;               /* banco sendo editado no 3o nivel */
static lv_obj_t *painel_banco = NULL;     /* submenu de 3o nivel do banco */

/* prototipos (implementacao vem depois dos pedais web) */
static void bancos_carregar(void);
static void bancos_salvar_campo(int b, const char *campo);
static void banco_aplicar(void);
static void repintar_chips_banco(void);
static void fechar_painel_banco(void);
static void abrir_painel_banco(int b);
static void ev_banco_usar(lv_event_t *e);
static void ev_banco_editar(lv_event_t *e);
static void ev_banco_nomear(lv_event_t *e);
static void ev_banco_pedal(lv_event_t *e);
static void ev_banco_amp(lv_event_t *e);
static void ev_banco_salvar_estado(lv_event_t *e);

/* ev_banco_editar: toque num chip BANCO abre o painel do banco (3o nivel) */
static void ev_banco_editar(lv_event_t *e)
{
    abrir_painel_banco((int)(intptr_t)lv_event_get_user_data(e));
}

/* ---- estado dos loops, compartilhado com a tela 1 ---- */

static bool loop_ligado[8] = { false };
static void (*cb_sync)(int, bool) = NULL;
static void (*cb_nome)(const char *) = NULL;
static void (*cb_cor)(uint32_t) = NULL;
static void (*cb_canal)(uint32_t) = NULL;
static void (*cb_fonte)(const lv_font_t *) = NULL;

void palco_registrar_fonte(void (*cb)(const lv_font_t *))
{
    cb_fonte = cb;
    if (cb_fonte) cb_fonte(estilos[estilo_atual].grande);
}

void palco_registrar_cor(void (*cb)(uint32_t))
{
    cb_cor = cb;
    if (cb_cor) cb_cor(cor_atual);
}

void palco_registrar_canal(void (*cb)(uint32_t))
{
    cb_canal = cb;
    if (cb_canal) cb_canal(cor_canal[0]);
}

void palco_registrar_nome(void (*cb)(const char *))
{
    cb_nome = cb;
    if (cb_nome) cb_nome(preset_nome);
}

void palco_registrar_sync(void (*cb)(int, bool))
{
    cb_sync = cb;
    for (int i = 0; i < 8; i++) {
        if (cb_sync) cb_sync(i, loop_ligado[i]);
    }
}

bool palco_loop_ligado(int i)
{
    return (i >= 0 && i < 8) ? loop_ligado[i] : false;
}

void palco_toggle_loop(int i)
{
    if (i < 0 || i >= 8) return;
    loop_ligado[i] = !loop_ligado[i];
    ESP_LOGI(TAG, "loop %d -> %s", i + 1, loop_ligado[i] ? "ON" : "OFF");
    if (cb_sync) cb_sync(i, loop_ligado[i]);   /* acende/apaga na tela 1 */
}

/* ---- teclado: escreve o nome do preset ---- */

static void fechar_teclado(void)
{
    if (teclado_box) {
        lv_obj_del(teclado_box);
        teclado_box = NULL;
        ta_nome = NULL;
    }
}

static void ev_teclado(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_READY) {            /* OK: salva */
        const char *t = lv_textarea_get_text(ta_nome);
        if (t && t[0]) {
            if (teclado_para_banco) {
                strncpy(banco_nome[banco_ativo], t, sizeof(banco_nome[0]) - 1);
                banco_nome[banco_ativo][sizeof(banco_nome[0]) - 1] = '\0';
                bancos_salvar_campo(banco_ativo, "nome");
                ESP_LOGI(TAG, "banco %d renomeado: %s", banco_ativo, banco_nome[banco_ativo]);
                repintar_chips_banco();
            } else {
                strncpy(preset_nome, t, sizeof(preset_nome) - 1);
                preset_nome[sizeof(preset_nome) - 1] = '\0';
                if (lbl_preset) lv_label_set_text(lbl_preset, preset_nome);
                if (cb_nome) cb_nome(preset_nome);      /* propaga para a tela 1 */
                ESP_LOGI(TAG, "preset salvo: %s", preset_nome);
            }
        }
        teclado_para_banco = false;
        fechar_teclado();
    } else if (code == LV_EVENT_CANCEL) {
        teclado_para_banco = false;
        fechar_teclado();
    }
}

static void abrir_teclado(lv_event_t *e)
{
    if (teclado_box) return;

    teclado_box = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(teclado_box);
    lv_obj_set_size(teclado_box, 800, 480);
    lv_obj_set_pos(teclado_box, 0, 0);
    lv_obj_set_style_bg_color(teclado_box, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(teclado_box, LV_OPA_80, LV_PART_MAIN);
    lv_obj_clear_flag(teclado_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(teclado_box, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *titulo = texto(teclado_box, "NOME DO PRESET",
                             &fonte_menu_22, OURO);
    lv_obj_set_style_text_letter_space(titulo, 3, LV_PART_MAIN);
    lv_obj_align(titulo, LV_ALIGN_TOP_MID, 0, 8);

    ta_nome = lv_textarea_create(teclado_box);
    lv_textarea_set_one_line(ta_nome, true);
    lv_textarea_set_max_length(ta_nome, sizeof(preset_nome) - 1);
    lv_textarea_set_text(ta_nome, preset_nome);
    lv_obj_set_size(ta_nome, 520, 50);
    lv_obj_align(ta_nome, LV_ALIGN_TOP_MID, 0, 38);
    lv_obj_set_style_bg_color(ta_nome, lv_color_hex(0x151008), LV_PART_MAIN);
    lv_obj_set_style_border_color(ta_nome, lv_color_hex(OURO), LV_PART_MAIN);
    lv_obj_set_style_border_width(ta_nome, 2, LV_PART_MAIN);
    lv_obj_set_style_text_color(ta_nome, lv_color_hex(OURO_CLARO), LV_PART_MAIN);
    lv_obj_set_style_text_font(ta_nome, &lv_font_montserrat_28, LV_PART_MAIN);

    lv_obj_t *kb = lv_keyboard_create(teclado_box);
    lv_obj_set_size(kb, 800, 372);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_UPPER);
    lv_keyboard_set_textarea(kb, ta_nome);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x0E0A04), LV_PART_MAIN);
    lv_obj_set_style_text_color(kb, lv_color_hex(OURO_CLARO), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x241A0A), LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, lv_color_hex(OURO_ESCURO), LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
    lv_obj_add_event_cb(kb, ev_teclado, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, ev_teclado, LV_EVENT_CANCEL, NULL);
}

/* ---- menus e submenus ---- */

/* direcao de abertura do submenu, escolhida em SISTEMA */
typedef enum { DIR_BAIXO = 0, DIR_ESQUERDA = 1 } dir_menu_t;
static dir_menu_t dir_menu = DIR_BAIXO;

typedef struct {
    const char *icone;          /* simbolo LVGL: menus sem texto */
    const char *titulo;         /* usado apenas no cabecalho do submenu */
    const char *itens[8];
} submenu_t;

/* paleta de cores das letras, todas dentro da estetica steampunk */
typedef struct { const char *nome; uint32_t cor; } cor_letra_t;
static const cor_letra_t cores_letra[] = {
    { "Ouro claro", 0xF3C969 },
    { "Latao",      0xC9922B },
    { "Cobre",      0xB87333 },
    { "Ambar",      0xFFA030 },
    { "Marfim",     0xEDE0C8 },
    { "Bronze",     0x8A6420 },
};
#define QTD_CORES (sizeof(cores_letra) / sizeof(cores_letra[0]))

static const submenu_t menus[] = {
    { LV_SYMBOL_SAVE,      "PRESETS", { "Renomear", "Salvar", "Carregar", "Duplicar", NULL, NULL } },
    { LV_SYMBOL_SHUFFLE,   "PEDAIS",  { NULL, NULL, NULL, NULL, NULL, NULL } },   /* gerado: 8 loops */
    { LV_SYMBOL_AUDIO,     "AMP",     { "Canal 1", "Canal 2", "Canal 3", "Boost", "Trocar Amp", NULL } },
    { LV_SYMBOL_WIFI,      "REDE",    { "WiFi", "ESP-NOW", "Pareamento", "Status", NULL, NULL } },
    { LV_SYMBOL_IMAGE,     "SKIN",    { "Vid: lento", "Vid: normal", "Vid: rapido", "Fundo SD 1", "Fundo SD 2", "Fundo: video", NULL } },
    { LV_SYMBOL_SETTINGS,  "SISTEMA", { "Menu: abaixo", "Menu: esquerda", "Cor das letras", "Estilo das letras", "Brilho", NULL } },
    { LV_SYMBOL_VOLUME_MAX, "MIDI",    { "Canal MIDI", "Clock Sync", "Scenes", NULL, NULL, NULL } },
};
#define MENU_PRESETS 0
#define MENU_PEDAIS 1
#define MENU_AMP    2
#define MENU_REDE   3
#define MENU_SKIN   4
#define MENU_MIDI   6

/* rotulos fixos dos loops e LEDs de estado dentro do submenu */
static const char *nome_loop[8] = {
    "Loop 1", "Loop 2", "Loop 3", "Loop 4",
    "Loop 5", "Loop 6", "Loop 7", "Loop 8"
};
static lv_obj_t *led_item[8];
static int canal_amp = 0;               /* canal do amp selecionado */
static lv_obj_t *btn_canal[4];
#define QTD_MENUS (sizeof(menus) / sizeof(menus[0]))

/* ---- pedais estilo web: strip 2 slots + imagens reais ---- */
extern const lv_img_dsc_t img_pedal_od;     /* overdrive 300x200 RGBA */
extern const lv_img_dsc_t img_pedal_ch;     /* chorus 300x200 RGBA */
extern const lv_img_dsc_t img_pedal_dl;     /* delay 300x200 RGBA */

typedef struct { const char *nome; const lv_img_dsc_t *img; } pedal_web_t;
static const pedal_web_t pedais_web[] = {
    { "OVERDRIVE", &img_pedal_od },
    { "CHORUS",    &img_pedal_ch },
    { "DELAY",     &img_pedal_dl },
};
#define QTD_PEDAIS_WEB (sizeof(pedais_web) / sizeof(pedais_web[0]))

#define SLOT_COUNT 8                            /* igual web: faixa de 8 slots */
static lv_obj_t *slot_web[SLOT_COUNT];          /* containers na base da tela */
static lv_obj_t *pedal_obj[SLOT_COUNT];         /* pedal dentro do slot (ou NULL) */
static const pedal_web_t *pedal_em_slot[SLOT_COUNT];
static lv_obj_t *pedal_check[QTD_PEDAIS_WEB];   /* toggle do painel pedais */

/* nome dos amps (igual app_main): usado no submenu do banco */
static const char *banco_amp_names[3] = { "LONESTAR", "SOLDANO", "JP-2C" };

/* ---- implementacao dos bancos (3o nivel do menu preset) ---- */

/* carrega os 3 bancos do NVS (nao existe: usa default) */
static void bancos_carregar(void)
{
    nvs_handle_t h;
    if (nvs_open("oldskull", NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "NVS vazio: bancos default");
        return;
    }
    char chave[16];
    for (int b = 0; b < BANCO_QTD; b++) {
        snprintf(chave, sizeof(chave), "b%dnome", b);
        size_t sz = sizeof(banco_nome[b]);
        nvs_get_str(h, chave, banco_nome[b], &sz);
        snprintf(chave, sizeof(chave), "b%damp", b);
        nvs_get_i8(h, chave, (int8_t *)&banco_amp[b]);
        uint8_t ped = 0;
        snprintf(chave, sizeof(chave), "b%dped", b);
        if (nvs_get_u8(h, chave, &ped) == ESP_OK) {
            banco_pedais[b][0] = (ped & 1) != 0;
            banco_pedais[b][1] = (ped & 2) != 0;
        }
    }
    nvs_get_i8(h, "ativo", (int8_t *)&banco_ativo);
    if (banco_ativo < 0 || banco_ativo >= BANCO_QTD) banco_ativo = 0;
    nvs_close(h);
    ESP_LOGI(TAG, "bancos do NVS (ativo: %d)", banco_ativo);
}

/* grava 1 campo de 1 banco (ou o indice ativo) */
static void bancos_salvar_campo(int b, const char *campo)
{
    nvs_handle_t h;
    if (nvs_open("oldskull", NVS_READWRITE, &h) != ESP_OK) return;
    char chave[16];
    if (strcmp(campo, "nome") == 0) {
        snprintf(chave, sizeof(chave), "b%dnome", b);
        nvs_set_str(h, chave, banco_nome[b]);
    } else if (strcmp(campo, "amp") == 0) {
        snprintf(chave, sizeof(chave), "b%damp", b);
        nvs_set_i8(h, chave, (int8_t)banco_amp[b]);
    } else if (strcmp(campo, "ped") == 0) {
        uint8_t ped = (banco_pedais[b][0] ? 1 : 0) | (banco_pedais[b][1] ? 2 : 0);
        snprintf(chave, sizeof(chave), "b%dped", b);
        nvs_set_u8(h, chave, ped);
    } else if (strcmp(campo, "ativo") == 0) {
        nvs_set_i8(h, "ativo", (int8_t)banco_ativo);
    }
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "banco %d: %s salvo no NVS", b, campo);
}

/* aplica o banco ao estado vivo: nome grande, amp do carrossel, pedais */
static void banco_aplicar(void)
{
    int b = banco_ativo;
    strncpy(preset_nome, banco_nome[b], sizeof(preset_nome) - 1);
    preset_nome[sizeof(preset_nome) - 1] = '\0';
    if (lbl_preset) lv_label_set_text(lbl_preset, preset_nome);
    if (cb_nome) cb_nome(preset_nome);

    int alvo = banco_amp[b] % 3;
    while (app_amp_atual() != alvo) app_trocar_amp();

    for (int p = 0; p < (int)QTD_PEDAIS_WEB; p++) {
        bool quer = banco_pedais[b][p];
        bool tem = false;
        for (int i = 0; i < SLOT_COUNT; i++) {
            if (pedal_em_slot[i] == &pedais_web[p]) { tem = true; break; }
        }
        if (quer == tem) continue;
        if (quer) {
            for (int i = 0; i < SLOT_COUNT; i++) {
                if (pedal_obj[i]) continue;
                lv_obj_t *im = lv_img_create(slot_web[i]);
                lv_img_set_src(im, pedais_web[p].img);
                lv_obj_center(im);
                lv_obj_clear_flag(im, LV_OBJ_FLAG_CLICKABLE);
                pedal_obj[i] = im;
                pedal_em_slot[i] = &pedais_web[p];
                break;
            }
        } else {
            for (int i = 0; i < SLOT_COUNT; i++) {
                if (pedal_em_slot[i] == &pedais_web[p]) {
                    lv_obj_del(pedal_obj[i]);
                    pedal_obj[i] = NULL;
                    pedal_em_slot[i] = NULL;
                    break;
                }
            }
        }
    }
    ESP_LOGI(TAG, "banco %d aplicado: %s", b, preset_nome);
}

/* chips BANCO 1/2/3 do painel preset: repinta nomes e marca o ativo */
static void repintar_chips_banco(void)
{
    for (int i = 0; i < BANCO_QTD; i++) {
        if (banco_lidas[i]) lv_label_set_text(banco_lidas[i], banco_nome[i]);
        if (banco_btn[i]) {
            lv_obj_set_style_bg_color(banco_btn[i],
                lv_color_hex(i == banco_ativo ? WEB_OURO : 0x0A0806), LV_PART_MAIN);
            if (banco_lidas[i]) {
                lv_obj_set_style_text_color(banco_lidas[i],
                    lv_color_hex(i == banco_ativo ? 0x0A0A08 : WEB_OURO_CLARO), LV_PART_MAIN);
            }
        }
    }
}

static void fechar_painel_banco(void)
{
    if (painel_banco) {
        lv_obj_del(painel_banco);
        painel_banco = NULL;
    }
}

/* 3o nivel: tudo de um banco (usar, nomear, pedais, amp, capturar) */
static void abrir_painel_banco(int b)
{
    fechar_painel_banco();
    banco_edit = b;

    painel_banco = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(painel_banco);
    estilo_painel_web(painel_banco);
    lv_obj_set_size(painel_banco, 320, 330);
    lv_obj_set_pos(painel_banco, 800 - 340, 8);
    lv_obj_add_flag(painel_banco, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *t = texto(painel_banco, "BANCO", &fonte_cinzel_26, WEB_OURO_CLARO);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 12);

    /* usar (aplica) */
    lv_obj_t *bu = btn_mweb(painel_banco, "USAR", &fonte_cinzel_22, 120, 36);
    lv_obj_set_pos(bu, 14, 48);
    lv_obj_add_event_cb(bu, ev_banco_usar, LV_EVENT_CLICKED, (void *)(intptr_t)b);

    /* nomear (teclado) */
    lv_obj_t *bn = btn_mweb(painel_banco, "NOMEAR", &fonte_cinzel_22, 120, 36);
    lv_obj_set_pos(bn, 146, 48);
    lv_obj_add_event_cb(bn, ev_banco_nomear, LV_EVENT_CLICKED, (void *)(intptr_t)b);

    /* pedais: 2 toggles com nome (quais) */
    for (int p = 0; p < (int)QTD_PEDAIS_WEB; p++) {
        lv_obj_t *row = item_web(painel_banco, 100 + p * 42, 292,
                                 pedais_web[p].nome, NULL);
        lv_obj_t *tg = toggle_web(row, 230, 7, banco_pedais[b][p]);
        lv_obj_add_event_cb(tg, ev_banco_pedal, LV_EVENT_CLICKED,
                            (void *)(intptr_t)(b * 2 + p));
    }

    /* amp do banco (qual): cicla 3 amps a cada toque */
    lv_obj_t *row_amp = item_web(painel_banco, 184, 292, "AMP", "→");
    lv_obj_add_event_cb(row_amp, ev_banco_amp, LV_EVENT_CLICKED, (void *)(intptr_t)b);

    /* capturar: salva posicao atual (pedais na tela + amp do carrossel) */
    lv_obj_t *bs = btn_mweb(painel_banco, "SALVAR POSIÇÃO ATUAL", &fonte_cinzel_22, 292, 36);
    lv_obj_set_pos(bs, 14, 226);
    lv_obj_add_event_cb(bs, ev_banco_salvar_estado, LV_EVENT_CLICKED, (void *)(intptr_t)b);

    lv_obj_t *h = hint_web(painel_banco, 270, 292,
        "USAR aplica o banco. SALVAR POSIÇÃO captura pedais+amp da tela agora.");
    (void)h;
}

static void ev_banco_usar(lv_event_t *e)
{
    int b = (int)(intptr_t)lv_event_get_user_data(e);
    banco_ativo = b;
    bancos_salvar_campo(b, "ativo");
    banco_aplicar();
    repintar_chips_banco();
    fechar_painel_banco();
}

static void ev_banco_nomear(lv_event_t *e)
{
    int b = (int)(intptr_t)lv_event_get_user_data(e);
    banco_ativo = b;
    bancos_salvar_campo(b, "ativo");
    teclado_para_banco = true;
    fechar_painel_banco();
    abrir_teclado(NULL);
}

static void ev_banco_pedal(lv_event_t *e)
{
    int v = (int)(intptr_t)lv_event_get_user_data(e);
    int b = v / 2, p = v % 2;
    banco_pedais[b][p] = !banco_pedais[b][p];
    toggle_web_set(lv_event_get_target(e), banco_pedais[b][p]);
    bancos_salvar_campo(b, "ped");
}

static void ev_banco_amp(lv_event_t *e)
{
    int b = (int)(intptr_t)lv_event_get_user_data(e);
    banco_amp[b] = (banco_amp[b] + 1) % 3;
    bancos_salvar_campo(b, "amp");
    /* reflete na tela so se este banco estiver ativo */
    if (b == banco_ativo) {
        int alvo = banco_amp[b];
        while (app_amp_atual() != alvo) app_trocar_amp();
    }
    lv_obj_t *row = lv_event_get_target(e);
    lv_obj_t *lv_ = lv_obj_get_child(row, 1);   /* label '->' a direita */
    if (lv_) {
        lv_label_set_text(lv_, banco_amp_names[banco_amp[b]]);
    }
}

static void ev_banco_salvar_estado(lv_event_t *e)
{
    int b = (int)(intptr_t)lv_event_get_user_data(e);
    banco_pedais[b][0] = false;
    banco_pedais[b][1] = false;
    for (int i = 0; i < SLOT_COUNT; i++) {
        for (int p = 0; p < (int)QTD_PEDAIS_WEB; p++) {
            if (pedal_em_slot[i] == &pedais_web[p]) banco_pedais[b][p] = true;
        }
    }
    banco_amp[b] = app_amp_atual();
    bancos_salvar_campo(b, "ped");
    bancos_salvar_campo(b, "amp");
    ESP_LOGI(TAG, "banco %d: posicao capturada (amp %d)", b, banco_amp[b]);
    fechar_painel_banco();
}

/* ev_pedal_check: checkbox do painel PEDAIS = add/remove o pedal da base */
static void ev_pedal_check(lv_event_t *e)
{
    int p = (int)(intptr_t)lv_event_get_user_data(e);
    if (p < 0 || p >= (int)QTD_PEDAIS_WEB) return;

    /* ja tem esse pedal na base? remove (toggle off) */
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (pedal_em_slot[i] == &pedais_web[p]) {
            lv_obj_del(pedal_obj[i]);
            pedal_obj[i] = NULL;
            pedal_em_slot[i] = NULL;
            toggle_web_set(pedal_check[p], false);
            ESP_LOGI(TAG, "pedal %s removido", pedais_web[p].nome);
            return;
        }
    }
    /* slot livre? coloca */
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (pedal_obj[i]) continue;
        lv_obj_t *im = lv_img_create(slot_web[i]);
        lv_img_set_src(im, pedais_web[p].img);
        lv_obj_center(im);
        lv_obj_clear_flag(im, LV_OBJ_FLAG_CLICKABLE);
        pedal_obj[i] = im;
        pedal_em_slot[i] = &pedais_web[p];
        toggle_web_set(pedal_check[p], true);
        ESP_LOGI(TAG, "pedal %s no slot %d", pedais_web[p].nome, i);
        return;
    }
    ESP_LOGI(TAG, "slots cheios");
}

/* tap no slot da base = remove o pedal (igual web) */
static void ev_pedal_slot(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= SLOT_COUNT) return;
    if (pedal_obj[i]) {
        /* desliga o checkbox do pedal removido */
        for (int p = 0; p < (int)QTD_PEDAIS_WEB; p++) {
            if (pedal_em_slot[i] == &pedais_web[p] && pedal_check[p]) {
                toggle_web_set(pedal_check[p], false);
            }
        }
        lv_obj_del(pedal_obj[i]);
        pedal_obj[i] = NULL;
        pedal_em_slot[i] = NULL;
        ESP_LOGI(TAG, "pedal removido do slot %d", i);
    }
}

static void ev_pedal_check(lv_event_t *e);   /* prototipo: toggle no painel */

/* prototipos: ev_item usa estas antes das definicoes */
static void fechar_submenu(void);
static void abrir_teclado(lv_event_t *e);
static void abrir_cores(void);
static void abrir_estilos(void);
static void aplicar_cor(uint32_t cor);
static void salvar_preset(void);

static int menu_aberto = -1;

/* clique num loop: alterna e atualiza o LED sem fechar o submenu */
static void ev_loop(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= 8) return;

    palco_toggle_loop(i);                       /* muda estado + tela 1 */

    if (led_item[i]) {                          /* reflete aqui na hora */
        bool on = palco_loop_ligado(i);
        lv_obj_set_style_bg_color(led_item[i],
            lv_color_hex(on ? VERDE_LED : 0x3A2A10), LV_PART_MAIN);
        lv_obj_set_style_shadow_color(led_item[i],
            lv_color_hex(on ? VERDE_LED : 0x000000), LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(led_item[i],
            on ? LV_OPA_60 : LV_OPA_TRANSP, LV_PART_MAIN);
    }
}

/* clique num canal do amp: selecao exclusiva */
static void ev_canal(lv_event_t *e)
{
    int c = (int)(intptr_t)lv_event_get_user_data(e);
    canal_amp = c;
    ESP_LOGI(TAG, "canal do amp: %d", c + 1);
    if (c >= 0 && c < 4 && cb_canal) cb_canal(cor_canal[c]);   /* tinge a tela 1 */

    for (int i = 0; i < 4; i++) {
        if (!btn_canal[i]) continue;
        bool sel = (i == c);
        lv_obj_set_style_bg_color(btn_canal[i],
            lv_color_hex(sel ? 0x4A3410 : 0x241A0A), LV_PART_MAIN);
        lv_obj_set_style_border_color(btn_canal[i],
            lv_color_hex(sel ? AMBAR : OURO_ESCURO), LV_PART_MAIN);
    }
}

static void ev_item(lv_event_t *e)
{
    const char *nome = (const char *)lv_event_get_user_data(e);
    if (!nome) return;

    if (strcmp(nome, "Renomear") == 0) {
        abrir_teclado(NULL);
        return;
    }
    if (strcmp(nome, "Menu: abaixo") == 0) {
        dir_menu = DIR_BAIXO;
        ESP_LOGI(TAG, "submenu abre por baixo");
        fechar_submenu();
        return;
    }
    if (strcmp(nome, "Menu: esquerda") == 0) {
        dir_menu = DIR_ESQUERDA;
        ESP_LOGI(TAG, "submenu abre pela esquerda");
        fechar_submenu();
        return;
    }
    if (strcmp(nome, "Cor das letras") == 0) {
        fechar_submenu();
        abrir_cores();
        return;
    }
    if (strcmp(nome, "Estilo das letras") == 0) {
        fechar_submenu();
        abrir_estilos();
        return;
    }
    if (strcmp(nome, "Salvar") == 0) {
        salvar_preset();
        fechar_submenu();
        return;
    }
    if (strcmp(nome, "Vid: lento") == 0)   { video_set_fps(8);  fechar_submenu(); return; }
    if (strcmp(nome, "Vid: normal") == 0)  { video_set_fps(24); fechar_submenu(); return; }
    if (strcmp(nome, "Vid: rapido") == 0)  { video_set_fps(48); fechar_submenu(); return; }
    if (strcmp(nome, "Trocar Amp") == 0)    { app_trocar_amp(); fechar_submenu(); return; }
    if (strcmp(nome, "Fundo SD 1") == 0) {
        sd_fundos_mostrar(lv_scr_act(), FUNDO_SD_1);
        fechar_submenu(); return;
    }
    if (strcmp(nome, "Fundo SD 2") == 0) {
        sd_fundos_mostrar(lv_scr_act(), FUNDO_SD_2);
        fechar_submenu(); return;
    }
    if (strcmp(nome, "Fundo: video") == 0) {
        sd_fundos_esconder();
        fechar_submenu(); return;
    }
    ESP_LOGI(TAG, "item: %s", nome);
}

static void fechar_submenu(void)
{
    if (painel_menu) {
        lv_anim_del(painel_menu, NULL);      /* anim nao pode ref obj morto */
        lv_obj_del_async(painel_menu);       /* delete fora do evento = sem panic */
        painel_menu = NULL;
    }
    /* os LEDs e botoes viviam dentro do painel: evita ponteiro solto */
    for (int i = 0; i < 8; i++) led_item[i] = NULL;
    for (int i = 0; i < 4; i++) btn_canal[i] = NULL;
    for (int p = 0; p < (int)QTD_PEDAIS_WEB; p++) pedal_check[p] = NULL;
    lbl_wifi = NULL;          /* status so existe dentro do menu REDE */
    menu_aberto = -1;
}

/* animacao: abre por baixo (altura) ou pela esquerda (largura) */
static void anim_altura(void *obj, int32_t v)
{
    lv_obj_set_height((lv_obj_t *)obj, v);
}

static void anim_largura(void *obj, int32_t v)
{
    lv_obj_set_width((lv_obj_t *)obj, v);
}

static void anim_opa(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, LV_PART_MAIN);
}

static void abrir_submenu(int idx)
{
    /* fecha o anterior: so um submenu por vez */
    fechar_submenu();
    if (idx < 0 || idx >= (int)QTD_MENUS) return;
    menu_aberto = idx;

    /* altura: PEDAIS = checkboxes + loops; outros = itens + hint */
    int n = 0;
    if (idx == MENU_PEDAIS) n = 8;
    else while (n < 8 && menus[idx].itens[n]) n++;

    const int larg = 320;
    int alt;
    if (idx == MENU_PEDAIS)      alt = 78 + (int)QTD_PEDAIS_WEB * 36 + 8 * 34;
    else if (idx == MENU_SKIN)   alt = 54 + n * 34 + 56;
    else if (idx == MENU_PRESETS) alt = 54 + n * 34 + 44 + 36 + 40;
    else                         alt = 54 + n * 34 + 48;

    painel_menu = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(painel_menu);
    estilo_painel_web(painel_menu);

    /* posicao conforme a direcao escolhida em SISTEMA */
    int x, y;
    if (dir_menu == DIR_BAIXO) {
        const int lg = 96;
        x = 16 + idx * (lg + 6);
        if (x + larg > 788) x = 788 - larg;
        y = 480 - 66 - alt;
        if (y < 8) y = 8;
        lv_obj_set_width(painel_menu, larg);
        lv_obj_set_height(painel_menu, alt);
    } else {
        x = 8;
        y = (480 - alt) / 2;
        if (y < 8) y = 8;
        lv_obj_set_width(painel_menu, 0);
        lv_obj_set_height(painel_menu, alt);
    }
    lv_obj_set_pos(painel_menu, x, y);

    /* titulo estilo h3: cinzel uppercase c/ regua embaixo */
    lv_obj_t *t = texto(painel_menu, menus[idx].titulo, &fonte_cinzel_26, WEB_OURO_CLARO);
    lv_obj_set_style_text_letter_space(t, 4, LV_PART_MAIN);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 16, 10);
    lv_obj_t *regua = lv_obj_create(painel_menu);
    lv_obj_remove_style_all(regua);
    lv_obj_set_size(regua, larg - 32, 2);
    lv_obj_align(regua, LV_ALIGN_TOP_LEFT, 16, 44);
    lv_obj_set_style_bg_color(regua, lv_color_hex(WEB_OURO), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(regua, LV_OPA_COVER, LV_PART_MAIN);

    if (idx == MENU_PEDAIS) {
        /* checkboxes estilo web (add/remove da base) */
        for (int p = 0; p < (int)QTD_PEDAIS_WEB; p++) {
            lv_obj_t *it = item_web(painel_menu, 54 + p * 36, larg - 28,
                                    pedais_web[p].nome, NULL);
            lv_obj_add_event_cb(it, ev_pedal_check, LV_EVENT_CLICKED,
                                (void *)(intptr_t)p);
            lv_obj_t *tg = toggle_web(painel_menu,
                                      16 + larg - 28 - 44 - 8, 54 + p * 36 + 7,
                                      pedal_em_slot[0] == &pedais_web[p] ||
                                      pedal_em_slot[1] == &pedais_web[p]);
            pedal_check[p] = tg;
            /* tap no toggle tambem alterna (mesma acao do item) */
            lv_obj_add_event_cb(tg, ev_pedal_check, LV_EVENT_CLICKED,
                                (void *)(intptr_t)p);
            /* toggle por cima do item: clicavel primeiro */
            lv_obj_move_foreground(tg);
        }
        /* loops 1-8 com LED (comportamento nativo preservado) */
        for (int i = 0; i < 8; i++) {
            lv_obj_t *it = item_web(painel_menu,
                                    54 + (int)QTD_PEDAIS_WEB * 36 + i * 34,
                                    larg - 28, nome_loop[i], NULL);
            lv_obj_add_event_cb(it, ev_loop, LV_EVENT_CLICKED, (void *)(intptr_t)i);

            lv_obj_t *led = lv_obj_create(it);
            lv_obj_remove_style_all(led);
            lv_obj_set_size(led, 12, 12);
            lv_obj_align(led, LV_ALIGN_RIGHT_MID, -10, 0);
            lv_obj_set_style_radius(led, LV_RADIUS_CIRCLE, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(led, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(led, 12, LV_PART_MAIN);
            bool on = palco_loop_ligado(i);
            lv_obj_set_style_bg_color(led,
                lv_color_hex(on ? VERDE_LED : 0x3A2A10), LV_PART_MAIN);
            lv_obj_set_style_shadow_color(led,
                lv_color_hex(on ? VERDE_LED : 0x000000), LV_PART_MAIN);
            lv_obj_set_style_shadow_opa(led,
                on ? LV_OPA_60 : LV_OPA_TRANSP, LV_PART_MAIN);
            led_item[i] = led;
        }
    } else {
        /* itens comuns: linha separadora, k a esquerda */
        for (int i = 0; i < n; i++) {
            lv_obj_t *it = item_web(painel_menu, 54 + i * 34, larg - 28,
                                    menus[idx].itens[i], NULL);
            const char *rotulo = menus[idx].itens[i];
            if (idx == MENU_AMP && i < 4) {
                lv_obj_add_event_cb(it, ev_canal, LV_EVENT_CLICKED, (void *)(intptr_t)i);
                btn_canal[i] = it;
                if (i == canal_amp)
                    lv_obj_set_style_border_color(it, lv_color_hex(AMBAR), LV_PART_MAIN);
            } else {
                lv_obj_add_event_cb(it, ev_item, LV_EVENT_CLICKED, (void *)rotulo);
            }
        }
        if (idx == MENU_MIDI) {
            /* canal do MIDI: linha extra com valor (visual-only, igual web) */
            item_web(painel_menu, 54 + n * 34, larg - 28, "Ch", "01");
        }
    }

    /* 3o nivel: chips dos bancos no menu PRESETS (sob os 4 itens) */
    if (idx == MENU_PRESETS) {
        for (int i = 0; i < BANCO_QTD; i++) {
            lv_obj_t *chip = btn_mweb(painel_menu, banco_nome[i],
                                      &fonte_cinzel_22, 96, 36);
            lv_obj_set_pos(chip, 14 + i * 100, 54 + 4 * 34 + 8);
            lv_obj_add_event_cb(chip, ev_banco_editar, LV_EVENT_CLICKED,
                                (void *)(intptr_t)i);
            banco_btn[i] = chip;
            banco_lidas[i] = (lv_obj_t *)lv_obj_get_child(chip, 0);
        }
        repintar_chips_banco();
        lv_obj_t *h = hint_web(painel_menu, 54 + 4 * 34 + 52, larg - 28,
            "Toque num banco para usar/editar (nomear, pedais, amp).");
        (void)h;
    }

    /* no menu REDE, status no rodape do painel */
    if (idx == MENU_REDE) {
        lbl_wifi = texto(painel_menu,
                         wifi_ok ? "Status: ONLINE" : "Status: OFFLINE",
                         &fonte_cinzel_16, wifi_ok ? VERDE_LED : VERMELHO);
        lv_obj_align(lbl_wifi, LV_ALIGN_BOTTOM_LEFT, 16, -10);
    }


    lv_anim_t f;
    lv_anim_init(&f);
    lv_anim_set_var(&f, painel_menu);
    lv_anim_set_exec_cb(&f, anim_opa);
    lv_anim_set_values(&f, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&f, 280);
    lv_anim_start(&f);
}

static lv_obj_t *btn_menu[8];

/* ---- cor das letras ---- */

static lv_obj_t *painel_cores = NULL;
static lv_obj_t *lbl_todos[16];
static int qtd_lbl = 0;

static void registrar_lbl(lv_obj_t *l)
{
    if (qtd_lbl < 16) lbl_todos[qtd_lbl++] = l;
}

static void aplicar_cor(uint32_t cor)
{
    cor_atual = cor;
    for (int i = 0; i < qtd_lbl; i++) {
        if (lbl_todos[i]) {
            lv_obj_set_style_text_color(lbl_todos[i], lv_color_hex(cor), LV_PART_MAIN);
        }
    }
    if (lbl_preset) lv_obj_set_style_text_color(lbl_preset, lv_color_hex(cor), LV_PART_MAIN);
    if (cb_cor) cb_cor(cor);            /* mesma cor na tela 1 */
    ESP_LOGI(TAG, "cor das letras: #%06lX", (unsigned long)cor);
}

static void ev_cor(lv_event_t *e)
{
    uint32_t cor = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    aplicar_cor(cor);
    if (painel_cores) { lv_obj_del(painel_cores); painel_cores = NULL; }
}

static void abrir_cores(void)
{
    if (painel_cores) { lv_obj_del(painel_cores); painel_cores = NULL; }

    painel_cores = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(painel_cores);
    lv_obj_set_size(painel_cores, 300, 46 + QTD_CORES * 38);
    lv_obj_align(painel_cores, LV_ALIGN_CENTER, 0, -20);
    estilo_painel(painel_cores);

    lv_obj_t *t = texto(painel_cores, "COR DAS LETRAS", &fonte_cinzel_20, cor_atual);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 8);

    for (int i = 0; i < (int)QTD_CORES; i++) {
        lv_obj_t *b = lv_obj_create(painel_cores);
        lv_obj_remove_style_all(b);
        lv_obj_set_size(b, 276, 34);
        lv_obj_set_pos(b, 12, 40 + i * 38);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x241A0A), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(b, LV_OPA_80, LV_PART_MAIN);
        lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(b, lv_color_hex(cores_letra[i].cor), LV_PART_MAIN);
        lv_obj_set_style_radius(b, 4, LV_PART_MAIN);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(b, ev_cor, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)cores_letra[i].cor);

        /* amostra na propria cor */
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, cores_letra[i].nome);
        lv_obj_set_style_text_font(l, &fonte_cinzel_16, LV_PART_MAIN);
        lv_obj_set_style_text_color(l, lv_color_hex(cores_letra[i].cor), LV_PART_MAIN);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 10, 0);
    }
}

/* ---- estilo das letras ---- */

static void ev_estilo(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= (int)QTD_ESTILOS) return;

    estilo_atual = idx;
    ESP_LOGI(TAG, "estilo das letras: %s", estilos[idx].nome);

    /* aplica no nome do preset ja visivel */
    if (lbl_preset) {
        lv_obj_set_style_text_font(lbl_preset, estilos[idx].grande, LV_PART_MAIN);
    }
    if (cb_fonte) cb_fonte(estilos[idx].grande);   /* e na tela 1 */

    if (painel_cores) { lv_obj_del(painel_cores); painel_cores = NULL; }
}

static void abrir_estilos(void)
{
    if (painel_cores) { lv_obj_del(painel_cores); painel_cores = NULL; }

    painel_cores = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(painel_cores);
    lv_obj_set_size(painel_cores, 330, 52 + QTD_ESTILOS * 50);
    lv_obj_align(painel_cores, LV_ALIGN_CENTER, 0, -10);
    estilo_painel(painel_cores);

    lv_obj_t *t = texto(painel_cores, "ESTILO", estilos[estilo_atual].media, cor_atual);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 8);

    for (int i = 0; i < (int)QTD_ESTILOS; i++) {
        lv_obj_t *b = lv_obj_create(painel_cores);
        lv_obj_remove_style_all(b);
        lv_obj_set_size(b, 306, 44);
        lv_obj_set_pos(b, 12, 46 + i * 50);
        lv_obj_set_style_bg_color(b,
            lv_color_hex(i == estilo_atual ? 0x4A3410 : 0x241A0A), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(b, LV_OPA_80, LV_PART_MAIN);
        lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(b,
            lv_color_hex(i == estilo_atual ? AMBAR : OURO_ESCURO), LV_PART_MAIN);
        lv_obj_set_style_radius(b, 4, LV_PART_MAIN);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(b, ev_estilo, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        /* amostra: o nome do estilo escrito na propria fonte */
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, estilos[i].nome);
        lv_obj_set_style_text_font(l, estilos[i].media, LV_PART_MAIN);
        lv_obj_set_style_text_color(l, lv_color_hex(cor_atual), LV_PART_MAIN);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 12, 0);
    }
}

/* ---- salvar preset ---- */

static void salvar_preset(void)
{
    /* o estado ja vive aqui (nome + loops); a tela 1 reflete por callback.
     * Confirmacao apenas no log: nada fica escrito na tela. */
    ESP_LOGI(TAG, "preset '%s' salvo | loops: %d%d%d%d%d%d%d%d",
             preset_nome,
             loop_ligado[0], loop_ligado[1], loop_ligado[2], loop_ligado[3],
             loop_ligado[4], loop_ligado[5], loop_ligado[6], loop_ligado[7]);
    if (cb_nome) cb_nome(preset_nome);
    for (int i = 0; i < 8; i++) {
        if (cb_sync) cb_sync(i, loop_ligado[i]);
    }
}

static void ev_menu(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    bool abrir = (menu_aberto != idx);

    if (abrir) abrir_submenu(idx);
    else       fechar_submenu();

    /* destaca apenas o menu ativo (estilo web: ouro solido) */
    for (int i = 0; i < (int)QTD_MENUS; i++) {
        if (!btn_menu[i]) continue;
        btn_mweb_ativo(btn_menu[i], abrir && (i == idx));
    }
}

/* ---- engrenagem: alterna interface / cenario limpo ---- */

static void ev_engrenagem(lv_event_t *e)
{
    ui_visivel = !ui_visivel;
    if (!ui_visivel) fechar_submenu();      /* limpa submenu aberto */
    if (grupo_ui) {
        if (ui_visivel) lv_obj_clear_flag(grupo_ui, LV_OBJ_FLAG_HIDDEN);
        else            lv_obj_add_flag(grupo_ui, LV_OBJ_FLAG_HIDDEN);
    }
    ESP_LOGI(TAG, "interface %s", ui_visivel ? "visivel" : "oculta");
}

/* toque fora dos paineis fecha o que estiver aberto */
static void ev_fundo(lv_event_t *e)
{
    /* só fecha se clique foi diretamente na tela, não borbulhou de um botão */
    lv_obj_t *tela = (lv_obj_t *)lv_event_get_user_data(e);
    if (lv_event_get_target(e) != tela) return;
    if (painel_menu)  fechar_submenu();
    if (painel_cores) { lv_obj_del(painel_cores); painel_cores = NULL; }
    for (int i = 0; i < (int)QTD_MENUS; i++) {
        if (!btn_menu[i]) continue;
        btn_mweb_ativo(btn_menu[i], false);
    }
}

/* ---- brasao vitoriano: moldura recortada sob o nome ---- */

/* Anima a cor de um friso: amarelo -> vermelho. Sem circulo, sem bola:
 * o efeito e uma linha ornamental que muda de temperatura. */
static void anim_cor_luz(void *obj, int32_t v)
{
    uint8_t r = 0xFF - (uint8_t)((0xFF - 0xD2) * v / 255);
    uint8_t g = 0xD2 - (uint8_t)((0xD2 - 0x2A) * v / 255);
    uint8_t b = 0x4A - (uint8_t)((0x4A - 0x12) * v / 255);
    lv_obj_set_style_bg_color((lv_obj_t *)obj, lv_color_make(r, g, b), LV_PART_MAIN);
}

/* ---- montagem da tela ---- */

void palco_montar(lv_obj_t *tela)
{
    qtd_lbl = 0;

    /* tela CLICKABLE: CLICKED dispara no fundo (target==tela → ev_fundo)
       e borbulha dos botões (target!=tela → ev_fundo ignora) */
    lv_obj_add_flag(tela, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tela, ev_fundo, LV_EVENT_CLICKED, (void *)tela);

    /* engrenagem no canto superior direito: fica SEMPRE visivel */
    lv_obj_t *eng = lv_obj_create(tela);
    lv_obj_remove_style_all(eng);
    lv_obj_set_size(eng, 46, 46);
    lv_obj_align(eng, LV_ALIGN_TOP_RIGHT, -10, 8);
    lv_obj_set_style_radius(eng, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(eng, lv_color_hex(0x2A1E08), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(eng, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_width(eng, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(eng, lv_color_hex(OURO), LV_PART_MAIN);
    lv_obj_set_style_shadow_color(eng, lv_color_hex(AMBAR), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(eng, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(eng, LV_OPA_40, LV_PART_MAIN);
    lv_obj_add_flag(eng, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(eng, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(eng, ev_engrenagem, LV_EVENT_CLICKED, NULL);

    lv_obj_t *ei = texto(eng, LV_SYMBOL_SETTINGS, &lv_font_montserrat_24, OURO_CLARO);
    lv_obj_center(ei);

    /* grupo que a engrenagem oculta: cobre a tela sem fundo proprio */
    grupo_ui = lv_obj_create(tela);
    lv_obj_remove_style_all(grupo_ui);
    lv_obj_set_size(grupo_ui, 800, 480);
    lv_obj_set_pos(grupo_ui, 0, 0);
    lv_obj_set_style_bg_opa(grupo_ui, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(grupo_ui, LV_OBJ_FLAG_SCROLLABLE);
    if (!ui_visivel) lv_obj_add_flag(grupo_ui, LV_OBJ_FLAG_HIDDEN);  /* comeca oculto */
    /* grupo_ui CLICKABLE: sem isso LVGL não despacha CLICKED pros botões filhos.
       ev_fundo aqui ignora cliques que vieram de botões via target check */
    lv_obj_add_event_cb(grupo_ui, ev_fundo, LV_EVENT_CLICKED, (void *)grupo_ui);
    lv_obj_move_foreground(eng);   /* engrenagem acima do grupo_ui: 1o toque abre, 2o esconde */

    /* nome do preset em fonte grande, logo abaixo do banner */
    lbl_preset = texto(grupo_ui, preset_nome, estilos[estilo_atual].grande, cor_atual);
    lv_obj_set_style_text_letter_space(lbl_preset, 4, LV_PART_MAIN);
    lv_obj_align(lbl_preset, LV_ALIGN_TOP_MID, 0, 100);

    /* toque no nome abre o teclado */
    lv_obj_t *hit = lv_obj_create(grupo_ui);
    lv_obj_remove_style_all(hit);
    lv_obj_set_size(hit, 300, 44);
    lv_obj_align(hit, LV_ALIGN_TOP_MID, 0, 92);
    lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(hit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(hit, abrir_teclado, LV_EVENT_CLICKED, NULL);

    /* barra de menus no TOPO (igual web .menubar): sem moldura, so os botoes */
    lv_obj_t *barra = lv_obj_create(grupo_ui);
    lv_obj_remove_style_all(barra);
    lv_obj_set_size(barra, 706, 54);
    lv_obj_align(barra, LV_ALIGN_TOP_LEFT, 4, 4);   /* engrenagem livre a direita */
    lv_obj_clear_flag(barra, LV_OBJ_FLAG_SCROLLABLE);

    const int lg = 96;
    for (int i = 0; i < (int)QTD_MENUS; i++) {
        lv_obj_t *b = btn_mweb(barra, menus[i].titulo, &fonte_cinzel_22, lg, 42);
        lv_obj_set_pos(b, 6 + i * (lg + 8), 6);
        lv_obj_add_event_cb(b, ev_menu, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        btn_menu[i] = b;
    }

    /* strip de pedais estilo web: 8 slots na base da tela */
    for (int s = 0; s < SLOT_COUNT; s++) {
        lv_obj_t *sl = lv_obj_create(grupo_ui);
        lv_obj_remove_style_all(sl);
        lv_obj_set_size(sl, 196, 132);
        lv_obj_set_pos(sl, 4 + s * 99, 480 - 132);
        lv_obj_set_style_radius(sl, 10, LV_PART_MAIN);
        /* slot transparente (igual web .pedal-slot border:none, sem fundo) */
        lv_obj_set_style_bg_opa(sl, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_add_flag(sl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(sl, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(sl, ev_pedal_slot, LV_EVENT_CLICKED, (void *)(intptr_t)s);
        slot_web[s] = sl;
    }

    palco_set_wifi(false);

    /* bancos NVS: carrega o salvo e aplica o ativo (nome/amp/pedais) */
    bancos_carregar();
    banco_aplicar();

    ESP_LOGI(TAG, "palco montado: %d menus", (int)QTD_MENUS);
}
