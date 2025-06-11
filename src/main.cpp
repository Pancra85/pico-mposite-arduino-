#include <Arduino.h>

#include <stdlib.h>
#include <math.h>

#include "memory.h"
#include "pico/stdlib.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "main.h"
#include "bitmap.h"
#include "graphics.h"
#include "cvideo.h"
#include "../lib/pico-mposite/ad724_clock.pio.h"

// Cube corner points
//
int shape_pts[8][8] = {
    {-20, 20, 20},
    {20, 20, 20},
    {-20, -20, 20},
    {20, -20, 20},
    {-20, 20, -20},
    {20, 20, -20},
    {-20, -20, -20},
    {20, -20, -20},
};

// Cube polygons (lines between corners + colour)
//
#if opt_colour == 0

int shape[6][5] = {
    {0, 1, 3, 2, 1},
    {6, 7, 5, 4, 2},
    {1, 5, 7, 3, 3},
    {2, 6, 4, 0, 4},
    {2, 3, 7, 6, 5},
    {0, 4, 5, 1, 6},
};

unsigned char col_mandelbrot[16] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

#else

int shape[6][5] = {
    {0, 1, 3, 2, col_red},
    {6, 7, 5, 4, col_green},
    {1, 5, 7, 3, col_blue},
    {2, 6, 4, 0, col_magenta},
    {2, 3, 7, 6, col_cyan},
    {0, 4, 5, 1, col_yellow},
};

unsigned char col_mandelbrot[16] = {
    rgb(0, 0, 0),
    rgb(1, 0, 0),
    rgb(2, 0, 0),
    rgb(3, 0, 0),
    rgb(4, 0, 0),
    rgb(5, 0, 0),
    rgb(6, 0, 0),
    rgb(7, 0, 0),
    rgb(7, 0, 0),
    rgb(7, 1, 0),
    rgb(7, 2, 0),
    rgb(7, 3, 0),
    rgb(7, 4, 0),
    rgb(7, 5, 0),
    rgb(7, 6, 0),
    rgb(7, 7, 0)};

#endif

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
    // demo_splash();

    // demo_spinny_cube();
    // demo_mandlebrot();

     draw_screen_border(col_white);
    draw_random(col_white);
    //demo_horizontal_sweep();

    wait_vblank();
    swap_video_buffer();
    clearScreen(0);
    //cls(0);
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
    //drawRectThickness(x,y,w,h,col_white,10);
    drawRect(x,y,w,h,col_white);
    sleep_ms(2000);
}
