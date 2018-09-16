/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 "Eric Poulsen" <eric@zyxod.com>
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
#include <string.h>

#include <time.h>
#include <sys/time.h>
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "driver/gpio.h"
#include "esp_wifi.h"

#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "timeutils.h"
#include "modmachine.h"
#include "machine_rtc.h"
#include "modesp32.h"

STATIC mp_obj_t esp32_lightsleep_wake_on_gpio(const mp_obj_t wake) {

    if (machine_rtc_config.wake_on_touch || machine_rtc_config.wake_on_ulp) {
        mp_raise_ValueError("no resources");
    }

    machine_rtc_config.ls_wake_on_gpio = mp_obj_is_true(wake);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp32_lightsleep_wake_on_gpio_obj, esp32_lightsleep_wake_on_gpio);

STATIC mp_obj_t esp32_lightsleep_wake_on_uart(const mp_obj_t ls_uart_num_in) {
    machine_rtc_config.ls_uart_num = mp_obj_get_int(ls_uart_num_in);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp32_lightsleep_wake_on_uart_obj, esp32_lightsleep_wake_on_uart);

STATIC mp_obj_t esp32_wake_on_ulp(const mp_obj_t wake) {
    if (machine_rtc_config.rtc_periph_force_on) {
        mp_raise_ValueError("no resources");
    }
    machine_rtc_config.wake_on_ulp = mp_obj_is_true(wake);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp32_wake_on_ulp_obj, esp32_wake_on_ulp);

STATIC mp_obj_t esp32_wake_on_touch(const mp_obj_t wake) {

    if (machine_rtc_config.ext0_pin != -1 || machine_rtc_config.ls_wake_on_gpio) {
        mp_raise_ValueError("no resources");
    }
    //nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "touchpad wakeup not available for this version of ESP-IDF"));

    machine_rtc_config.wake_on_touch = mp_obj_is_true(wake);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp32_wake_on_touch_obj, esp32_wake_on_touch);

