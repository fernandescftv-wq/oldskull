#include "oldskull_pedal.h"
extern const lv_img_dsc_t oldskull_pedal;
#include "oldskull_pedal.h"

/*
 * OLD SKULL PEDAL - BACKUP ESCRITO NO CHAT
 *
 * Estado atual:
 * - Imagem carregando
 * - Resolução/orientação ainda pode estar errada
 * - Código mantido simples para facilitar recuperação
 *
 * LVGL usa:
 * - escala 256 = 100%
 * - rotação em décimos de grau:
 *   900  = 90 graus
 *   1800 = 180 graus
 *   2700 = 270 graus
 */

static lv_obj_t *oldskull_img = NULL;

static int oldskull_rotation = 0;
static uint16_t oldskull_scale = 256;

static void oldskull_apply_transform(void)
{
    if (oldskull_img == NULL) {
        return;
    }

    lv_img_set_zoom(oldskull_img, oldskull_scale);
    lv_img_set_angle(oldskull_img, oldskull_rotation);

    /*
     * Pivô no centro real da imagem.
     */
    lv_img_set_pivot(
        oldskull_img,
        oldskull_pedal.header.w / 2,
        oldskull_pedal.header.h / 2
    );

    lv_obj_center(oldskull_img);
}

void oldskull_pedal_create(lv_obj_t *parent)
{
    if (parent == NULL) {
        parent = lv_scr_act();
    }

    /*
     * Fundo preto
     */
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    /*
     * Cria imagem
     */
    oldskull_img = lv_img_create(parent);
    lv_img_set_src(oldskull_img, &oldskull_pedal);

    /*
     * Remove estilos extras
     */
    lv_obj_set_style_border_width(oldskull_img, 0, 0);
    lv_obj_set_style_pad_all(oldskull_img, 0, 0);
    lv_obj_set_style_bg_opa(oldskull_img, LV_OPA_TRANSP, 0);

    oldskull_apply_transform();
}

void oldskull_pedal_set_rotation(int angle)
{
    oldskull_rotation = angle;
    oldskull_apply_transform();
}

void oldskull_pedal_set_scale(uint16_t scale)
{
    oldskull_scale = scale;
    oldskull_apply_transform();
}

void oldskull_pedal_show(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_clean(scr);

    oldskull_img = NULL;

    oldskull_pedal_create(scr);
}
