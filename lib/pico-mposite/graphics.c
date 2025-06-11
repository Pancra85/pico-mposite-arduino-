//
// Title:	        Pico-mposite Graphics Primitives
// Description:		A hacked-together composite video output for the Raspberry Pi Pico
// Author:	        Dean Belfield
// Created:	        01/02/2021
// Last Updated:	02/03/2022
//
// Modinfo:
// 03/02/2022:      Fixed bug in print_char, typos in comments
// 05/02/2022:      Added support for colour
// 07/02/2022:      Added filled primitives
// 08/07/2022:      Optimised filled circle drawing
// 20/02/2022:      Added scroll_up, bitmap now initialised in cvideo.c
// 02/03/2022:      Added blit
#include <Arduino.h>
#include <math.h>

#include "memory.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "charset.h" // The character set
#include "cvideo.h"

#include "graphics.h"
#include "glcdfont.h"

#include <math.h> //para el cos y sin



// For writing text
#define tabspace 4 // number of spaces for a tab
// For accessing the font library
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))

// For drawing characters
unsigned short cursor_y, cursor_x, textsize;
char textcolor, textbgcolor, wrap;

// Clear the screen
// - c: Background colour to fill screen with
//
void clearScreen(unsigned char c) {  //borra mas rapido esta version
  unsigned int* p = (unsigned int*)screen_bitmap_next;
  for (int i = 0; i < screenHeight * screenWidth / 4; i++) {
    p[i] = 0;
  }
}

// Print a character
// - x: X position on screen (pixels)
// - y: Y position on screen (pixels)
// - c: Character to print (ASCII 32 to 127)
// - bc: Background colour
// - fc: Foreground colour
//
void print_char(int x, int y, int c, unsigned char bc, unsigned char fc)
{
    int char_index;
    unsigned char *ptr;

    if (c >= 32 && c < 128)
    {
        char_index = (c - 32) * 8;
        ptr = &screen_bitmap[screenWidth * y + x + 7];
        for (int row = 0; row < 8; row++)
        {
            unsigned char data = charset[char_index + row];
            for (int bit = 0; bit < 8; bit++)
            {
                *(ptr - bit) = data & 1 << bit ? colour_base + fc : colour_base + bc;
            }
            ptr += screenWidth;
        }
    }
}

// Print a string
// - x: X position on screen (pixels)
// - y: Y position on screen (pixels)
// - s: Zero terminated string
// - bc: Background
// - fc: Foreground colour (
//
void print_string(int x, int y, char *s, unsigned char bc, unsigned char fc)
{
    for (int i = 0; i < strlen(s); i++)
    {
        print_char(x + i * 8, y, s[i], bc, fc);
    }
}

// Plot a point
// - x: X position on screen
// - y: Y position on screen
// - c: Pixel colour
//
void drawPixel(short x, short y, unsigned char c)
{
    if (x >= 0 && x < screenWidth && y >= 0 && y < screenHeight)
    {
        screen_bitmap_next[screenWidth * y + x] = colour_base + c;
    }
}

unsigned char getPixel(short x, short y)
{
    if (x >= 0 && x < screenWidth && y >= 0 && y < screenHeight)
        return screen_bitmap_next[y * screenWidth + x];
    return 0;
}

void drawVLine(short x, short y, short h, unsigned char c)
{
    if (x < 0 || x >= screenWidth || h <= 0)
        return;
    if (y < 0)
    {
        h += y;
        y = 0;
    }
    if (y + h > screenHeight)
        h = screenHeight - y;
    if (h <= 0)
        return;
    for (short i = 0; i < h; i++)
    {
        screen_bitmap_next[(y + i) * screenWidth + x-1] = colour_base + c;
    }
}

void drawHLine(short x, short y, short w, unsigned char c)
{
    if (y < 0 || y >= screenHeight || w <= 0)
        return;
    if (x < 0)
    {
        w += x;
        x = 0;
    }
    if (x + w > screenWidth)
        w = screenWidth - x;
    if (w <= 0)
        return;
    for (short i = 0; i < w; i++)
    {
        screen_bitmap_next[y * screenWidth + x + i] = colour_base + c;
    }
}

void drawRectCenter(short x, short y, short w, short h, char c)
{
    drawRect(x - (w >> 1), y - (h >> 1), w, h, c);
}

void drawLineThickness(short x0, short y0, short x1, short y1, unsigned char c, short thickness)
{
    if (thickness <= 1)
    {
        drawLine(x0, y0, x1, y1, c);
        return;
    }
    short dx = abs(x1 - x0), dy = abs(y1 - y0);
    short signX = x0 < x1 ? 1 : -1;
    short signY = y0 < y1 ? 1 : -1;
    short error = dx - dy;
    short x = x0, y = y0;
    while (x != x1 || y != y1)
    {
        for (short tx = -thickness / 2; tx <= thickness / 2; tx++)
        {
            for (short ty = -thickness / 2; ty <= thickness / 2; ty++)
            {
                drawPixel(x + tx, y + ty, c);
            }
        }
        short e2 = 2 * error;
        if (e2 > -dy)
        {
            error -= dy;
            x += signX;
        }
        if (e2 < dx)
        {
            error += dx;
            y += signY;
        }
    }
}

void fillRect(short x, short y, short w, short h, char c)
{
    if (w < 0 || h < 0)
        return;
    if (x < 0)
    {
        w += x;
        x = 0;
    }
    if (y < 0)
    {
        h += y;
        y = 0;
    }
    if (x + w > screenWidth)
        w = screenWidth - x;
    if (y + h > screenHeight)
        h = screenHeight - y;
    if (w <= 0 || h <= 0)
        return;
    for (short j = 0; j < h; j++)
    {
        memset(&screen_bitmap_next[(y + j) * screenWidth + x], colour_base + c, w);
    }
}

// For drawLine
void swapNumb(short *a, short *b)
{
    short t = *a;
    *a = *b;
    *b = t;
}

// Bresenham's algorithm - thx wikipedia and thx Bruce!
void drawLine(short x0, short y0, short x1, short y1, char color)
{
    short steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep)
    {
        swapNumb(&x0, &y0);
        swapNumb(&x1, &y1);
    }

    if (x0 > x1)
    {
        swapNumb(&x0, &x1);
        swapNumb(&y0, &y1);
    }

    short dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    short err = dx / 2;
    short ystep;

    if (y0 < y1)
    {
        ystep = 1;
    }
    else
    {
        ystep = -1;
    }

    short y = y0;
    for (short x = x0; x <= x1; x++)
    {
        if (steep)
            drawPixel(y, x, color);
        else
            drawPixel(x, y, color);
        err -= dy;
        if (err < 0)
        {
            y += ystep;
            err += dx;
        }
    }
}

void drawRect(short x, short y, short w, short h, char color)
{
    if (w < 0 || h < 0)
        return;
    drawHLine(x, y, w, color);
    drawHLine(x, y + h , w, color);
    drawVLine(x, y, h, color);
    drawVLine(x + w , y, h, color);
}

