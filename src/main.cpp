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
    { -20,  20,  20 },
    {  20,  20,  20 },
    { -20, -20,  20 },
    {  20, -20,  20 },
    { -20,  20, -20 },
    {  20,  20, -20 },
    { -20, -20, -20 },
    {  20, -20, -20 },
};

// Cube polygons (lines between corners + colour)
//
#if opt_colour == 0

int shape[6][5] = {
    { 0,1,3,2, 1 },
    { 6,7,5,4, 2 },
    { 1,5,7,3, 3 },
    { 2,6,4,0, 4 },
    { 2,3,7,6, 5 },
    { 0,4,5,1, 6 },
};

unsigned char col_mandelbrot[16] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

#else

int shape[6][5] = {
    { 0,1,3,2, col_red },
    { 6,7,5,4, col_green },
    { 1,5,7,3, col_blue },
    { 2,6,4,0, col_magenta },
    { 2,3,7,6, col_cyan },
    { 0,4,5,1, col_yellow },
};

unsigned char col_mandelbrot[16] = { 
    rgb(0,0,0),
    rgb(1,0,0),
    rgb(2,0,0),
    rgb(3,0,0),
    rgb(4,0,0),
    rgb(5,0,0),
    rgb(6,0,0),
    rgb(7,0,0),
    rgb(7,0,0),
    rgb(7,1,0),
    rgb(7,2,0),
    rgb(7,3,0),
    rgb(7,4,0),
    rgb(7,5,0),
    rgb(7,6,0),
    rgb(7,7,0)
};

#endif


#if VIDEO_NTSC
#define AD724_CLOCK_FREQ 3579545 // NTSC
#else
#define AD724_CLOCK_FREQ 4433618 // PAL
#endif
#define AD724_CLOCK_PIN 29
#define AD724_CLOCK_SM 2

void setup(){
    // Initialize the AD724 clock on pin 29
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ad724_clock_program);
    ad724_clock_init(pio, AD724_CLOCK_SM, offset, AD724_CLOCK_PIN, AD724_CLOCK_FREQ);
    
    initialise_cvideo();    // Initialise the composite video stuff
    set_mode(1);
}

void loop(){
    //demo_splash();

    //demo_spinny_cube();
    //demo_mandlebrot(); 
    
    draw_screen_border(col_white);
    draw_random(col_white);
    //demo_horizontal_sweep();
   
    wait_vblank();
    swap_bitmap();
    cls(0);
}

void demo_splash() {
    cls(0);
    blit(&sample_bitmap, 0, 0, 256, 192, (width - 256) / 2, 0);
    set_border(col_white);
    print_string(60, 8, "Pico-mposite v"version, col_blue, col_white);
    print_string(64, 24, "By Dean Belfield", col_green, col_white);
    print_string(24, 180, "www.breakintoprogram.co.uk", col_red, col_white);    

    sleep_ms(10000);
}

// Demo: Spinning 3D cube
//
void demo_spinny_cube() {
    double the = 0;
    double psi = 0;
    double phi = 0;

    set_border(col_white);

    for(int i = 0; i < 1000; i++) {
        wait_vblank();
        cls(col_white);

        print_string(0, height-10, "Pico-mposite Graphics Primitives", col_red, col_white); 
        draw_circle(128, 96, 80, i >= 500 ? col_grey : col_black, i >= 500);
        render_spinny_cube(0, 0, the, psi, phi, i >= 500);
        the += 0.01;
        psi += 0.03;
        phi -= 0.02;
    }
}

// Demo: Mandlebrot set
//
void demo_mandlebrot() {
    cls(col_black);
    set_border(col_white);
    render_mandlebrot();
    #if opt_colour == 0
    print_string(16, 180, "Pico-mposite Mandlebrot Demo", 0, 15);
    #else
    print_string(16, 180, "Pico-mposite Mandlebrot Demo", col_red, col_white);
    #endif
    sleep_ms(10000);
}

// Draw a 3D cube
// xo: X position in view
// yo: Y position in view
// the, psi, phi: Rotation angles
// colour: Pixel colour
//
void render_spinny_cube(int xo, int yo, double the, double psi, double phi, bool filled) {
    int i;
    double x, y, z, xx, yy, zz;
    int a[8], b[8];
    int xd =0, yd = 0;
    int x1, y1, x2, y2, x3, y3, x4, y4;
    double sd = 512, od = 256;

    for(i = 0; i < 8 ; i++) {
        xx = shape_pts[i][0];
        yy = shape_pts[i][1];
        zz = shape_pts[i][2];

         y = yy * cos(phi) - zz * sin(phi);
        zz = yy * sin(phi) + zz * cos(phi);
         x = xx * cos(the) - zz * sin(the);
        zz = xx * sin(the) + zz * cos(the);
        xx =  x * cos(psi) -  y * sin(psi);
        yy =  x * sin(psi) +  y * cos(psi);

        xx += xo + xd;
        yy += yo + yd;

        a[i] = 128 + xx * sd / (od - zz);
        b[i] =  96 + yy * sd / (od - zz);
    }

    for(i = 0; i < 6; i++) {
        x1 = a[shape[i][0]];
        x2 = a[shape[i][1]];
        x3 = a[shape[i][2]];
        x4 = a[shape[i][3]];
        y1 = b[shape[i][0]];
        y2 = b[shape[i][1]];
        y3 = b[shape[i][2]];
        y4 = b[shape[i][3]];

        if(x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2) <= 0) {
            draw_polygon(x1, y1, x2, y2, x3, y3, x4, y4, shape[i][4], filled);
        }
    }
}

// Draw a Mandlebrot
//
void render_mandlebrot(void) {
    int k = 0;
    float i , j , r , x , y;
    for(y = 0; y < height; y++) {
        for(x = 0; x < width; x++) {
            plot(x, y, col_mandelbrot[k]);
            for(i = k = r = 0; j = r * r - i * i - 2 + x / 100, i = 2 * r * i + (y - 96) / 70, j * j + i * i < 11 && k++ < 15; r = j);
        }
    }
}

void demo_horizontal_sweep() {
    static int y = 80;
    static int dir =1;
    //cls(col_black);
    draw_horizontal_line(y, 0, width - 1, col_white);
    set_border(col_white);
    //wait_vblank();
    y += dir;
    if (y >= height) {
        y = 0;
    }
}

void draw_screen_border(unsigned char color) {
    int w = 20;
    // Borde superior
    draw_polygon(0, 0, width-1, 0, width-1, w-1, 0, w-1, color, true);
    // Borde inferior
    draw_polygon(0, height-w, width-1, height-w, width-1, height-1, 0, height-1, color, true);
    // Borde izquierdo
    draw_polygon(0, w, w-1, w, w-1, height-w-1, 0, height-w-1, color, true);
    // Borde derecho
    draw_polygon(width-w, w, width-1, w, width-1, height-w-1, width-w, height-w-1, color, true);
}

void draw_random(unsigned char color) {
    int w = 20;
   draw_polygon(random(0,width), random(0,height), random(0,width), random(0,height), random(0,width),random(0,width), random(0,height), w-1, color, true);
}
