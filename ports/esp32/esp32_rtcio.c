/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 shawwwn <shawwwn1@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#include <stdio.h>

#include "esp_log.h"

#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "soc/rtc_periph.h"

#include "py/runtime.h"
#include "modmachine.h"
#include "modesp32.h"

NORETURN void _rtcio_exceptions(esp_err_t e) {
   switch (e) {
      case ESP_ERR_INVALID_ARG:
        mp_raise_msg(&mp_type_OSError, "not an RTCIO");
      default:
        nlr_raise(mp_obj_new_exception_msg_varg(
          &mp_type_RuntimeError, "RTCIO Unknown Error 0x%04x", e
        ));
   }
}

static inline void rtcio_exceptions(esp_err_t e) {
    if (e != ESP_OK) _rtcio_exceptions(e);
}

#define RTCIO_EXCEPTIONS(x) do { rtcio_exceptions(x); } while (0);

typedef struct _rtcio_obj_t {
    mp_obj_base_t base;
    gpio_num_t gpio_id;
    gpio_num_t rtcio_id;
} rtcio_obj_t;

STATIC const rtcio_obj_t rtcio_obj[] = {
    // base_obj, gpio num, rtcio num
    {{&esp32_rtcio_type}, GPIO_NUM_36, 0},
    {{&esp32_rtcio_type}, GPIO_NUM_37, 1},
    {{&esp32_rtcio_type}, GPIO_NUM_38, 2},
    {{&esp32_rtcio_type}, GPIO_NUM_39, 3},
    {{&esp32_rtcio_type}, GPIO_NUM_34, 4},
    {{&esp32_rtcio_type}, GPIO_NUM_35, 5},
    {{&esp32_rtcio_type}, GPIO_NUM_32, 9},
    {{&esp32_rtcio_type}, GPIO_NUM_33, 8},
    {{&esp32_rtcio_type}, GPIO_NUM_25, 6},
    {{&esp32_rtcio_type}, GPIO_NUM_26, 7},
    {{&esp32_rtcio_type}, GPIO_NUM_27, 17},
    {{&esp32_rtcio_type}, GPIO_NUM_14, 16},
    {{&esp32_rtcio_type}, GPIO_NUM_12, 15},
    {{&esp32_rtcio_type}, GPIO_NUM_13, 14},
    {{&esp32_rtcio_type}, GPIO_NUM_15,13},
    {{&esp32_rtcio_type}, GPIO_NUM_2, 12},
    {{&esp32_rtcio_type}, GPIO_NUM_0, 11},
    {{&esp32_rtcio_type}, GPIO_NUM_4, 10},
};

STATIC mp_obj_t rtcio_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw,
        const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, true);
    gpio_num_t gpio_id = machine_pin_get_id(args[0]);
    const rtcio_obj_t *self = NULL;
    for (int i = 0; i < MP_ARRAY_SIZE(rtcio_obj); i++) {
        if (gpio_id == rtcio_obj[i].gpio_id) { self = &rtcio_obj[i]; break; }
    }
    if (!self) mp_raise_ValueError("not an RTCIO");
    return MP_OBJ_FROM_PTR(self);
    mp_raise_ValueError("Parameter Error");
}

STATIC void rtcio_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    rtcio_obj_t *self = self_in;
    mp_printf(print, "RTCPin(%u, Pin(%u))", self->rtcio_id, self->gpio_id);
}