void drawRectTransparency(short x, short y, short w, short h, char color, uint8_t thickness, int transparency)
{
    if (w < 0 || h < 0)
        return;
    short halfWidth = (w >> 1);
    short halfHeight = (h >> 1);
    // Verificar transparencia
    if (transparency <= 0)
    {
        return;
    }
    transparency = constrain(transparency, 0, 255);

    // Si el grosor es 1, usar la lógica anterior
    if (thickness == 1)
    {
        // Si es totalmente opaco, usar la versión original
        if (transparency == 255)
        {
            drawHLine(x - halfWidth, y - halfHeight, w, color);
            drawHLine(x - halfWidth, y + h - 1 - halfHeight, w, color);
            drawVLine(x - halfWidth, y - halfHeight, h, color);
            drawVLine(x + w - 1 - halfWidth, y - halfHeight, h, color);
            return;
        }

        // Aplicar dithering a las líneas horizontales
        int threshold;

        // Línea horizontal superior
        threshold = (transparency * (bayerMatrixMax + 1)) >> 8;
        if (threshold >= bayerMatrixY[y % bayerMatrixSize])
        {
            drawHLine(x - halfWidth, y - halfHeight, w, color);
        }

        // Línea horizontal inferior
        threshold = (transparency * (bayerMatrixMax + 1)) >> 8;
        if (threshold >= bayerMatrixY[(y + h - 1) % bayerMatrixSize])
        {
            drawHLine(x - halfWidth, y + h - 1 - halfHeight, w, color);
        }

        // Líneas verticales
        for (int j = y; j < (y + h); j++)
        {
            threshold = (transparency * (bayerMatrixMax + 1)) >> 8;
            if (threshold >= bayerMatrixY[j % bayerMatrixSize])
            {
                // Dibujar los puntos de las líneas verticales
                drawPixel(x - halfWidth, j - halfHeight, color);         // Línea izquierda
                drawPixel(x + w - 1 - halfWidth, j - halfHeight, color); // Línea derecha
            }
        }
    }
    // Si el grosor es mayor que 1
    else
    {
        // Dibujar múltiples rectángulos concéntricos
        for (uint8_t i = 0; i < thickness; i++)
        {
            // Si es totalmente opaco, usar la versión sin transparencia
            if (transparency == 255)
            {
                drawHLine(x + i - halfWidth, y + i - halfHeight, w - (2 * i), color);
                drawHLine(x + i - halfWidth, y + h - 1 - i - halfHeight, w - (2 * i), color);
                drawVLine(x + i - halfWidth, y + i - halfHeight, h - (2 * i), color);
                drawVLine(x + w - 1 - i - halfWidth, y + i - halfHeight, h - (2 * i), color);
            }
            else
            {
                // Líneas horizontales con transparencia
                int threshold;

                // Línea superior
                threshold = (transparency * (bayerMatrixMax + 1)) >> 8;
                if (threshold >= bayerMatrixY[(y + i) % bayerMatrixSize])
                {
                    drawHLine(x + i - halfWidth, y + i - halfHeight, w - (2 * i), color);
                }

                // Línea inferior
                threshold = (transparency * (bayerMatrixMax + 1)) >> 8;
                if (threshold >= bayerMatrixY[(y + h - 1 - i) % bayerMatrixSize])
                {
                    drawHLine(x + i - halfWidth, y + h - 1 - i - halfHeight, w - (2 * i), color);
                }

                // Líneas verticales
                for (int j = y + i; j < (y + h - i); j++)
                {
                    threshold = (transparency * (bayerMatrixMax + 1)) >> 8;
                    if (threshold >= bayerMatrixY[j % bayerMatrixSize])
                    {
                        drawPixel(x + i - halfWidth, j - halfHeight, color);         // Línea izquierda
                        drawPixel(x + w - 1 - i - halfWidth, j - halfHeight, color); // Línea derecha
                    }
                }
            }
        }
    }
}

void drawRectCenterThickness(short x, short y, short w, short h, char color, short thickness)
{
    drawRectThickness(x - (w >> 1), y - (h >> 1), w, h, color, thickness);
}

void drawCircleHelper(short x0, short y0, short r, unsigned char cornername, char color)
{
    // Helper function for drawing circles and circular objects
    short f = 1 - r;
    short ddF_x = 1;
    short ddF_y = -2 * r;
    short x = 0;
    short y = r;

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        if (cornername & 0x4)
        {
            drawPixel(x0 + x, y0 + y, color);
            drawPixel(x0 + y, y0 + x, color);
        }
        if (cornername & 0x2)
        {
            drawPixel(x0 + x, y0 - y, color);
            drawPixel(x0 + y, y0 - x, color);
        }
        if (cornername & 0x8)
        {
            drawPixel(x0 - y, y0 + x, color);
            drawPixel(x0 - x, y0 + y, color);
        }
        if (cornername & 0x1)
        {
            drawPixel(x0 - y, y0 - x, color);
            drawPixel(x0 - x, y0 - y, color);
        }
    }
}

