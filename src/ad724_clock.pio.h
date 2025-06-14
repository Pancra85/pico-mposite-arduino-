// -------------------------------------------------- //
// This file is autogenerated by pioasm; do not edit! //
// -------------------------------------------------- //

#pragma once

#if !PICO_NO_HARDWARE
#include "hardware/pio.h"
#endif

// ----------- //
// ad724_clock //
// ----------- //

#define ad724_clock_wrap_target 0
#define ad724_clock_wrap 1

static const uint16_t ad724_clock_program_instructions[] = {
            //     .wrap_target
    0xe001, //  0: set    pins, 1                    
    0xe000, //  1: set    pins, 0                    
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program ad724_clock_program = {
    .instructions = ad724_clock_program_instructions,
    .length = 2,
    .origin = -1,
};

static inline pio_sm_config ad724_clock_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + ad724_clock_wrap_target, offset + ad724_clock_wrap);
    return c;
}

#include "hardware/pio.h"
// freq: desired output frequency (e.g. 4430000 for PAL, 3579545 for NTSC)
void ad724_clock_init(PIO pio, uint sm, uint offset, uint pin, float freq) {
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    pio_sm_config c = ad724_clock_program_get_default_config(offset);
    sm_config_set_out_pins(&c, pin, 1);
    sm_config_set_set_pins(&c, pin, 1);
    sm_config_set_clkdiv(&c, (float) (250000000.0f / (freq * 2.0f)) );
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

#endif
