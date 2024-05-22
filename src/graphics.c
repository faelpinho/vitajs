#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>
#include <jpeglib.h>
#include <time.h>
#include <png.h>
#include <psp2common/ctrl.h>
#include <vita2d.h>

#include "graphics.h"
#include "env.h"

// 2D drawing functions
void init_graphics()
{
	vita2d_init();
	vita2d_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));
}

void end_graphics()
{
	vita2d_fini();
}

void start_drawing()
{
	vita2d_start_drawing();
}

void start_drawing_and_clear()
{
	vita2d_start_drawing();
	vita2d_clear_screen();
}

void end_drawing()
{
	vita2d_end_drawing();
	vita2d_swap_buffers();
}

void swap_buffers()
{
	vita2d_swap_buffers();
}

void waitVblankStart()
{
	sceDisplayWaitVblankStart();
}

void clearScreen()
{
	vita2d_clear_screen();
}

void clearScreenWithColor(uint32_t color)
{
	vita2d_set_clear_color(color);
	vita2d_clear_screen();
}

void set_vblank_wait(bool enable)
{
	vita2d_set_vblank_wait(enable);
}

SceGxmContext *get_GxmContext()
{
	return vita2d_get_context();
}

vita2d_texture *load_png(const char *path)
{
	return vita2d_load_PNG_file(path);
}

vita2d_texture *load_bmp(const char *path)
{
	return vita2d_load_BMP_file(*path);
}

vita2d_texture *load_jpeg(const char *path)
{
	return vita2d_load_JPEG_file(path);
}

void loadFont(const char *fontPath)
{
	defaultFont = vita2d_load_font_file(fontPath);
}

void freeFont()
{
	vita2d_free_font(defaultFont);
}

int printfFontText(float x, float y, float scale, uint32_t color, const char *text)
{
	return vita2d_font_draw_textf(defaultFont, x, y, color, scale, text);
}

int getFreeVRAM()
{
	// TODO
	return 0;
}

int common_dialog_update()
{
	return vita2d_common_dialog_update();
}

void drawTexture(vita2d_texture *texture, float x, float y)
{
	vita2d_draw_texture(texture, x, y);
}

void drawPixel(float x, float y, uint32_t color)
{
	vita2d_draw_pixel(x, y, color);
}

void drawLine(float x, float y, float x2, float y2, uint32_t color)
{
	vita2d_draw_line(x, y, x2, y2, color);
}

void drawRect(float x, float y, int width, int height, uint32_t color)
{
	// TODO: TEST IT
	// vita2d_draw_rectangle(x, y, width, height, color); // default rect
	vita2d_draw_rectangle(x - 0.5f, y - 0.5f, (x + width) - 0.5f, (y + height) - 0.5f, color);
}

void drawRectCentered(float x, float y, int width, int height, uint32_t color)
{
	vita2d_draw_rectangle(x - width / 2, y - height / 2, (x + width) - width / 2, (y + height) - height / 2, color);
}

void drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t color)
{
	vita2d_draw_line(x1, y1, x2, y2, color);
	vita2d_draw_line(x2, y2, x3, y3, color);
	vita2d_draw_line(x3, y3, x1, y1, color);

	/*
	TODO: Test it
	// Calcule os comprimentos dos lados do triângulo
	float len1 = sqrtf((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
	float len2 = sqrtf((x3 - x2) * (x3 - x2) + (y3 - y2) * (y3 - y2));
	float len3 = sqrtf((x1 - x3) * (x1 - x3) + (y1 - y3) * (y1 - y3));

	// Calcule os ângulos entre os lados do triângulo
	float angle1 = acosf((len2 * len2 + len3 * len3 - len1 * len1) / (2 * len2 * len3));
	float angle2 = acosf((len3 * len3 + len1 * len1 - len2 * len2) / (2 * len3 * len1));
	float angle3 = acosf((len1 * len1 + len2 * len2 - len3 * len3) / (2 * len1 * len2));

	// Desenhe os lados do triângulo com base nos comprimentos e ângulos calculados
	vita2d_draw_texture_rotate_scale(lineTexture, x1, y1, 0, len1, 1.0f);
	vita2d_draw_texture_rotate_scale(lineTexture, x2, y2, angle1, len2, 1.0f);
	vita2d_draw_texture_rotate_scale(lineTexture, x3, y3, angle2 + angle1, len3, 1.0f);
	*/
}

void drawCircle(float x, float y, float radius, uint32_t color, bool filled)
{
	vita2d_draw_fill_circle(x, y, radius, color);
}

float *getRefreshRate()
{
	float *fps;
	sceDisplayGetRefreshRate(fps);
	return fps;
}

void vitajs_error_screen(const char *errMsg)
{
	uint32_t color = RGBA8(0x20, 0x20, 0x20, 0xFF);

	if (errMsg != NULL)
	{
		printf("VitaJS ERROR!\n%s", errMsg);

		if (strstr(errMsg, "EvalError") != NULL)
		{
			color = RGBA8(0x56, 0x71, 0x7D, 0xFF);
		}
		else if (strstr(errMsg, "SyntaxError") != NULL)
		{
			color = RGBA8(0x20, 0x60, 0xB0, 0xFF);
		}
		else if (strstr(errMsg, "TypeError") != NULL)
		{
			color = RGBA8(0x3b, 0x81, 0x32, 0xFF);
		}
		else if (strstr(errMsg, "ReferenceError") != NULL)
		{
			color = RGBA8(0xE5, 0xDE, 0x00, 0xFF);
		}
		else if (strstr(errMsg, "RangeError") != NULL)
		{
			color = RGBA8(0xD0, 0x31, 0x3D, 0xFF);
		}
		else if (strstr(errMsg, "InternalError") != NULL)
		{
			color = RGBA8(0x8A, 0x00, 0xC2, 0xFF);
		}
		else if (strstr(errMsg, "URIError") != NULL)
		{
			color = RGBA8(0xFF, 0x78, 0x1F, 0xFF);
		}
		else if (strstr(errMsg, "AggregateError") != NULL)
		{
			color = RGBA8(0xE2, 0x61, 0x9F, 0xFF);
		}

		while (ctrl.buttons != SCE_CTRL_START)
		{
			start_drawing();
			clearScreen();

			printfFontText(20, 50, 25, color, "VitaJS ERROR!");
			printfFontText(20, 100, 16, color, errMsg);
			printfFontText(20, 350, 16, color, "\nPress [start] to close the app.\n");
			printf("erro\n");

			end_drawing();
			swap_buffers();
		}
	}
}