void drawCircle(short x0, short y0, short w, short h, char color, uint8_t thickness, int transparency)
{
    if (transparency <= 0)
        return;
    transparency = constrain(transparency, 0, 255);

    int threshold = (transparency * (bayerMatrixMax + 1)) >> 8;
    // Caso especial: si w o h es 1, dibujamos líneas
    if (w == 1 || h == 1)
    {
        if (w == 1)
        {
            // Dibuja línea vertical
            for (uint8_t t = 0; t < thickness; t++)
            {
                if (transparency == 255)
                {
                    drawVLine(x0 + t / 2, y0 - h / 2, h, color); // Línea derecha del grosor
                                                                 // drawVLine(x0 - t, y0 - h / 2, h, color);  // Línea izquierda del grosor
                }
                else
                {
                    for (short y = y0 - h / 2; y < y0 + h / 2; y++)
                    {
                        if (threshold >= bayerMatrixY[y % bayerMatrixSize])
                        {
                            drawPixel(x0 + t, y, color);
                            drawPixel(x0 - t, y, color);
                        }
                    }
                }
            }
        }
        else if (h == 1)
        {
            // Dibuja línea horizontal
            for (uint8_t t = 0; t < thickness; t++)
            {
                // Serial.println(t);
                if (threshold >= bayerMatrixY[(y0 + t) % bayerMatrixSize])
                {
                    drawHLine(x0 - w / 2, y0 + t / 2, w, color); // Línea superior del grosor
                                                                 // drawHLine(x0 - w / 2, y0 - t, w, color);  // Línea inferior del grosor
                }
            }
        }
        return;
    }

    // Si no es un caso especial, continúa con el código original de la elipse
    short rx = w / 2;
    short ry = h / 2;

    // Si es completamente opaco
    if (transparency == 255)
    {
        for (uint8_t t = 0; t < thickness; t++)
        {
            float a = rx - t;
            float b = ry - t;

            if (a < 1 || b < 1)
                continue; // Skip si el radio es muy pequeño

            long dx = 0;
            long dy = (long)round(b);
            long a2 = (long)(a * a);
            long b2 = (long)(b * b);
            long err = b2 - (2 * dy - 1) * a2;

            do
            {
                drawPixel(x0 + dx, y0 + dy, color); // Cuadrante 1
                drawPixel(x0 - dx, y0 + dy, color); // Cuadrante 2
                drawPixel(x0 - dx, y0 - dy, color); // Cuadrante 3
                drawPixel(x0 + dx, y0 - dy, color); // Cuadrante 4

                long e2 = 2 * err;
                if (e2 < (2 * dx + 1) * b2)
                {
                    dx++;
                    err += (2 * dx + 1) * b2;
                }
                if (e2 > -(2 * dy - 1) * a2)
                {
                    dy--;
                    err -= (2 * dy - 1) * a2;
                }
            } while (dy >= 0);

            // Completar la otra parte de la elipse
            while (dx < (long)round(a))
            {
                dx++;
                drawPixel(x0 + dx, y0, color);
                drawPixel(x0 - dx, y0, color);
            }
        }
        return;
    }

    // Versión con transparencia
    for (uint8_t t = 0; t < thickness; t++)
    {
        float a = rx - t;
        float b = ry - t;

        if (a < 1 || b < 1)
            continue; // Skip si el radio es muy pequeño

        long dx = 0;
        long dy = (long)round(b);
        long a2 = (long)(a * a);
        long b2 = (long)(b * b);
        long err = b2 - (2 * dy - 1) * a2;

        do
        {

            if (threshold >= bayerMatrixY[(y0 + dy) % bayerMatrixSize])
            {
                drawPixel(x0 + dx, y0 + dy, color); // Cuadrante 1
                drawPixel(x0 - dx, y0 + dy, color); // Cuadrante 2
            }
            if (threshold >= bayerMatrixY[(y0 - dy) % bayerMatrixSize])
            {
                drawPixel(x0 - dx, y0 - dy, color); // Cuadrante 3
                drawPixel(x0 + dx, y0 - dy, color); // Cuadrante 4
            }

            long e2 = 2 * err;
            if (e2 < (2 * dx + 1) * b2)
            {
                dx++;
                err += (2 * dx + 1) * b2;
            }
            if (e2 > -(2 * dy - 1) * a2)
            {
                dy--;
                err -= (2 * dy - 1) * a2;
            }
        } while (dy >= 0);

        // Completar la otra parte de la elipse
        while (dx < (long)round(a))
        {
            dx++;
            if (threshold >= bayerMatrixY[y0 % bayerMatrixSize])
            {
                drawPixel(x0 + dx, y0, color);
                drawPixel(x0 - dx, y0, color);
            }
        }
    }
}

void drawCircleRotated(short x0, short y0, short w, short h, char color, uint8_t thickness, int transparency, short angleDeg)
{
    if (transparency <= 0)
        return;
    transparency = constrain(transparency, 0, 255);

    // Si no hay rotación, usar la función original optimizada
    if (angleDeg == 0)
    {
        drawCircle(x0, y0, w, h, color, thickness, transparency);
        return;
    }

    // Normalizar ángulo
    short angleMod = angleDeg % 360;
    if (angleMod < 0)
        angleMod += 360;

    // Optimizaciones para ángulos específicos
    if (angleMod == 0 || angleMod == 180)
    {
        drawCircle(x0, y0, w, h, color, thickness, transparency);
        return;
    }
    else if (angleMod == 90 || angleMod == 270)
    {
        drawCircle(x0, y0, h, w, color, thickness, transparency); // Intercambiar w y h
        return;
    }

    // Obtener valores de las tablas trigonométricas
    short sinA = sinTable[angleMod];
    short cosA = cosTable[angleMod];

    int thresholdBase = (transparency * (bayerMatrixMax + 1)) >> 8;

    // Caso especial: si w o h es 1, dibujamos líneas rotadas
    if (w == 1 || h == 1)
    {
        if (w == 1)
        {
            // Dibuja línea vertical rotada
            for (uint8_t t = 0; t < thickness; t++)
            {
                for (short dy = -h / 2; dy < h / 2; dy++)
                {
                    short dx = t / 2;
                    // Rotar el punto
                    int rotated_x = ((dx * cosA - dy * sinA) >> 10) + x0;
                    int rotated_y = ((dx * sinA + dy * cosA) >> 10) + y0;

                    // Verificar transparencia basada en Y final
                    if (thresholdBase >= bayerMatrixY[rotated_y & (bayerMatrixSize - 1)])
                    {
                        drawPixel(rotated_x, rotated_y, color);

                        // Punto simétrico para el grosor
                        dx = -t / 2;
                        rotated_x = ((dx * cosA - dy * sinA) >> 10) + x0;
                        rotated_y = ((dx * sinA + dy * cosA) >> 10) + y0;
                        drawPixel(rotated_x, rotated_y, color);
                    }
                }
            }
        }
        else if (h == 1)
        {
            // Dibuja línea horizontal rotada
            for (uint8_t t = 0; t < thickness; t++)
            {
                for (short dx = -w / 2; dx < w / 2; dx++)
                {
                    short dy = t / 2;
                    // Rotar el punto
                    int rotated_x = ((dx * cosA - dy * sinA) >> 10) + x0;
                    int rotated_y = ((dx * sinA + dy * cosA) >> 10) + y0;

                    // Verificar transparencia basada en Y final
                    if (thresholdBase >= bayerMatrixY[rotated_y & (bayerMatrixSize - 1)])
                    {
                        drawPixel(rotated_x, rotated_y, color);

                        // Punto simétrico para el grosor
                        dy = -t / 2;
                        rotated_x = ((dx * cosA - dy * sinA) >> 10) + x0;
                        rotated_y = ((dx * sinA + dy * cosA) >> 10) + y0;
                        drawPixel(rotated_x, rotated_y, color);
                    }
                }
            }
        }
        return;
    }

    // Si no es un caso especial, continúa con el código de la elipse rotada
    short rx = w / 2;
    short ry = h / 2;

    // Si es completamente opaco, optimización sin transparencia
    if (transparency == 255)
    {
        for (uint8_t t = 0; t < thickness; t++)
        {
            float a = rx - t;
            float b = ry - t;

            if (a < 1 || b < 1)
                continue; // Skip si el radio es muy pequeño

            long dx = 0;
            long dy = (long)round(b);
            long a2 = (long)(a * a);
            long b2 = (long)(b * b);
            long err = b2 - (2 * dy - 1) * a2;

            do
            {
                // Aplicar rotación a cada punto de la elipse y dibujar
                // Cuadrante 1
                int rot_x1 = ((dx * cosA - dy * sinA) >> 10) + x0;
                int rot_y1 = ((dx * sinA + dy * cosA) >> 10) + y0;
                drawPixel(rot_x1, rot_y1, color);

                // Cuadrante 2
                int rot_x2 = ((-dx * cosA - dy * sinA) >> 10) + x0;
                int rot_y2 = ((-dx * sinA + dy * cosA) >> 10) + y0;
                drawPixel(rot_x2, rot_y2, color);

                // Cuadrante 3
                int rot_x3 = ((-dx * cosA - (-dy) * sinA) >> 10) + x0;
                int rot_y3 = ((-dx * sinA + (-dy) * cosA) >> 10) + y0;
                drawPixel(rot_x3, rot_y3, color);

                // Cuadrante 4
                int rot_x4 = ((dx * cosA - (-dy) * sinA) >> 10) + x0;
                int rot_y4 = ((dx * sinA + (-dy) * cosA) >> 10) + y0;
                drawPixel(rot_x4, rot_y4, color);

                long e2 = 2 * err;
                if (e2 < (2 * dx + 1) * b2)
                {
                    dx++;
                    err += (2 * dx + 1) * b2;
                }
                if (e2 > -(2 * dy - 1) * a2)
                {
                    dy--;
                    err -= (2 * dy - 1) * a2;
                }
            } while (dy >= 0);

            // Completar la otra parte de la elipse
            while (dx < (long)round(a))
            {
                dx++;

                // Puntos en el eje horizontal rotado
                int rot_x_pos = ((dx * cosA) >> 10) + x0;
                int rot_y_pos = ((dx * sinA) >> 10) + y0;
                drawPixel(rot_x_pos, rot_y_pos, color);

                int rot_x_neg = ((-dx * cosA) >> 10) + x0;
                int rot_y_neg = ((-dx * sinA) >> 10) + y0;
                drawPixel(rot_x_neg, rot_y_neg, color);
            }
        }
        return;
    }

    // Versión con transparencia optimizada
    for (uint8_t t = 0; t < thickness; t++)
    {
        float a = rx - t;
        float b = ry - t;

        if (a < 1 || b < 1)
            continue; // Skip si el radio es muy pequeño

        long dx = 0;
        long dy = (long)round(b);
        long a2 = (long)(a * a);
        long b2 = (long)(b * b);
        long err = b2 - (2 * dy - 1) * a2;

        do
        {
            // Aplicar rotación a cada punto de la elipse
            // Cuadrante 1
            int rot_x1 = ((dx * cosA - dy * sinA) >> 10) + x0;
            int rot_y1 = ((dx * sinA + dy * cosA) >> 10) + y0;

            // Cuadrante 2
            int rot_x2 = ((-dx * cosA - dy * sinA) >> 10) + x0;
            int rot_y2 = ((-dx * sinA + dy * cosA) >> 10) + y0;

            // Cuadrante 3
            int rot_x3 = ((-dx * cosA - (-dy) * sinA) >> 10) + x0;
            int rot_y3 = ((-dx * sinA + (-dy) * cosA) >> 10) + y0;

            // Cuadrante 4
            int rot_x4 = ((dx * cosA - (-dy) * sinA) >> 10) + x0;
            int rot_y4 = ((dx * sinA + (-dy) * cosA) >> 10) + y0;

            // Verificar transparencia por coordenada Y final y dibujar
            if (thresholdBase >= bayerMatrixY[rot_y1 & (bayerMatrixSize - 1)])
            {
                drawPixel(rot_x1, rot_y1, color);
            }
            if (thresholdBase >= bayerMatrixY[rot_y2 & (bayerMatrixSize - 1)])
            {
                drawPixel(rot_x2, rot_y2, color);
            }
            if (thresholdBase >= bayerMatrixY[rot_y3 & (bayerMatrixSize - 1)])
            {
                drawPixel(rot_x3, rot_y3, color);
            }
            if (thresholdBase >= bayerMatrixY[rot_y4 & (bayerMatrixSize - 1)])
            {
                drawPixel(rot_x4, rot_y4, color);
            }

            long e2 = 2 * err;
            if (e2 < (2 * dx + 1) * b2)
                {
                    dx++;
                    err += (2 * dx + 1) * b2;
                }
            if (e2 > -(2 * dy - 1) * a2)
            {
                dy--;
                err -= (2 * dy - 1) * a2;
            }
        } while (dy >= 0);

        // Completar la otra parte de la elipse
        while (dx < (long)round(a))
        {
            dx++;

            // Puntos en el eje horizontal rotado
            int rot_x_pos = ((dx * cosA) >> 10) + x0;
            int rot_y_pos = ((dx * sinA) >> 10) + y0;
            int rot_x_neg = ((-dx * cosA) >> 10) + x0;
            int rot_y_neg = ((-dx * sinA) >> 10) + y0;

            // Verificar transparencia y dibujar
            if (thresholdBase >= bayerMatrixY[rot_y_pos & (bayerMatrixSize - 1)])
            {
                drawPixel(rot_x_pos, rot_y_pos, color);
            }
            if (thresholdBase >= bayerMatrixY[rot_y_neg & (bayerMatrixSize - 1)])
            {
                drawPixel(rot_x_neg, rot_y_neg, color);
            }
        }
    }
}

