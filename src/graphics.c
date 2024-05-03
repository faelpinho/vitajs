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

#define PI 3.14159265359

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

/*
TODO: PARA IMPLEMENTAR

void vita2d_set_region_clip(SceGxmRegionClipMode mode, unsigned int x_min, unsigned int y_min, unsigned int x_max, unsigned int y_max);
void vita2d_enable_clipping();
void vita2d_disable_clipping();
int vita2d_get_clipping_enabled();
void vita2d_set_clip_rectangle(int x_min, int y_min, int x_max, int y_max);
void vita2d_get_clip_rectangle(int *x_min, int *y_min, int *x_max, int *y_max);
void vita2d_set_blend_mode_add(int enable);

void *vita2d_pool_malloc(unsigned int size);
void *vita2d_pool_memalign(unsigned int size, unsigned int alignment);
unsigned int vita2d_pool_free_space();
void vita2d_pool_reset();

void vita2d_draw_array(SceGxmPrimitiveType mode, const vita2d_color_vertex *vertices, size_t count);

void vita2d_texture_set_alloc_memblock_type(SceKernelMemBlockType type);
SceKernelMemBlockType vita2d_texture_get_alloc_memblock_type();
vita2d_texture *vita2d_create_empty_texture(unsigned int w, unsigned int h);
vita2d_texture *vita2d_create_empty_texture_format(unsigned int w, unsigned int h, SceGxmTextureFormat format);
vita2d_texture *vita2d_create_empty_texture_rendertarget(unsigned int w, unsigned int h, SceGxmTextureFormat format);

void vita2d_free_texture(vita2d_texture *texture);

unsigned int vita2d_texture_get_width(const vita2d_texture *texture);
unsigned int vita2d_texture_get_height(const vita2d_texture *texture);
unsigned int vita2d_texture_get_stride(const vita2d_texture *texture);
SceGxmTextureFormat vita2d_texture_get_format(const vita2d_texture *texture);
void *vita2d_texture_get_datap(const vita2d_texture *texture);
void *vita2d_texture_get_palette(const vita2d_texture *texture);
SceGxmTextureFilter vita2d_texture_get_min_filter(const vita2d_texture *texture);
SceGxmTextureFilter vita2d_texture_get_mag_filter(const vita2d_texture *texture);
void vita2d_texture_set_filters(vita2d_texture *texture, SceGxmTextureFilter min_filter, SceGxmTextureFilter mag_filter);

void vita2d_draw_texture_rotate(const vita2d_texture *texture, float x, float y, float rad);
void vita2d_draw_texture_rotate_hotspot(const vita2d_texture *texture, float x, float y, float rad, float center_x, float center_y);
void vita2d_draw_texture_scale(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale);
void vita2d_draw_texture_part(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h);
void vita2d_draw_texture_part_scale(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale);
void vita2d_draw_texture_scale_rotate_hotspot(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale, float rad, float center_x, float center_y);
void vita2d_draw_texture_scale_rotate(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale, float rad);
void vita2d_draw_texture_part_scale_rotate(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale, float rad);

void vita2d_draw_texture_tint(const vita2d_texture *texture, float x, float y, unsigned int color);
void vita2d_draw_texture_tint_rotate(const vita2d_texture *texture, float x, float y, float rad, unsigned int color);

vita2d_texture *vita2d_load_PNG_file(const char *filename);
vita2d_texture *vita2d_load_PNG_buffer(const void *buffer);

vita2d_texture *vita2d_load_JPEG_file(const char *filename);
vita2d_texture *vita2d_load_JPEG_buffer(const void *buffer, unsigned long buffer_size);

vita2d_texture *vita2d_load_BMP_file(const char *filename);
vita2d_texture *vita2d_load_BMP_buffer(const void *buffer);

// PGF functions are weak imports at the moment, they have to be resolved manually.
vita2d_pgf *vita2d_load_system_pgf(int numFonts, const vita2d_system_pgf_config *configs);
vita2d_pgf *vita2d_load_default_pgf();
vita2d_pgf *vita2d_load_custom_pgf(const char *path);
void vita2d_free_pgf(vita2d_pgf *font);
int vita2d_pgf_draw_text(vita2d_pgf *font, int x, int y, unsigned int color, float scale, const char *text);
int vita2d_pgf_draw_text_ls(vita2d_pgf *font, int x, int y, float linespace, unsigned int color, float scale, const char *text);
int vita2d_pgf_draw_textf(vita2d_pgf *font, int x, int y, unsigned int color, float scale, const char *text, ...);
int vita2d_pgf_draw_textf_ls(vita2d_pgf *font, int x, int y, float linespace, unsigned int color, float scale, const char *text, ...);
void vita2d_pgf_text_dimensions(vita2d_pgf *font, float scale, const char *text, int *width, int *height);
int vita2d_pgf_text_width(vita2d_pgf *font, float scale, const char *text);
int vita2d_pgf_text_height(vita2d_pgf *font, float scale, const char *text);

vita2d_pvf *vita2d_load_system_pvf(int numFonts, const vita2d_system_pvf_config *configs);
vita2d_pvf *vita2d_load_default_pvf();
vita2d_pvf *vita2d_load_custom_pvf(const char *path);
void vita2d_free_pvf(vita2d_pvf *font);
int vita2d_pvf_draw_text(vita2d_pvf *font, int x, int y, unsigned int color, float scale, const char *text);
int vita2d_pvf_draw_textf(vita2d_pvf *font, int x, int y, unsigned int color, float scale, const char *text, ...);
int vita2d_pvf_draw_text_ls(vita2d_pvf *font, int x, int y, float linespace, unsigned int color, float scale, const char *text);
int vita2d_pvf_draw_textf_ls(vita2d_pvf *font, int x, int y, float linespace, unsigned int color, float scale, const char *text, ...);
void vita2d_pvf_text_dimensions(vita2d_pvf *font, float scale, const char *text, int *width, int *height);
int vita2d_pvf_text_width(vita2d_pvf *font, float scale, const char *text);
int vita2d_pvf_text_height(vita2d_pvf *font, float scale, const char *text);

*/

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
	uint32_t color2 = RGBA8(0xFF, 0xFF, 0xFF, 0xFF);

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

		color2 = color;
		color = RGBA8(0x20, 0x20, 0x20, 0xFF);

		clearScreen();
		printfFontText(20, 50, 25, color2, "VitaJS ERROR!");
		printfFontText(20, 100, 16, color2, errMsg);
		printfFontText(20, 350, 16, color2, "\nPress [start] to close the app.\n");
	}
}
