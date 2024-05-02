#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdbool.h>
#include <vita2d.h>

static vita2d_font *defaultFont = NULL;

#define R(color) ((u8)(color & 0xFF))
#define G(color) ((u8)(color >> 8 & 0xFF))
#define B(color) ((u8)(color >> 16 & 0xFF))
#define A(color) ((u8)(color >> 24 & 0xFF))

vita2d_texture *load_png(const char *path);
vita2d_texture *load_bmp(const char *path);
vita2d_texture *load_jpeg(const char *path);

void clearScreen();
void clearScreenWithColor(uint32_t color);

void set_vblank_wait(bool enable);

SceGxmContext *get_GxmContext();

void loadFont(const char *fontPath);
void freeFont();
int printfFontText(float x, float y, float scale, uint32_t color, const char *text);

int getFreeVRAM();

void start_drawing();
void start_drawing_and_clear();
void end_drawing();

int common_dialog_update();

void drawTexture(vita2d_texture *texture, float x, float y);

void drawPixel(float x, float y, uint32_t color);
void drawLine(float x, float y, float x2, float y2, uint32_t color);
void drawRect(float x, float y, int width, int height, uint32_t color);
void drawRectCentered(float x, float y, int width, int height, uint32_t color);
void drawTriangle(float x, float y, float x2, float y2, float x3, float y3, uint32_t color);
void drawCircle(float x, float y, float radius, uint32_t color, bool filled);

float *getRefreshRate();

void init_graphics();
void end_graphics();
void swap_buffers();
void waitVblankStart();

void athena_error_screen(const char *errMsg);

#endif