void fillCircle(short x0, short y0, short r, char color)
{
    /* Draw a filled circle with center (x0,y0) and radius r, with given color
      Parameters:
           x0: x-coordinate of center of circle. The top-left of the screen
               has x-coordinate 0 and increases to the right
           y0: y-coordinate of center of circle. The top-left of the screen
               has y-coordinate 0 and increases to the bottom
           r:  radius of circle
           color: 16-bit color value for the circle
      Returns: Nothing
    */
    drawVLine(x0, y0 - r, 2 * r + 1, color);
    fillCircleHelper(x0, y0, r, 3, 0, color);
}

void filledElipsisTransparency(short x0, short y0, short w, short h, char color, int transparency)
{
    if (transparency == 0)
        return;

    int threshold = constrain(transparency, 0, 255);
    threshold = ((transparency * 64) >> 8); // Escalamos 0–255 a 0–64

    short a = w / 2;
    short b = h / 2;

    for (short yRel = -b; yRel <= b; yRel++)
    {
        short yScreen = y0 + yRel;

        // Dithering relativo al centro del elipse
        int ditherIndex = (yRel + b) & (bayerMatrixSize - 1);
        if (bayerMatrixY[ditherIndex] > threshold)
            continue;

        // Elipse: x = a * sqrt(1 - (yRel^2 / b^2))
        int y2 = yRel * yRel;
        int yNorm = (y2 * 255) / (b * b); // 0–255
        if (yNorm > 255)
            continue;

        int xNorm = sqrtLut[255 - yNorm]; // sqrt(1 - yNorm)
        int dx = (a * xNorm) / 255;

        drawHLine(x0 - dx, yScreen, 2 * dx + 1, color);
    }
}