// return tuple(gpio_num, rtcio_num)
STATIC mp_obj_t rtcio_pin_num(mp_obj_t self_in) {
    rtcio_obj_t *self = self_in;
    mp_obj_tuple_t *t = mp_obj_new_tuple(2, NULL);
    t->items[0] = mp_obj_new_int(self->gpio_id);
    t->items[1] = mp_obj_new_int(self->rtcio_id);
    return MP_OBJ_FROM_PTR(t);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(rtcio_pin_num_obj, rtcio_pin_num);

STATIC mp_obj_t rtcio_active(mp_obj_t self_in, mp_obj_t active_in) {
    rtcio_obj_t *self = self_in;
    gpio_num_t gpio_id = self->gpio_id;
    bool active = mp_obj_is_true(active_in);
    if (active) {
        RTCIO_EXCEPTIONS(rtc_gpio_init(gpio_id));
    } else {
        RTCIO_EXCEPTIONS(rtc_gpio_deinit(gpio_id));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(rtcio_active_obj, rtcio_active);

STATIC mp_obj_t rtcio_value(size_t n_args, const mp_obj_t *args) {
    rtcio_obj_t *self = args[0];
    gpio_num_t gpio_id = self->gpio_id;
    
    if (n_args == 1) {
        // get
        uint32_t value = rtc_gpio_get_level(gpio_id);
        if (value == ESP_ERR_INVALID_ARG) {
            mp_raise_ValueError("not an RTCIO");
        }
        return mp_obj_new_int(value);
    } else {
        // set
        RTCIO_EXCEPTIONS(rtc_gpio_set_level(gpio_id, mp_obj_get_int(args[1])));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rtcio_value_obj, 1, 2, rtcio_value);

STATIC mp_obj_t rtcio_hold(size_t n_args, const mp_obj_t *args) {
    rtcio_obj_t *self = args[0];
    gpio_num_t gpio_id = self->gpio_id;
    
    if (n_args == 1) {
        // get
        return (GET_PERI_REG_MASK(rtc_gpio_desc[gpio_id].reg, rtc_gpio_desc[gpio_id].hold) == 0) ? 
                mp_const_false : 
                mp_const_true;
    } else {
        // set
        if (mp_obj_is_true(args[1])) {
            RTCIO_EXCEPTIONS(rtc_gpio_hold_en(gpio_id));
        } else {
            RTCIO_EXCEPTIONS(rtc_gpio_hold_dis(gpio_id));
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rtcio_hold_obj, 1, 2, rtcio_hold);

STATIC mp_obj_t rtcio_drive(size_t n_args, const mp_obj_t *args) {
    rtcio_obj_t *self = args[0];
    gpio_num_t gpio_id = self->gpio_id;
    
    if (n_args == 1) {
        // get
        gpio_drive_cap_t drive;
        RTCIO_EXCEPTIONS(rtc_gpio_get_drive_capability(gpio_id, &drive));
        return mp_obj_new_int(drive);
    } else {
        // set
        gpio_drive_cap_t drive = mp_obj_get_int(args[1]);
        RTCIO_EXCEPTIONS(rtc_gpio_set_drive_capability(gpio_id, drive));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rtcio_drive_obj, 1, 2, rtcio_drive);

STATIC mp_obj_t rtcio_direction(size_t n_args, const mp_obj_t *args) {
    rtcio_obj_t *self = args[0];
    gpio_num_t gpio_id = self->gpio_id;

    if (n_args == 1) {
        // get
        // TODO: add get()
        mp_raise_ValueError("get not yet supported");
        return mp_const_none;
    } else {
        // set
        rtc_gpio_mode_t mode = mp_obj_get_int(args[1]);
        RTCIO_EXCEPTIONS(rtc_gpio_set_direction(gpio_id, mode));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rtcio_direction_obj, 1, 2, rtcio_direction);

STATIC mp_obj_t rtcio_ls_wake(mp_obj_t self_in, mp_obj_t level_in) {
    rtcio_obj_t *self = self_in;
    gpio_num_t gpio_id = self->gpio_id;

    if (level_in == mp_const_none) {
        // disable
        rtc_gpio_wakeup_disable(gpio_id);
    } else {
        // enable
        gpio_int_type_t level = mp_obj_get_int(level_in);
        RTCIO_EXCEPTIONS(rtc_gpio_wakeup_enable(gpio_id, level));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(rtcio_ls_wake_obj, rtcio_ls_wake);

STATIC mp_obj_t rtcio_pullup(size_t n_args, const mp_obj_t *args) {
    rtcio_obj_t *self = args[0];
    gpio_num_t gpio_id = self->gpio_id;

    if (n_args == 1) {
        // get
        return (GET_PERI_REG_MASK(rtc_gpio_desc[gpio_id].reg, rtc_gpio_desc[gpio_id].pullup) == 0) ?
                mp_const_false : 
                mp_const_true;
    } else {
        // set
        bool enable = mp_obj_is_true(args[1]);
        if (enable) {
            RTCIO_EXCEPTIONS(rtc_gpio_pullup_en(gpio_id));
        } else {
            RTCIO_EXCEPTIONS(rtc_gpio_pullup_dis(gpio_id));
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rtcio_pullup_obj, 1, 2, rtcio_pullup);

STATIC mp_obj_t rtcio_pulldown(size_t n_args, const mp_obj_t *args) {
    rtcio_obj_t *self = args[0];
    gpio_num_t gpio_id = self->gpio_id;

    if (n_args == 1) {
        // get
        return (SET_PERI_REG_MASK(rtc_gpio_desc[gpio_id].reg, rtc_gpio_desc[gpio_id].pulldown) == 0) ?
                mp_const_false : 
                mp_const_true;
    } else {
        // set
        bool enable = mp_obj_is_true(args[1]);
        if (enable) {
            RTCIO_EXCEPTIONS(rtc_gpio_pulldown_en(gpio_id));
        } else {
            RTCIO_EXCEPTIONS(rtc_gpio_pulldown_dis(gpio_id));
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rtcio_pulldown_obj, 1, 2, rtcio_pulldown);

STATIC mp_obj_t rtcio_isolate(mp_obj_t self_in) {
    rtcio_obj_t *self = self_in;
    gpio_num_t gpio_id = self->gpio_id;
    RTCIO_EXCEPTIONS(rtc_gpio_isolate(gpio_id));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(rtcio_isolate_obj, rtcio_isolate);

STATIC mp_obj_t rtcio_force_hold_dis_all() {
    rtc_gpio_force_hold_dis_all();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(rtcio_force_hold_dis_all_obj, rtcio_force_hold_dis_all);
STATIC MP_DEFINE_CONST_STATICMETHOD_OBJ(rtcio_force_hold_dis_all_static_obj, MP_ROM_PTR(&rtcio_force_hold_dis_all_obj));

STATIC const mp_map_elem_t rtcio_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_RTCPin) },
    { MP_ROM_QSTR(MP_QSTR_pin_num), (mp_obj_t)&rtcio_pin_num_obj },
    { MP_ROM_QSTR(MP_QSTR_active), (mp_obj_t)&rtcio_active_obj },
    { MP_ROM_QSTR(MP_QSTR_value), (mp_obj_t)&rtcio_value_obj },
    { MP_ROM_QSTR(MP_QSTR_hold), (mp_obj_t)&rtcio_hold_obj },
    { MP_ROM_QSTR(MP_QSTR_drive), (mp_obj_t)&rtcio_drive_obj },
    { MP_ROM_QSTR(MP_QSTR_direction), (mp_obj_t)&rtcio_direction_obj },
    { MP_ROM_QSTR(MP_QSTR_ls_wake), (mp_obj_t)&rtcio_ls_wake_obj },
    { MP_ROM_QSTR(MP_QSTR_pullup), (mp_obj_t)&rtcio_pullup_obj },
    { MP_ROM_QSTR(MP_QSTR_pulldown), (mp_obj_t)&rtcio_pulldown_obj },
    { MP_ROM_QSTR(MP_QSTR_isolate), (mp_obj_t)&rtcio_isolate_obj },

    { MP_ROM_QSTR(MP_QSTR_force_hold_dis_all), (mp_obj_t)&rtcio_force_hold_dis_all_static_obj },

    { MP_ROM_QSTR(MP_QSTR_GPIO_INTR_LOW_LEVEL), MP_ROM_INT(GPIO_INTR_LOW_LEVEL) },
    { MP_ROM_QSTR(MP_QSTR_GPIO_INTR_HIGH_LEVEL), MP_ROM_INT(GPIO_INTR_HIGH_LEVEL) },
    { MP_ROM_QSTR(MP_QSTR_DRIVE_CAP_WEAK), MP_ROM_INT(GPIO_DRIVE_CAP_0) },
    { MP_ROM_QSTR(MP_QSTR_DRIVE_CAP_STRONGER), MP_ROM_INT(GPIO_DRIVE_CAP_1) },
    { MP_ROM_QSTR(MP_QSTR_DRIVE_CAP_DEFAULT), MP_ROM_INT(GPIO_DRIVE_CAP_2) },
    { MP_ROM_QSTR(MP_QSTR_DRIVE_CAP_STRONGEST), MP_ROM_INT(GPIO_DRIVE_CAP_3) },
    { MP_ROM_QSTR(MP_QSTR_IN), MP_ROM_INT(RTC_GPIO_MODE_INPUT_ONLY) },
    { MP_ROM_QSTR(MP_QSTR_OUT), MP_ROM_INT(RTC_GPIO_MODE_INPUT_OUTPUT) },
};
STATIC MP_DEFINE_CONST_DICT(rtcio_locals_dict, rtcio_locals_dict_table);

const mp_obj_type_t esp32_rtcio_type = {
    { &mp_type_type },
    .name = MP_QSTR_RTCPin,
    .print = rtcio_print,
    .make_new = rtcio_make_new,
    .locals_dict = (mp_obj_t)&rtcio_locals_dict,
};
