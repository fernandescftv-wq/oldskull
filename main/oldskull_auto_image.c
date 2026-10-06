#include <stdint.h>
#include "oldskull_auto_image.h"

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_LARGE_CONST
#endif

#ifndef LV_ATTRIBUTE_IMG_OLDSKULL_AUTO_IMAGE
#define LV_ATTRIBUTE_IMG_OLDSKULL_AUTO_IMAGE
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_OLDSKULL_AUTO_IMAGE uint8_t oldskull_auto_image_map[] = {
    0x00, 0x00, 0x00, 0x00
};

const lv_img_dsc_t oldskull_auto_image = {
    .header.always_zero = 0,
    .header.w = 1,
    .header.h = 1,
    .data_size = 4,
    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,
    .data = oldskull_auto_image_map,
};