void filledElipsisRotated(short x0, short y0, short w, short h, char color, int transparency, short angleDeg)
{

    if (transparency == 0)
        return;

    // Normalizar ángulo a 0-359
    angleDeg = ((angleDeg % 360) + 360) % 360;

    // Si no hay rotación, usar función optimizada
    if (angleDeg == 0)
    {
        filledElipsisTransparency(x0, y0, w, h, color, transparency);
        return;
    }

    int threshold = constrain(transparency, 0, 255);
    threshold = ((transparency * 64) >> 8);

    short a = w / 2; // Semi-eje horizontal
    short b = h / 2; // Semi-eje vertical

    // Obtener sin y cos del ángulo (escalados por 256)
    int cosA = cosLut[angleDeg];
    int sinA = sinLut[angleDeg];

    // Calcular bounding box rotado
    long cosA_abs = abs(cosA);
    long sinA_abs = abs(sinA);
    short boundW = ((a * cosA_abs + b * sinA_abs) >> 8) + 1;
    short boundH = ((a * sinA_abs + b * cosA_abs) >> 8) + 1;

    // Recorrer cada línea y del bounding box
    for (short y = -boundH; y <= boundH; y++)
    {
        short yScreen = y0 + y;

        // Verificar límites de pantalla
        if (yScreen < 0 || yScreen >= screenHeight)
            continue;

        // Aplicar dithering en Y
        int ditherIndex = (y + boundH) & (bayerMatrixSize - 1);
        if (bayerMatrixY[ditherIndex] > threshold)
            continue;

        // Encontrar segmentos horizontales dentro de la elipse
        bool inEllipse = false;
        short xStart = 0;

        for (short x = -boundW; x <= boundW; x++)
        {
            // Rotar punto (x,y) al sistema de coordenadas de la elipse original
            // Rotación inversa: usar -angleDeg
            long xRot = (x * cosA + y * sinA) >> 8;  // x * cos(-θ) - y * sin(-θ)
            long yRot = (-x * sinA + y * cosA) >> 8; // x * sin(-θ) + y * cos(-θ)

            // Verificar si está dentro de la elipse: (x/a)² + (y/b)² <= 1
            // Evitar división: (x*b)² + (y*a)² <= (a*b)²
            long ellipseTest = (xRot * xRot * b * b) + (yRot * yRot * a * a);
            long ellipseLimit = (long)a * a * b * b;

            bool pixelInside = (ellipseTest <= ellipseLimit);

            if (pixelInside && !inEllipse)
            {
                // Comenzar nuevo segmento
                xStart = x;
                inEllipse = true;
            }
            else if (!pixelInside && inEllipse)
            {
                // Terminar segmento actual
                short xScreenStart = x0 + xStart;
                short segmentWidth = x - xStart;

                // Verificar límites y dibujar
                if (xScreenStart < screenWidth && xScreenStart + segmentWidth > 0)
                {
                    if (xScreenStart < 0)
                    {
                        segmentWidth += xScreenStart;
                        xScreenStart = 0;
                    }
                    if (xScreenStart + segmentWidth > screenWidth)
                    {
                        segmentWidth = screenWidth - xScreenStart;
                    }
                    if (segmentWidth > 0)
                    {
                        drawHLine(xScreenStart, yScreen, segmentWidth, color);
                    }
                }
                inEllipse = false;
            }
        }

        // Si terminamos el loop y todavía estamos dentro, dibujar el último segmento
        if (inEllipse)
        {
            short xScreenStart = x0 + xStart;
            short segmentWidth = boundW + 1 - xStart;

            if (xScreenStart < screenWidth && xScreenStart + segmentWidth > 0)
            {
                if (xScreenStart < 0)
                {
                    segmentWidth += xScreenStart;
                    xScreenStart = 0;
                }
                if (xScreenStart + segmentWidth > screenWidth)
                {
                    segmentWidth = screenWidth - xScreenStart;
                }
                if (segmentWidth > 0)
                {
                    drawHLine(xScreenStart, yScreen, segmentWidth, color);
                }
            }
        }
    }
}

// Draw a rounded rectangle
void drawRoundRect(short x, short y, short w, short h, short r, char color)
{
    /* Draw a rounded rectangle outline with top left vertex (x,y), width w,
      height h and radius of curvature r at given color
      Parameters:
           x:  x-coordinate of top-left vertex. The x-coordinate of
               the top-left of the screen is 0. It increases to the right.
           y:  y-coordinate of top-left vertex. The y-coordinate of
               the top-left of the screen is 0. It increases to the bottom.
           w:  width of the rectangle
           h:  height of the rectangle
           color:  16-bit color of the rectangle outline
      Returns: Nothing
    */
    // smarter version
    drawHLine(x + r, y, w - 2 * r, color);         // Top
    drawHLine(x + r, y + h - 1, w - 2 * r, color); // Bottom
    drawVLine(x, y + r, h - 2 * r, color);         // Left
    drawVLine(x + w - 1, y + r, h - 2 * r, color); // Right
    // draw four corners
    drawCircleHelper(x + r, y + r, r, 1, color);
    drawCircleHelper(x + w - r - 1, y + r, r, 2, color);
    drawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
    drawCircleHelper(x + r, y + h - r - 1, r, 8, color);
}

// Fill a rounded rectangle
void fillRoundRect(short x, short y, short w, short h, short r, char color)
{
    // smarter version
    fillRect(x + r, y, w - 2 * r, h, color);

    // draw four corners
    fillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
    fillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, color);
}

void fillRectCenter(short x, short y, short w, short h, char color)
{
    fillRect(x - (w >> 1), y - (h >> 1), w, h, color);
}

void fillRectTransparency(short x, short y, short w, short h, char color, int transparency)
{
    if (w < 0 || h < 0)
        return;
    if (transparency <= 0)
    {
        return;
    }
    transparency = constrain(transparency, 0, 255);

    if (transparency == 255)
    {
        fillRectCenter(x, y, w, h, color);
    }
    // Iterate through the y of the rectangle

    for (int j = y; j < (y + h); j++)
    {
        int threshold = (transparency * (bayerMatrixMax + 1)) >> 8; // Map pixel intensity to Bayer matrix range
        // Serial.print(threshold);
        //   Serial.print("/");
        // Serial.println(bayerMatrixY[(screenHeight + j) % bayerMatrixSize]);
        // Compare transparency with the threshold for dithering
        if (threshold >= bayerMatrixY[(j + y) % bayerMatrixSize])
        { // si DoItFast == 1
            drawHLine(x - (w >> 1), j - (h >> 1), w, color);
        }
    }
}

void drawFillRectRotated(short x, short y, short w, short h, char color, int transparency, short angleDeg)
{
    if (w < 0 || h < 0)
        return;
    if (transparency <= 0)
    {
        return;
    }
    transparency = constrain(transparency, 0, 255);

    // Si no hay rotación, usar la función original optimizada
    if (angleDeg == 0)
    {
        fillRectTransparency(x, y, w, h, color, transparency);
        return;
    }

    // Para ángulos múltiplos de 90°, podemos optimizar
    int normalizedAngle = angleDeg % 360;
    if (normalizedAngle == 90 || normalizedAngle == 270)
    {
        // Rotar 90° o 270° intercambia w y h
        fillRectTransparency(x, y, h, w, color, transparency);
        return;
    }
    if (normalizedAngle == 180)
    {
        // Rotar 180° es igual que el original
        fillRectTransparency(x, y, w, h, color, transparency);
        return;
    }

    // Para otros ángulos, usar la versión pixel por pixel  if (w < 0 || h < 0) return;
    if (transparency <= 0)
    {
        return;
    }
    transparency = constrain(transparency, 0, 255);

    // Si no hay rotación, usar la función original optimizada
    if (angleDeg == 0)
    {
        fillRectTransparency(x, y, w, h, color, transparency);
        return;
    }

    // Hacer positivo el ángulo
    while (angleDeg < 0)
    {
        angleDeg = angleDeg + 360;
    }

    // Obtener valores de las tablas trigonométricas
    int angleTemp = angleDeg % 360;
    short cosAngleScaled = cosTable[angleTemp];
    short sinAngleScaled = sinTable[angleTemp];

    // Centro del rectángulo
    short center_x = x;
    short center_y = y;

    // Half dimensions para trabajar desde el centro
    short half_w = w >> 1;
    short half_h = h >> 1;

    int thresholdBase = (transparency * (bayerMatrixMax + 1)) >> 8;

    // Iterar por cada posición del rectángulo original (sin rotar)
    for (int dy = -half_h; dy < half_h; dy++)
    {
        // Calcular índice Y relativo para la matriz de transparencia (líneas horizontales)
        int relativeY = dy + half_h; // Convierte de rango [-half_h, half_h) a [0, h)

        // Verificar si esta línea horizontal debe dibujarse según el patrón de transparencia
        if (thresholdBase >= bayerMatrixY[relativeY & (bayerMatrixSize - 1)])
        {
            for (int dx = -half_w; dx < half_w; dx++)
            {
                // Aplicar rotación al punto (dx, dy) relativo al centro
                int rotated_x = ((dx * cosAngleScaled - dy * sinAngleScaled) >> 10) + center_x;
                int rotated_y = ((dx * sinAngleScaled + dy * cosAngleScaled) >> 10) + center_y;

                drawPixel(rotated_x, rotated_y, color);
            }
        }
    }
}

