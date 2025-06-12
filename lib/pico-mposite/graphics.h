//
// Title:	        Pico-mposite Graphics Primitives
// Author:	        Dean Belfield
// Created:	        01/02/2022
// Last Updated:	02/03/2022
//
// Modinfo:
// 07/02/2022:      Added support for filled primitives
// 20/02/2022:      Added scroll_up, bitmap now initialised in cvideo.c
// 02/03/2022:      Added blit

#pragma once


#include <stdint.h>
#include <stdbool.h>

#define rgb(r,g,b) (((b&6)<<5)|(g<<3)|r)

extern const short sinLut[360];
extern const short cosLut[360];
extern const short sinTable[360];
extern const short cosTable[360];
extern const uint8_t sqrtLut[256];
extern const int bayerMatrixMax;
extern const int bayerMatrixSize;

#define bayerMatrixSize 8
extern const int bayerMatrix[bayerMatrixSize][bayerMatrixSize];
extern const int bayerMatrixY[bayerMatrixSize];


#ifdef __cplusplus
extern "C" {
#endif

void clearScreen(unsigned char c);

void print_char(int x, int y, int c, unsigned char bc, unsigned char fc);
void print_string(int x, int y, char *s, unsigned char bc, unsigned char fc);

void drawPixel(short x, short y, unsigned char c);
void drawVLine(short x, short y, short h, unsigned char color);
void drawHLine(short x, short y, short w, unsigned char color);
//void drawHLineFast(short startX, short y, short w, uint8_t color);
void drawLine(short x0, short y0, short x1, short y1, char color);
void drawLineThickness(short x0, short y0, short x1, short y1, char unsigned color, short thickness);
void drawRect(short x, short y, short w, short h, char color);
void drawRectThickness(short x, short y, short w, short h, char color, short thickness);
void drawRectCenter(short x, short y, short w, short h, char color);
void drawRectCenterThickness(short x, short y, short w, short h, char color, short thickness);
void drawRectRotated(short x, short y, short w, short h, char color, uint8_t thickness, int transparency, short angleDeg);
void drawRectTransparency(short x, short y, short w, short h, char color, uint8_t thickness, int transparency);
void drawCircleHelper(short x0, short y0, short r, unsigned char cornername, char color);
void drawCircle(short x0, short y0, short w, short h, char color, uint8_t thickness, int transparency);
void drawCircleRotated(short x0, short y0, short w, short h, char color, uint8_t thickness, int transparency, short angleDeg);
void fillCircle(short x0, short y0, short r, char color);
void filledElipsisTransparency(short x0, short y0, short w, short h, char color, int transparency);
void filledElipsisRotated(short x0, short y0, short w, short h, char color, int transparency, short angleDeg);
void fillCircleHelper(short x0, short y0, short r, unsigned char cornername, short delta, char color);
void drawRoundRect(short x, short y, short w, short h, short r, char color);
void fillRoundRect(short x, short y, short w, short h, short r, char color);
void fillRect(short x, short y, short w, short h, char color);
void fillRectCenter(short x, short y, short w, short h, char color);
void fillRectTransparency(short x, short y, short w, short h, char color, int transparency);
void drawFillRectRotated(short x, short y, short w, short h, char color, int transparency, short angleDeg);
void fillRectFast(short x, short y, short w, short h, char color);
void drawChar(short x, short y, unsigned char c, char color, char bg, unsigned char size, short transparency);
void drawCharCustomSize(short x, short y, unsigned char c, char color, char bg, short widthSize, short heightSize, short transparency);
void setTextCursor(short x, short y);
void setTextColor(char c);
void setTextColor2(char c, char bg);
void setTextSize(unsigned char s);
void setTextWrap(char w);
void tft_write(unsigned char c);
void writeString(char* str);
void drawImage(int xPosition, int yPosition, int targetWidth, int targetHeight, unsigned char* bitmapData, int bitmapWidth, int bitmapHeight, int color, int bgColor, bool doItFast, int transparency);
void drawStar(short x0, short y0, short w, short h, char color, uint8_t thickness, int transparency);
void drawPussy(short x0, short y0, short w, short h, char color, uint8_t thickness, int transparency);

// Función para autómata de Conway
unsigned char getPixel(short x, short y);

void swapNumb(short *a, short *b);
#ifdef __cplusplus
}
#endif

