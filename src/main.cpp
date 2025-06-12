#include <Arduino.h>

#include <stdlib.h>
#include <math.h>

#include "memory.h"
#include "pico/stdlib.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "main.h"
#include "bitmaps.h"
#include "graphics.h"
#include "cvideo.h"
#include "ad724_clock.pio.h"

#if VIDEO_NTSC
#define AD724_CLOCK_FREQ 3579545 // NTSC
#else
#define AD724_CLOCK_FREQ 4433618 // PAL
#endif
#define AD724_CLOCK_PIN 29
#define AD724_CLOCK_SM 2

void setup()
{
    // Initialize the AD724 clock on pin 29
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ad724_clock_program);
    ad724_clock_init(pio, AD724_CLOCK_SM, offset, AD724_CLOCK_PIN, AD724_CLOCK_FREQ);

    initialise_cvideo(); // Initialise the composite video stuff
    set_mode(1);
}

void loop()
{
     draw_screen_border(col_white);
    draw_random(col_white);


    wait_vblank();
    swap_video_buffer();
    clearScreen(0);
}


void demo_horizontal_sweep()
{
    static int y = 80;
    static int dir =1;
    //cls(col_black);
    drawHLine(0, y, screenWidth - 1, col_white);
    //set_border(col_white);
    //wait_vblank();
    y += dir;
    if (y >= screenHeight) {
        y = 0;
    }
}

void draw_screen_border(unsigned char color)
{
    short w = 20;
    // Borde superior
    drawRect(0, 0, screenWidth, w, color);
    // Borde inferior
    drawRect(0, screenHeight - w, screenWidth, w, color);
    // Borde izquierdo
    drawRect(0, w, w, screenHeight - 2 * w, color);
    // Borde derecho
    drawRect(screenWidth - w, w, w, screenHeight - 2 * w, color);
}

void draw_random(unsigned char color)
{
    short x = random(0, screenWidth - 20);
    short y = random(0, screenHeight - 20);
    short w = random(20, 200);
    short h = w;
    //drawRectThickness(x, y, w, h, color,5);
    // drawHLine(x,y+10,w,col_white);
    // drawVLine(x+w,y+10,h,col_white);
    //drawRectThickness(x,y,w,h,col_white,1);
    drawFillRectRotated(x,y,w,h,col_white,255,random(0,360));
    //drawRectCenter(x,y,w,h,col_white);
    //drawRect(x,y,w,h,col_white);
    //sleep_ms(2000);
}