void drawRectThickness(short x, short y, short w, short h, char color, short thickness)
{
    if (w < 0 || h < 0)
        return;

    /* Draw a rectangle with specified thickness at the given position and size
      Parameters:
           x: x-coordinate of the top-left corner of the rectangle. The x-coordinate of
               the top-left of the screen is 0. It increases to the right.
           y: y-coordinate of the top-left corner of the rectangle. The y-coordinate of
               the top-left of the screen is 0. It increases to the bottom.
           w: width of the rectangle
           h: height of the rectangle
           color: 3-bit color value for the rectangle
           thickness: thickness of the rectangle's borders
    */
    if (thickness <= 1)
    {
        // If thickness is 1 or less, simply call drawRect function
        drawRect(x, y, w, h, color);
        return;
    }

    // Draw the top and bottom edges with specified thickness
    for (short i = 0; i < thickness; i++)
    {
        drawRect(x, y + i, w, 1, color);         // Top edge
        drawRect(x, y + h - i - 1, w, 1, color); // Bottom edge
    }

    // Draw the left and right edges with specified thickness
    for (short i = 0; i < thickness; i++)
    {
        drawRect(x + i, y, 1, h, color);         // Left edge
        drawRect(x + w - i - 1, y, 1, h, color); // Right edge
    }
}

void drawRectRotated(short x, short y, short w, short h, char color, uint8_t thickness, int transparency, short angleDeg)
{
    if (w <= 0 || h <= 0 || transparency <= 0)
        return;
    transparency = constrain(transparency, 0, 255);

    short angleMod = angleDeg % 360;
    if (angleMod < 0)
        angleMod += 360;
    if (angleMod == 0 || angleMod == 180)
    {
        drawRectTransparency(x, y, w, h, color, thickness, transparency);
        return;
    }
    else if (angleMod == 90 || angleMod == 270)
    {
        drawRectTransparency(x, y, h, w, color, thickness, transparency);
        return;
    }

    short sinA = sinTable[angleMod];
    short cosA = cosTable[angleMod];
    int thresholdBase = (transparency * (bayerMatrixMax + 1)) >> 8;

    for (uint8_t i = 0; i < thickness; i++)
    {
        short innerW = w - 2 * i;
        short innerH = h - 2 * i;
        short dx = -innerW / 2;
        short dy = -innerH / 2;

        for (short u = 0; u < innerW; u++)
        {
            short fx = dx + u;

            short fy = dy;
            short pyTemp = y + ((fx * sinA + fy * cosA) >> 10);
            short px = x + ((fx * cosA - fy * sinA) >> 10);
            if (thresholdBase >= bayerMatrixY[(pyTemp) & (bayerMatrixSize - 1)])
            {
                drawPixel(px, pyTemp, color);
            }

            fy = dy + innerH - 1;
            pyTemp = y + ((fx * sinA + fy * cosA) >> 10);
            px = x + ((fx * cosA - fy * sinA) >> 10);
            if (thresholdBase >= bayerMatrixY[(pyTemp) & (bayerMatrixSize - 1)])
            {
                drawPixel(px, pyTemp, color);
            }
        }

        for (short v = 0; v < innerH; v++)
        {
            short fy = dy + v;

            short fx = dx;
            short pyTemp = y + ((fx * sinA + fy * cosA) >> 10);
            short px = x + ((fx * cosA - fy * sinA) >> 10);
            if (thresholdBase >= bayerMatrixY[(y + v) & (bayerMatrixSize - 1)])
            {
                drawPixel(px, pyTemp, color);
            }

            fx = dx + innerW - 1;
            pyTemp = y + ((fx * sinA + fy * cosA) >> 10);
            px = x + ((fx * cosA - fy * sinA) >> 10);
            if (thresholdBase >= bayerMatrixY[(y + v) & (bayerMatrixSize - 1)])
            {
                drawPixel(px, pyTemp, color);
            }
        }
    }
}

// Draw a character
void drawChar(short x, short y, unsigned char c, char color, char bg, unsigned char size, short transparency)
{
    drawCharCustomSize(x, y, c, color, bg, size, size, transparency);
}

void drawCharCustomSize(short x, short y, unsigned char c, char color, char bg, short widthSize, short heightSize, short transparency)
{
    if (transparency <= 0)
        return;
    if (transparency > 255)
        transparency = 255;

    int threshold = (transparency * (bayerMatrixMax + 1)) >> 8;
    widthSize += 1;

    // ---- Camino para tamaños chicos (<8x8) ----
    if (widthSize < 8 || heightSize < 8)
    {
        short offsetX = widthSize / 2;
        short offsetY = heightSize / 2 + 1; // ajuste visual
        x -= offsetX;
        y -= offsetY;

        for (short outY = 0; outY < heightSize; outY++)
        {
            char fontY = (outY * 8) / heightSize;
            unsigned char line = font8x8_basic[c][fontY];
            short screenY = y + outY;
            int bayerValue = bayerMatrixY[screenY % bayerMatrixSize];

            for (short outX = 0; outX < widthSize; outX++)
            {
                char fontX = (outX * 8) / widthSize;
                bool isOn = (line & (1 << (7 - fontX))) != 0;

                if (isOn || color != bg)
                {
                    char drawColor = isOn ? color : bg;
                    if (threshold >= bayerValue)
                    {
                        drawPixel(x + outX, screenY, drawColor);
                    }
                }
            }
        }

        return;
    }

    // ---- Camino optimizado para tamaños grandes (>=8x8) ----
    short realWidth = widthSize / 10;
    realWidth += 1;
    short realHeight = (heightSize > 6) ? (heightSize + 2) / 8 : 1;

    short offsetX = (8 * realWidth) / 2;
    short offsetY = (8 * realHeight) / 2;
    x -= offsetX;
    y -= offsetY;

    for (char row = 0; row < 8; row++)
    {
        unsigned char line = font8x8_basic[c][row];

        for (char col = 0; col < 8; col++)
        {
            bool isOn = (line & (1 << (7 - col))) != 0;
            if (!isOn && color == bg)
                continue;

            char drawColor = isOn ? color : bg;
            short drawX = x + col * realWidth;
            short drawY = y + row * realHeight;

            for (int dy = 0; dy < realHeight; dy++)
            {
                short screenY = drawY + dy;
                int bayerValue = bayerMatrixY[screenY % bayerMatrixSize];

                if (threshold >= bayerValue)
                {
                    drawHLine(drawX, screenY, realWidth, drawColor);
                }
            }
        }
    }
}

