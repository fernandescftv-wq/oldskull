#ifndef OLDSKULL_PEDAL_H
#define OLDSKULL_PEDAL_H

#include "lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void oldskull_pedal_create(lv_obj_t *parent);
void oldskull_pedal_show(void);
void oldskull_pedal_set_rotation(int angle);
void oldskull_pedal_set_scale(uint16_t scale);

#ifdef __cplusplus
}
#endif

#endif