STATIC mp_obj_t esp32_wake_on_ext0(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    if (machine_rtc_config.wake_on_touch) {
        mp_raise_ValueError("no resources");
    }
    enum {ARG_pin, ARG_level};
    const mp_arg_t allowed_args[] = {
        { MP_QSTR_pin,  MP_ARG_OBJ, {.u_obj = mp_obj_new_int(machine_rtc_config.ext0_pin)} },
        { MP_QSTR_level,  MP_ARG_BOOL, {.u_bool = machine_rtc_config.ext0_level} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (args[ARG_pin].u_obj == mp_const_none) {
        machine_rtc_config.ext0_pin = -1; // "None"
    } else {
        gpio_num_t pin_id = machine_pin_get_id(args[ARG_pin].u_obj);
        if (pin_id != machine_rtc_config.ext0_pin) {
            if (!RTC_IS_VALID_EXT_PIN(pin_id)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid pin"));
            }
            machine_rtc_config.ext0_pin = pin_id;
        }
    }

    machine_rtc_config.ext0_level = args[ARG_level].u_bool;
    machine_rtc_config.ext0_wake_types = MACHINE_WAKE_SLEEP | MACHINE_WAKE_DEEPSLEEP;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(esp32_wake_on_ext0_obj, 0, esp32_wake_on_ext0);

STATIC mp_obj_t esp32_wake_on_ext1(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum {ARG_pins, ARG_level};
    const mp_arg_t allowed_args[] = {
        { MP_QSTR_pins, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_level, MP_ARG_BOOL, {.u_bool = machine_rtc_config.ext1_level} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    uint64_t ext1_pins = machine_rtc_config.ext1_pins;


    // Check that all pins are allowed
    if (args[ARG_pins].u_obj != mp_const_none) {
        mp_uint_t len = 0;
        mp_obj_t *elem;
        mp_obj_get_array(args[ARG_pins].u_obj, &len, &elem);
        ext1_pins = 0;

        for (int i = 0; i < len; i++) {

            gpio_num_t pin_id = machine_pin_get_id(elem[i]);
            if (!RTC_IS_VALID_EXT_PIN(pin_id)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid pin"));
                break;
            }
            ext1_pins |= (1ll << pin_id);
        }
    }

    if (machine_rtc_config.ext1_pins !=0 && machine_rtc_config.ls_wake_on_gpio) {
        mp_raise_ValueError("no resources");
    }
    machine_rtc_config.ext1_level = args[ARG_level].u_bool;
    machine_rtc_config.ext1_pins = ext1_pins;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(esp32_wake_on_ext1_obj, 0, esp32_wake_on_ext1);

STATIC mp_obj_t esp32_raw_temperature(void) {
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_SAR, 3, SENS_FORCE_XPD_SAR_S);
    SET_PERI_REG_BITS(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_CLK_DIV, 10, SENS_TSENS_CLK_DIV_S);
    CLEAR_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_POWER_UP);
    CLEAR_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_DUMP_OUT);
    SET_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_POWER_UP_FORCE);
    SET_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_POWER_UP);
    ets_delay_us(100);
    SET_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_DUMP_OUT);
    ets_delay_us(5);
    int res = GET_PERI_REG_BITS2(SENS_SAR_SLAVE_ADDR3_REG, SENS_TSENS_OUT, SENS_TSENS_OUT_S);

    return mp_obj_new_int(res);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(esp32_raw_temperature_obj, esp32_raw_temperature);

STATIC mp_obj_t esp32_sleep_pd_config(const mp_obj_t domain_in, const mp_obj_t option_in) {
    esp_sleep_pd_domain_t domain = mp_obj_get_int(domain_in);
    esp_sleep_pd_option_t option = mp_obj_get_int(option_in);

    if (domain == ESP_PD_DOMAIN_RTC_PERIPH && 
        option == ESP_PD_OPTION_ON &&
        machine_rtc_config.wake_on_ulp) {
            mp_raise_ValueError("no resources");
        }

    if (esp_sleep_pd_config(domain, option) != ESP_OK) {
        mp_raise_ValueError("invalid arugment");
    }

    if (domain == ESP_PD_DOMAIN_RTC_PERIPH) {
        machine_rtc_config.rtc_periph_force_on = (option == ESP_PD_OPTION_ON);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(esp32_sleep_pd_config_obj, esp32_sleep_pd_config);

// wifi_power_save([value])
STATIC mp_obj_t esp32_wifi_power_save_mode(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        // get
        wifi_ps_type_t type;
        esp_wifi_get_ps(&type);
        return mp_obj_new_int(type);
    } else {
        // set
        esp_wifi_set_ps(mp_obj_get_int(args[0]));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(esp32_wifi_power_save_mode_obj, 0, 1, esp32_wifi_power_save_mode);

extern const mp_obj_module_t mp_module_rtcio;

STATIC const mp_rom_map_elem_t esp32_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_esp32) },

    { MP_ROM_QSTR(MP_QSTR_wake_on_touch), MP_ROM_PTR(&esp32_lightsleep_wake_on_gpio_obj) },
    { MP_ROM_QSTR(MP_QSTR_wake_on_touch), MP_ROM_PTR(&esp32_lightsleep_wake_on_uart_obj) },
    { MP_ROM_QSTR(MP_QSTR_wake_on_touch), MP_ROM_PTR(&esp32_wake_on_touch_obj) },
    { MP_ROM_QSTR(MP_QSTR_wake_on_ext0), MP_ROM_PTR(&esp32_wake_on_ext0_obj) },
    { MP_ROM_QSTR(MP_QSTR_wake_on_ext1), MP_ROM_PTR(&esp32_wake_on_ext1_obj) },
    { MP_ROM_QSTR(MP_QSTR_wake_on_ulp), MP_ROM_PTR(&esp32_wake_on_ulp_obj) },
    { MP_ROM_QSTR(MP_QSTR_lightsleep_wake_on_gpio), MP_ROM_PTR(&esp32_lightsleep_wake_on_gpio_obj) },
    { MP_ROM_QSTR(MP_QSTR_lightsleep_wake_on_uart), MP_ROM_PTR(&esp32_lightsleep_wake_on_uart_obj) },
    { MP_ROM_QSTR(MP_QSTR_raw_temperature), MP_ROM_PTR(&esp32_raw_temperature_obj) },
    { MP_ROM_QSTR(MP_QSTR_sleep_pd_config), MP_ROM_PTR(&esp32_sleep_pd_config_obj) },
    { MP_ROM_QSTR(MP_QSTR_wifi_power_save), MP_ROM_PTR(&esp32_wifi_power_save_mode_obj) },

    { MP_ROM_QSTR(MP_QSTR_ULP), MP_ROM_PTR(&esp32_ulp_type) },
    { MP_ROM_QSTR(MP_QSTR_RTCPin), MP_ROM_PTR(&esp32_rtcio_type) },
    { MP_ROM_QSTR(MP_QSTR_UART0), MP_ROM_PTR(&uart0_type) },

    { MP_ROM_QSTR(MP_QSTR_WAKEUP_ALL_LOW), MP_ROM_PTR(&mp_const_false_obj) },
    { MP_ROM_QSTR(MP_QSTR_WAKEUP_ANY_HIGH), MP_ROM_PTR(&mp_const_true_obj) },
    { MP_ROM_QSTR(MP_QSTR_WIFI_PS_NONE), MP_ROM_INT(WIFI_PS_NONE) },
    { MP_ROM_QSTR(MP_QSTR_WIFI_PS_MIN_MODEM), MP_ROM_INT(WIFI_PS_MIN_MODEM) },
    { MP_ROM_QSTR(MP_QSTR_WIFI_PS_MAX_MODEM), MP_ROM_INT(WIFI_PS_MAX_MODEM) },
    { MP_ROM_QSTR(MP_QSTR_PD_OPTION_OFF), MP_ROM_INT(ESP_PD_OPTION_OFF) },
    { MP_ROM_QSTR(MP_QSTR_PD_OPTION_ON), MP_ROM_INT(ESP_PD_OPTION_ON) },
    { MP_ROM_QSTR(MP_QSTR_PD_OPTION_AUTO), MP_ROM_INT(ESP_PD_OPTION_AUTO) },
    { MP_ROM_QSTR(MP_QSTR_PD_DOMAIN_RTC_PERIPH), MP_ROM_INT(ESP_PD_DOMAIN_RTC_PERIPH) },
    { MP_ROM_QSTR(MP_QSTR_PD_DOMAIN_RTC_SLOW_MEM), MP_ROM_INT(ESP_PD_DOMAIN_RTC_SLOW_MEM) },
    { MP_ROM_QSTR(MP_QSTR_PD_DOMAIN_RTC_FAST_MEM), MP_ROM_INT(ESP_PD_DOMAIN_RTC_FAST_MEM) },
    { MP_ROM_QSTR(MP_QSTR_PD_DOMAIN_XTAL), MP_ROM_INT(ESP_PD_DOMAIN_XTAL) },
};

STATIC MP_DEFINE_CONST_DICT(esp32_module_globals, esp32_module_globals_table);

const mp_obj_module_t esp32_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&esp32_module_globals,
};