void setTextCursor(short x, short y)
{
    /* Set cursor for text to be printed
      Parameters:
           x = x-coordinate of top-left of text starting
           y = y-coordinate of top-left of text starting
      Returns: Nothing
    */
    cursor_x = x;
    cursor_y = y;
}

void setTextSize(unsigned char s)
{
    /*Set size of text to be displayed
      Parameters:
           s = text size (1 being smallest)
      Returns: nothing
    */
    textsize = (s > 0) ? s : 1;
}

void setTextColor(char c)
{
    // For 'transparent' background, we'll set the bg
    // to the same as fg instead of using a flag
    textcolor = textbgcolor = c;
}

void setTextColor2(char c, char b)
{
    /* Set color of text to be displayed
      Parameters:
           c = 16-bit color of text
           b = 16-bit color of text background
    */
    textcolor = c;
    textbgcolor = b;
}

void setTextWrap(char w)
{
    wrap = w;
}

void tft_write(unsigned char c)
{

    if (c == '\n')
    {
        cursor_y += textsize * 8;
        cursor_x = 0;
    }
    else if (c == '\r')
    {
        // skip em
    }
    else if (c == '\t')
    {
        int new_x = cursor_x + tabspace;
        if (new_x < screenWidth)
        {
            cursor_x = new_x;
        }
    }
    else
    {
        drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize, 255);
        cursor_x += textsize * 8;
        if (wrap && (cursor_x > (screenWidth - textsize * 8)))
        {
            cursor_y += textsize * 8;
            cursor_x = 0;
        }
    }
}

void writeString(char *str)
{
    /* Print text onto screen
      Call tft_setTextCursor(), tft_setTextColor(), tft_setTextSize()
       as necessary before printing
    */
    while (*str)
    {
        tft_write(*str++);
    }
}

// Función para dibujar un image en la pantalla con una escala determinada
void drawImage(int xPosition, int yPosition,
               int targetWidth, int targetHeight,
               unsigned char *bitmapData,
               int bitmapWidth, int bitmapHeight,
               int color, int bgColor,
               bool doItFast, int transparency)
{
    if (transparency <= 0)
        return;
    if (transparency > 255)
        transparency = 255;
    int threshold = (transparency * (bayerMatrixMax + 1)) >> 8;

    float invScaleX, invScaleY, scaleXf, scaleYf;
    bool isDownscale = (targetWidth <= bitmapWidth * 2 && targetHeight <= bitmapHeight * 2);
    if (isDownscale)
    {
        invScaleX = (float)bitmapWidth / (float)targetWidth;
        invScaleY = (float)bitmapHeight / (float)targetHeight;
    }
    else
    {
        scaleXf = (float)targetWidth / (float)bitmapWidth;
        scaleYf = (float)targetHeight / (float)bitmapHeight;
    }

    int xStart = xPosition - (targetWidth >> 1);
    int yStart = yPosition - (targetHeight >> 1);

    const int lowerEdgeY = -10, higherEdgeY = screenHeight;
    const int lowerEdgeX = -10, higherEdgeX = screenWidth;

    // --------- MODO DOWNSCALE ---------
    if (isDownscale)
    {
        for (int j = 0; j < targetHeight; j++)
        {
            int drawY = yStart + j;
            if (drawY > higherEdgeY)
                break;
            if (drawY < lowerEdgeY)
                continue;

            for (int i = 0; i < targetWidth; i++)
            {
                int drawX = xStart + i;
                if (drawX < lowerEdgeX)
                    continue;
                if (drawX > higherEdgeX)
                    break;

                int srcX = (int)(i * invScaleX);
                int srcY = (int)(j * invScaleY);
                if (srcX >= bitmapWidth || srcY >= bitmapHeight)
                    continue;

                uint32_t arrayPos = (srcY * ((bitmapWidth + 7) & ~7) + srcX) >> 3;
                uint8_t bitPos = ((srcY * ((bitmapWidth + 7) & ~7) + srcX) & 0x07);
                bool pixelBit = ((bitmapData[arrayPos] >> bitPos) & 0x01) != 0;

                if (!pixelBit && bgColor == color)
                    continue;

                uint8_t finalColor = 0;
                if (pixelBit)
                {
                    if (doItFast)
                    {
                        int bValY = bayerMatrixY[(drawY + yPosition) % bayerMatrixSize];
                        if (threshold < bValY)
                            continue;
                        finalColor = color;
                    }
                    else
                    {
                        int bayerX = (drawX + xPosition) % bayerMatrixSize;
                        int bayerY = (drawY + yPosition) % bayerMatrixSize;
                        int bVal = bayerMatrix[bayerY][bayerX];
                        if (threshold < bVal)
                            continue;
                        finalColor = color;
                    }
                }
                else
                {
                    finalColor = bgColor;
                }

                if (doItFast)
                {
                    drawHLine(drawX, drawY, 1, finalColor);
                }
                else
                {
                    drawPixel(drawX, drawY, finalColor);
                }
            }
        }

        // --------- MODO UPSCALE ---------
    }
    else
    {
        for (int j = 0; j < bitmapHeight; j++)
        {
            int baseDestY = yStart + (int)(j * scaleYf);
            int drawH = (int)(scaleYf + 0.999f);
            if (baseDestY > higherEdgeY)
                break;
            if (baseDestY + drawH < lowerEdgeY)
                continue;

            for (int i = 0; i < bitmapWidth; i++)
            {
                int baseDestX = xStart + (int)(i * scaleXf);
                int drawW = (int)(scaleXf + 0.999f);
                if (baseDestX > higherEdgeX)
                    break;
                if (baseDestX + drawW < lowerEdgeX)
                    continue;

                uint32_t arrayPos = (j * ((bitmapWidth + 7) & ~7) + i) >> 3;
                uint8_t bitPos = ((j * ((bitmapWidth + 7) & ~7) + i) & 0x07);
                bool pixelBit = ((bitmapData[arrayPos] >> bitPos) & 0x01) != 0;

                if (!pixelBit && bgColor == color)
                    continue;

                if (pixelBit)
                {
                    if (doItFast)
                    {
                        // Dithering Y por cada línea del bloque
                        for (int sy = 0; sy < drawH; sy++)
                        {
                            int drawY = baseDestY + sy;
                            if (drawY < lowerEdgeY || drawY > higherEdgeY)
                                continue;
                            int bValY = bayerMatrixY[(drawY + yPosition) % bayerMatrixSize];
                            if (threshold >= bValY)
                            {
                                drawHLine(baseDestX, drawY, drawW, color);
                            }
                        }
                    }
                    else
                    {
                        int bayerX = (baseDestX + xPosition) % bayerMatrixSize;
                        int bayerY = (baseDestY + yPosition) % bayerMatrixSize;
                        int bVal = bayerMatrix[bayerY][bayerX];
                        if (threshold >= bVal)
                        {
                            for (int sy = 0; sy < drawH; sy++)
                            {
                                int drawY = baseDestY + sy;
                                if (drawY < lowerEdgeY || drawY > higherEdgeY)
                                    continue;
                                drawHLine(baseDestX, drawY, drawW, color);
                            }
                        }
                    }
                }
                else
                {
                    // fondo se dibuja sin dithering
                    for (int sy = 0; sy < drawH; sy++)
                    {
                        int drawY = baseDestY + sy;
                        if (drawY < lowerEdgeY || drawY > higherEdgeY)
                            continue;
                        drawHLine(baseDestX, drawY, drawW, bgColor);
                    }
                }
            }
        }
    }
}

