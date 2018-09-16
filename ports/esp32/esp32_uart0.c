/*
 * Calling machine.UART(0) in MicroPython esp32 port will cause panic.
 * This may be due to the use of high-level UART API from ESP-IDF interfering 
 * with low level UART ISR handler.
 *
 * This dirty hack enables reading from UART(0) when REPL is blocking, similar to
 * how things work in esp8266.
 *
 * Until The Micropython Project <http://micropython.org/> has come up with a way 
 * to deal with low level UART functionality or reroute its internal logic 
 * specifically for UART(0), this hack is necessary for my micropython-ainput 
 * library <https://github.com/shawwwn/micropython-ainput>.
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
#include <stdint.h>
#include <string.h>

#include "rom/uart.h"
#include "uart.h"

#include "py/ringbuf.h"
#include "py/runtime.h"
#include "py/mperrno.h"
#include "modmachine.h"
#include "mphalport.h"

typedef struct _uart0_obj_t {
    mp_obj_base_t base;
} uart0_obj_t;

const mp_obj_type_t uart0_type;

STATIC const uart0_obj_t esp32_ulp_obj = {{&uart0_type}}; // singleton

STATIC mp_obj_t uart0_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    return (mp_obj_t)&esp32_ulp_obj;
}

STATIC void uart0_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mp_printf(print, "UART0");
}

STATIC mp_obj_t uart0_any(mp_obj_t self_in) {
    if (stdin_ringbuf.iget != stdin_ringbuf.iput) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(uart0_any_obj, uart0_any);

// TODO: read multiple chars
STATIC mp_obj_t uart0_read(size_t n_args, const mp_obj_t *args) {
    int c = ringbuf_get(&stdin_ringbuf);
    if (c != -1) {
        return mp_obj_new_str((const char *)&c, 1);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(uart0_read_obj, 1, 2, uart0_read);

STATIC mp_obj_t uart0_init(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(uart0_init_obj, 1, uart0_init);

STATIC const mp_rom_map_elem_t uart0_globals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_UART0) },
    { MP_ROM_QSTR(MP_QSTR_any), MP_ROM_PTR(&uart0_any_obj) },
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&uart0_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&uart0_init_obj) },
};
STATIC MP_DEFINE_CONST_DICT(uart0_globals_dict, uart0_globals_dict_table);

const mp_obj_type_t uart0_type = {
    { &mp_type_type },
    .name = MP_QSTR_UART0,
    .print = uart0_print,
    .make_new = uart0_make_new,
    .locals_dict = (mp_obj_t)&uart0_globals_dict,
};