void drawStar(short x0, short y0, short w, short h, char color, uint8_t thickness, int transparency)
{
    if (transparency <= 0)
        return;
    transparency = constrain(transparency, 0, 255);

    // Calculamos los radios
    short rx = w / 2;
    short ry = h / 2;
    // Radio interior más pequeño para los picos
    short rx_inner = rx / 2;
    short ry_inner = ry / 2;

    // Puntos fijos de la estrella (4 puntas principales)
    const short num_points = 4;
    // Coordenadas relativas pre-calculadas para evitar senos y cosenos
    const short points_x[] = {1, 0, -1, 0}; // Multiplicar por rx
    const short points_y[] = {0, 1, 0, -1}; // Multiplicar por ry

    // Si es completamente opaco
    if (transparency == 255)
    {
        for (uint8_t t = 0; t < thickness; t++)
        {
            short outer_x = rx - t;
            short outer_y = ry - t;
            short inner_x = rx_inner - t;
            short inner_y = ry_inner - t;

            if (outer_x < 1 || outer_y < 1)
                continue;

            // Dibujamos las líneas conectando los puntos
            for (int i = 0; i < num_points; i++)
            {
                // Punto exterior actual
                short x1 = x0 + points_x[i] * outer_x;
                short y1 = y0 + points_y[i] * outer_y;

                // Punto interior siguiente (rotado 45 grados)
                int next = (i + 1) % num_points;
                short x2 = x0 + (points_x[i] + points_x[next]) * inner_x / 2;
                short y2 = y0 + (points_y[i] + points_y[next]) * inner_y / 2;

                // Dibujamos línea del punto exterior al interior
                drawLine(x1, y1, x2, y2, color);

                // Punto exterior siguiente
                short x3 = x0 + points_x[next] * outer_x;
                short y3 = y0 + points_y[next] * outer_y;

                // Dibujamos línea del punto interior al siguiente exterior
                drawLine(x2, y2, x3, y3, color);
            }
        }
        return;
    }

    // Versión con transparencia
    for (uint8_t t = 0; t < thickness; t++)
    {
        short outer_x = rx - t;
        short outer_y = ry - t;
        short inner_x = rx_inner - t;
        short inner_y = ry_inner - t;

        if (outer_x < 1 || outer_y < 1)
            continue;

        for (int i = 0; i < num_points; i++)
        {
            short x1 = x0 + points_x[i] * outer_x;
            short y1 = y0 + points_y[i] * outer_y;

            int next = (i + 1) % num_points;
            short x2 = x0 + (points_x[i] + points_x[next]) * inner_x / 2;
            short y2 = y0 + (points_y[i] + points_y[next]) * inner_y / 2;

            // Aplicamos transparencia a las líneas
            int threshold = (transparency * (bayerMatrixMax + 1)) >> 8;
            if (threshold >= bayerMatrixY[y1 % bayerMatrixSize])
            {
                drawLine(x1, y1, x2, y2, color);
            }

            short x3 = x0 + points_x[next] * outer_x;
            short y3 = y0 + points_y[next] * outer_y;

            if (threshold >= bayerMatrixY[y2 % bayerMatrixSize])
            {
                drawLine(x2, y2, x3, y3, color);
            }
        }
    }
}

void drawPixelWithTransparency(short px, short py, short color, short transparency)
{
    int threshold = (transparency * (bayerMatrixMax + 1)) >> 8;
    if (threshold >= bayerMatrixY[py % bayerMatrixSize])
    {
        drawPixel(px, py, color);
    }
}

void drawPussy(short x0, short y0, short w, short h, char color, uint8_t thickness, int transparency)
{
    if (transparency <= 0)
        return;
    transparency = constrain(transparency, 0, 255);

    short x;
    short y;
    short dx;
    short dy;
    short err;
    // Calculamos los radios para la elipse
    short rx = w / 2;
    short ry = h / 2;

    for (uint8_t t = 0; t < thickness; t++)
    {
        // Algoritmo de bresenham modificado para elipses
        long dx, dy, err;
        long a = (rx - t) * (rx - t);
        long b = (ry - t) * (ry - t);

        if (rx < 1 || ry < 1)
            return;

        x = 0;
        y = ry - t;
        dx = 0;
        dy = 2 * a * y;
        err = 0;

        // Dibujar con transparencia

        while (y >= 0)
        {
            // Dibujar los 4 cuadrantes
            if (transparency == 255)
            {
                drawPixel(x0 + x, y0 + y, color);
                drawPixel(x0 - x, y0 + y, color);
                drawPixel(x0 + x, y0 - y, color);
                drawPixel(x0 - x, y0 - y, color);
            }
            else
            {
                drawPixelWithTransparency(x0 + x, y0 + y, color,transparency);
                drawPixelWithTransparency(x0 - x, y0 + y, color,transparency);
                drawPixelWithTransparency(x0 + x, y0 - y, color,transparency);
                drawPixelWithTransparency(x0 - x, y0 - y, color,transparency);
            }

            x++;
            dx += 2 * b;
            err += dx;
            if (2 * err + dy > 0)
            {
                y--;
                dy -= 2 * a;
                err += dy;
            }
        }

        // Segunda parte del algoritmo
        x = rx - t;
        y = 0;
        dx = -2 * a * x;
        dy = 0;
        err = 0;

        while (x >= 0)
        {
            // Dibujar los 4 cuadrantes
            if (transparency == 255)
            {
                drawPixel(x0 + x, y0 + y, color);
                drawPixel(x0 - x, y0 + y, color);
                drawPixel(x0 + x, y0 - y, color);
                drawPixel(x0 - x, y0 - y, color);
            }
            else
            {
                drawPixelWithTransparency(x0 + x, y0 + y,color, transparency);
                drawPixelWithTransparency(x0 - x, y0 + y,color, transparency);
                drawPixelWithTransparency(x0 + x, y0 - y,color, transparency);
                drawPixelWithTransparency(x0 - x, y0 - y,color, transparency);
            }

            y++;
            dy += 2 * b;
            err += dy;
            if (2 * err + dx > 0)
            {
                x--;
                dx += 2 * a;
                err += dx;
            }
        }
    }
}