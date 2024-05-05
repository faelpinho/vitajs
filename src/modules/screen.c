#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "../graphics.h"
#include "../env.h"

#define PI 3.14159265359

#define MAX_TEXTURES 255

static JSClassID js_texture_class_id;

typedef struct TextureData
{
	int id;
	vita2d_texture *texture;
} TextureData;

static TextureData textures[MAX_TEXTURES];

SceGxmContext *gmxContext = NULL;

bool is_invalid_texture(TextureData *texture)
{
	return (texture->id == -1 && !texture->texture);
}

static int new_texture(const vita2d_texture *texture)
{
	int id = -1;

	for (int i = 0; i < MAX_TEXTURES; i++)
	{
		if (is_invalid_texture(&textures[i]))
		{
			textures[i].id = i;
			textures[i].texture = texture;
			id = i;
			break;
		}
	}

	return id;
}

static TextureData get_texture_data(int textureId)
{
	return textures[textureId];
}

static int del_texture(int textureId)
{
	int ret = -1;

	for (int i = 0; i < MAX_TEXTURES; i++)
	{
		if (textures[i].id == textureId)
		{
			vita2d_free_texture(textures[i].texture);
			textures[i].id = -1;
			textures[i].texture = NULL;
			ret = 0;
			break;
		}
	}

	return ret;
}

static JSValue vitajs_clear(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	uint32_t colorR, colorG, colorB, colorA = 0;
	JS_ToUint32(ctx, &colorR, argv[0]);
	JS_ToUint32(ctx, &colorG, argv[1]);
	JS_ToUint32(ctx, &colorB, argv[2]);
	JS_ToUint32(ctx, &colorA, argv[3]);

	if (argc > 0)
		vita2d_set_clear_color(RGBA8(colorR, colorG, colorB, colorA));

	vita2d_clear_screen();
	return JS_UNDEFINED;
}

static JSValue vitajs_start_drawing(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	vita2d_start_drawing();
	return JS_UNDEFINED;
}

static JSValue vitajs_start_drawing_advanced(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
#warning TODO: vitajs_start_drawing_advanced
	return JS_ThrowInternalError(ctx, "start_drawing_advanced not implemented yet.");
	/*
	if (argc != 2)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected two (target: vita2d_texture, flags: number)");

	unsigned int flags;
	JS_GetOpaque2(ctx, argv[0], &emptyTexture);
	JS_ToUint32(ctx, &flags, argv[1]);

	vita2d_start_drawing_advanced(emptyTexture, flags);
	return JS_UNDEFINED;
	*/
}

static JSValue vitajs_end_drawing(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	vita2d_end_drawing();
	return JS_UNDEFINED;
}

static JSValue vitajs_common_dialog_update(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	vita2d_common_dialog_update();
	return JS_UNDEFINED;
}

static JSValue vitajs_swap_buffers(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	vita2d_swap_buffers();
	return JS_UNDEFINED;
}

static JSValue vitajs_wait_rendering_done(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	vita2d_wait_rendering_done();
	return JS_UNDEFINED;
}

static JSValue vitajs_wait_vblank_start(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	sceDisplayWaitVblankStart();
	return JS_UNDEFINED;
}

static JSValue vitajs_print(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *str;
	if (argc == 1)
		str = JS_ToCStringLen2(ctx, NULL, argv[0], 0);

	printfFontText(50, 50, 20, RGBA8(0, 255, 0, 255), str);
	return JS_UNDEFINED;
}

static JSValue vitajs_gmx_get_context(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	gmxContext = vita2d_get_context();
	return JS_UNDEFINED;
}

static JSValue vitajs_set_region_clip(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 5)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected five (mode: number, x_min: number, y_min: number, x_max: number, y_max: number)");

	SceGxmRegionClipMode mode;
	unsigned int x_min, y_min, x_max, y_max;

	JS_ToUint32(ctx, &mode, argv[0]);
	JS_ToUint32(ctx, &x_min, argv[1]);
	JS_ToUint32(ctx, &y_min, argv[2]);
	JS_ToUint32(ctx, &x_max, argv[3]);
	JS_ToUint32(ctx, &y_max, argv[4]);

	vita2d_set_region_clip(mode, x_min, y_min, x_max, y_max);

	JS_FreeValue(ctx, argv[0]);
	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	JS_FreeValue(ctx, argv[3]);
	JS_FreeValue(ctx, argv[4]);
	return JS_UNDEFINED;
}

static JSValue vitajs_disable_clipping(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	vita2d_disable_clipping();
	return JS_UNDEFINED;
}

static JSValue vitajs_get_clipping_enabled(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	vita2d_get_clipping_enabled();
	return JS_UNDEFINED;
}

static JSValue vitajs_set_clip_rectangle(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 4)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected four (x_min, y_min, x_max, y_max: number)");

	int x_min, y_min, x_max, y_max;

	JS_ToInt32(ctx, &x_min, argv[0]);
	JS_ToInt32(ctx, &y_min, argv[1]);
	JS_ToInt32(ctx, &x_max, argv[2]);
	JS_ToInt32(ctx, &y_max, argv[3]);

	vita2d_set_clip_rectangle(x_min, y_min, x_max, y_max);

	JS_FreeValue(ctx, argv[0]);
	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	JS_FreeValue(ctx, argv[3]);
	return JS_UNDEFINED;
}

static JSValue vitajs_get_clip_rectangle(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 4)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected four (x_min, y_min, x_max, y_max: number)");

	uint32_t x_min, y_min, x_max, y_max;
	JS_ToUint32(ctx, &x_min, argv[0]);
	JS_ToUint32(ctx, &y_min, argv[1]);
	JS_ToUint32(ctx, &x_max, argv[2]);
	JS_ToUint32(ctx, &y_max, argv[3]);

	vita2d_get_clip_rectangle(x_min, y_min, x_max, y_max);

	JS_FreeValue(ctx, argv[0]);
	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	JS_FreeValue(ctx, argv[3]);
	return JS_UNDEFINED;
}

static JSValue vitajs_set_blend_mode_add(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (enable: number)");

	uint32_t enable = 0;
	JS_ToUint32(ctx, &enable, argv[0]);

	vita2d_set_blend_mode_add(enable);

	JS_FreeValue(ctx, argv[0]);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_pixel(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 3)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected three (x, y, color: number)");

	uint32_t x, y, color = 0;
	JS_ToUint32(ctx, &x, argv[0]);
	JS_ToUint32(ctx, &y, argv[1]);
	JS_ToUint32(ctx, &color, argv[2]);

	vita2d_draw_pixel(x, y, color);

	JS_FreeValue(ctx, argv[0]);
	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_line(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 5)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (enable: number)");

	uint32_t x0, y0, x1, y1, color = 0;
	JS_ToUint32(ctx, &x0, argv[0]);
	JS_ToUint32(ctx, &y0, argv[1]);
	JS_ToUint32(ctx, &x1, argv[2]);
	JS_ToUint32(ctx, &y1, argv[3]);
	JS_ToUint32(ctx, &color, argv[4]);

	vita2d_draw_line(x0, y0, x1, y1, color);

	JS_FreeValue(ctx, argv[0]);
	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	JS_FreeValue(ctx, argv[3]);
	JS_FreeValue(ctx, argv[4]);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_rectangle(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 5)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected five (x, y, w, h, color: number)");

	uint32_t x, y, w, h, color = 0;
	JS_ToUint32(ctx, &x, argv[0]);
	JS_ToUint32(ctx, &y, argv[1]);
	JS_ToUint32(ctx, &w, argv[2]);
	JS_ToUint32(ctx, &h, argv[3]);
	JS_ToUint32(ctx, &color, argv[4]);

	vita2d_draw_rectangle(x, y, w, h, color);

	JS_FreeValue(ctx, argv[0]);
	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	JS_FreeValue(ctx, argv[3]);
	JS_FreeValue(ctx, argv[4]);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_fill_circle(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 4)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected four (radius: float, x, y, color: number)");

	uint32_t x, y, color = 0;
	float radius = 0;
	JS_ToUint32(ctx, &radius, argv[0]);
	JS_ToUint32(ctx, &x, argv[1]);
	JS_ToUint32(ctx, &y, argv[2]);
	JS_ToUint32(ctx, &color, argv[3]);

	vita2d_draw_fill_circle(x, y, radius, color);

	JS_FreeValue(ctx, argv[0]);
	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	JS_FreeValue(ctx, argv[3]);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_array1(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 6)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected six (mode: SceGxmPrimitiveType, x, y, z, color, count: number)");

	SceGxmPrimitiveType mode = SCE_GXM_PRIMITIVE_TRIANGLES;
	const vita2d_color_vertex *vertices;
	uint32_t count = 0;
	JS_ToUint32(ctx, &mode, argv[0]);
	JS_ToUint32(ctx, &vertices->x, argv[1]);
	JS_ToUint32(ctx, &vertices->y, argv[2]);
	JS_ToUint32(ctx, &vertices->z, argv[3]);
	JS_ToUint32(ctx, &vertices->color, argv[4]);
	JS_ToUint32(ctx, count, argv[5]);

	vita2d_draw_array(mode, vertices, count);

	JS_FreeValue(ctx, argv[0]);
	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	JS_FreeValue(ctx, argv[3]);
	JS_FreeValue(ctx, argv[4]);
	JS_FreeValue(ctx, argv[5]);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_array2(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 3)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected three (mode: number, vertices: vita2d_color_vertex, count: number)");

	uint32_t mode;
	vita2d_color_vertex *vertices;
	uint32_t count;

	JS_ToUint32(ctx, &mode, argv[0]);
	JS_ToFloat32(ctx, (float *)&count, argv[1]);

	vita2d_draw_array((SceGxmPrimitiveType)mode, vertices, count);

	JS_FreeValue(ctx, argv[0]);
	JS_FreeValue(ctx, argv[1]);
	return JS_UNDEFINED;
}

static JSValue vitajs_create_empty_texture(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 2)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected two (w, h: number)");

	unsigned int w, h;
	JS_ToUint32(ctx, &w, argv[0]);
	JS_ToUint32(ctx, &h, argv[1]);
	JS_FreeValue(ctx, argv[0]);
	JS_FreeValue(ctx, argv[1]);

	vita2d_texture *texture = vita2d_create_empty_texture(w, h);

	int textureId = 0;
	if (textureId = new_texture(texture) < 0)
		return JS_ThrowInternalError(ctx, "failed to create empty texture (%i)", textureId);

	return JS_NewUint32(ctx, textureId);
}

static JSValue vitajs_create_empty_texture_format(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 3)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected three (w, h, format: number)");

	unsigned int w, h;
	SceGxmTextureFormat format;
	JS_ToUint32(ctx, &w, argv[0]);
	JS_ToUint32(ctx, &h, argv[1]);
	JS_ToUint32(ctx, &format, argv[2]);
	JS_FreeValue(ctx, argv[0]);
	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);

	vita2d_texture *texture = vita2d_create_empty_texture_format(w, h, format);

	int textureId = 0;
	if (textureId = new_texture(texture) < 0)
		return JS_ThrowInternalError(ctx, "failed to create empty texture with format (%i)", textureId);

	return JS_NewUint32(ctx, textureId);
}

static JSValue vitajs_create_empty_texture_rendertarget(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 3)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected three (width, height, format: number)");

	unsigned int w, h;
	SceGxmTextureFormat format;
	JS_ToUint32(ctx, &w, argv[0]);
	JS_ToUint32(ctx, &h, argv[1]);
	JS_ToUint32(ctx, &format, argv[2]);
	JS_FreeValue(ctx, argv[0]);
	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);

	vita2d_texture *texture = vita2d_create_empty_texture_rendertarget(w, h, format);

	int textureId = 0;
	if (textureId = new_texture(texture) < 0)
		return JS_ThrowInternalError(ctx, "failed to create empty render target texture (%i)", textureId);

	return JS_NewUint32(ctx, textureId);
}

static JSValue vitajs_free_texture(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (textureId: number)");

	int textureId = 0;

	JS_ToUint32(ctx, &textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (del_texture(textureId) < 0)
		return JS_ThrowInternalError(ctx, "trying to free a null texture, id %i.", textureId);

	return JS_NewBool(ctx, 1);
}

static JSValue vitajs_texture_get_width(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (textureId: number)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	unsigned int width = vita2d_texture_get_width(textures[textureId].texture);
	return JS_NewUint32(ctx, width);
}

static JSValue vitajs_texture_get_height(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (textureId: number)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	unsigned int height = vita2d_texture_get_height(textures[textureId].texture);
	return JS_NewUint32(ctx, height);
}

static JSValue vitajs_texture_get_stride(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (textureId: number)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	unsigned int stride = vita2d_texture_get_stride(textures[textureId].texture);
	return JS_NewUint32(ctx, stride);
}

static JSValue vitajs_texture_get_format(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (textureId: number)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	SceGxmTextureFormat format = vita2d_texture_get_format(textures[textureId].texture);
	return JS_NewUint32(ctx, format);
}

static JSValue vitajs_texture_get_datap(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 2)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (textureId: number, isShared: bool)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	void *data = vita2d_texture_get_datap(textures[textureId].texture);

	bool isShared = JS_ToBool(ctx, argv[1]);
	JS_FreeValue(ctx, argv[1]);

	return JS_NewArrayBuffer(ctx, data, vita2d_texture_get_stride(textures[textureId].texture) * vita2d_texture_get_height(textures[textureId].texture), NULL, NULL, isShared);
}

static JSValue vitajs_texture_get_palette(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 2)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (textureId: number, isShared: bool)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	void *data = vita2d_texture_get_palette(textures[textureId].texture);

	bool isShared = JS_ToBool(ctx, argv[1]);
	JS_FreeValue(ctx, argv[1]);

	return JS_NewArrayBuffer(ctx, data, vita2d_texture_get_stride(textures[textureId].texture) * vita2d_texture_get_height(textures[textureId].texture), NULL, NULL, isShared);
}

static JSValue vitajs_texture_get_min_filter(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (textureId: number)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	SceGxmTextureFilter min_filter = vita2d_texture_get_min_filter(textures[textureId].texture);
	return JS_NewInt32(ctx, min_filter);
}

static JSValue vitajs_texture_get_mag_filter(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (textureId: number)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	SceGxmTextureFilter mag_filter = vita2d_texture_get_mag_filter(textures[textureId].texture);
	return JS_NewInt32(ctx, mag_filter);
}

static JSValue vitajs_texture_set_filters(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 3)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected three (textureId: number, min_filter: number, mag_filter: number)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	uint32_t min_filter, mag_filter;
	JS_ToUint32(ctx, &min_filter, argv[1]);
	JS_ToUint32(ctx, &mag_filter, argv[2]);

	vita2d_texture_set_filters(textures[textureId].texture, min_filter, mag_filter);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_texture(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 3)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected three (textureId: number, x, y: number)");

	int *textureId = 0;

	JS_ToUint32(ctx, &textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	TextureData textureData = get_texture_data(textureId);
	if (!textureData.texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	uint32_t x, y = 0;

	JS_ToUint32(ctx, &x, argv[1]);
	JS_ToUint32(ctx, &y, argv[2]);

	// printf("texture: x: %i , y: %i\n", x, y);
	vita2d_draw_texture(textureData.texture, (float)x, (float)y);

	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_texture_rotate(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 4)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected four (textureId: number, x: number, y: number, rad: number)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	float x, y, rad;
	JS_ToFloat64(ctx, &x, argv[1]);
	JS_ToFloat64(ctx, &y, argv[2]);
	JS_ToFloat64(ctx, &rad, argv[3]);

	vita2d_draw_texture_rotate(textures[textureId].texture, x, y, rad);

	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	JS_FreeValue(ctx, argv[3]);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_texture_tint_rotate_hotspot(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 7)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected seven (textureId: number, x: number, y: number, rad: number, center_x: number, center_y: number, color: number)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	float x, y, rad, center_x, center_y;
	unsigned int color;
	JS_ToFloat64(ctx, &x, argv[1]);
	JS_ToFloat64(ctx, &y, argv[2]);
	JS_ToFloat64(ctx, &rad, argv[3]);
	JS_ToFloat64(ctx, &center_x, argv[4]);
	JS_ToFloat64(ctx, &center_y, argv[5]);
	JS_ToUint32(ctx, &color, argv[6]);

	vita2d_draw_texture_tint_rotate_hotspot(textures[textureId].texture, x, y, rad, center_x, center_y, color);

	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	JS_FreeValue(ctx, argv[3]);
	JS_FreeValue(ctx, argv[4]);
	JS_FreeValue(ctx, argv[5]);
	JS_FreeValue(ctx, argv[6]);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_texture_tint_scale(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 6)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected six (textureId: number, x: number, y: number, x_scale: number, y_scale: number, color: number)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	float x, y, x_scale, y_scale;
	unsigned int color;
	JS_ToFloat64(ctx, &x, argv[1]);
	JS_ToFloat64(ctx, &y, argv[2]);
	JS_ToFloat64(ctx, &x_scale, argv[3]);
	JS_ToFloat64(ctx, &y_scale, argv[4]);
	JS_ToUint32(ctx, &color, argv[5]);

	vita2d_draw_texture_tint_scale(textures[textureId].texture, x, y, x_scale, y_scale, color);

	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	JS_FreeValue(ctx, argv[3]);
	JS_FreeValue(ctx, argv[4]);
	JS_FreeValue(ctx, argv[5]);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_texture_tint_part(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 8)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected eight (textureId: number, x: number, y: number, tex_x: number, tex_y: number, tex_w: number, tex_h: number, color: number)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	float x, y, tex_x, tex_y, tex_w, tex_h;
	unsigned int color;
	JS_ToFloat64(ctx, &x, argv[1]);
	JS_ToFloat64(ctx, &y, argv[2]);
	JS_ToFloat64(ctx, &tex_x, argv[3]);
	JS_ToFloat64(ctx, &tex_y, argv[4]);
	JS_ToFloat64(ctx, &tex_w, argv[5]);
	JS_ToFloat64(ctx, &tex_h, argv[6]);
	JS_ToUint32(ctx, &color, argv[7]);

	vita2d_draw_texture_tint_part(textures[textureId].texture, x, y, tex_x, tex_y, tex_w, tex_h, color);

	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	JS_FreeValue(ctx, argv[3]);
	JS_FreeValue(ctx, argv[4]);
	JS_FreeValue(ctx, argv[5]);
	JS_FreeValue(ctx, argv[6]);
	JS_FreeValue(ctx, argv[7]);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_texture_tint_part_scale(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 10)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected ten (textureId: number, x: number, y: number, tex_x: number, tex_y: number, tex_w: number, tex_h: number, x_scale: number, y_scale: number, color: number)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	float x, y, tex_x, tex_y, tex_w, tex_h, x_scale, y_scale;
	unsigned int color;
	JS_ToFloat64(ctx, &x, argv[1]);
	JS_ToFloat64(ctx, &y, argv[2]);
	JS_ToFloat64(ctx, &tex_x, argv[3]);
	JS_ToFloat64(ctx, &tex_y, argv[4]);
	JS_ToFloat64(ctx, &tex_w, argv[5]);
	JS_ToFloat64(ctx, &tex_h, argv[6]);
	JS_ToFloat64(ctx, &x_scale, argv[7]);
	JS_ToFloat64(ctx, &y_scale, argv[8]);
	JS_ToUint32(ctx, &color, argv[9]);

	vita2d_draw_texture_tint_part_scale(textures[textureId].texture, x, y, tex_x, tex_y, tex_w, tex_h, x_scale, y_scale, color);

	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	JS_FreeValue(ctx, argv[3]);
	JS_FreeValue(ctx, argv[4]);
	JS_FreeValue(ctx, argv[5]);
	JS_FreeValue(ctx, argv[6]);
	JS_FreeValue(ctx, argv[7]);
	JS_FreeValue(ctx, argv[8]);
	JS_FreeValue(ctx, argv[9]);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_texture_tint_scale_rotate_hotspot(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 9)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected nine (textureId: number, x: number, y: number, x_scale: number, y_scale: number, rad: number, center_x: number, center_y: number, color: number)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	float x, y, x_scale, y_scale, rad, center_x, center_y;
	unsigned int color;
	JS_ToFloat64(ctx, &x, argv[1]);
	JS_ToFloat64(ctx, &y, argv[2]);
	JS_ToFloat64(ctx, &x_scale, argv[3]);
	JS_ToFloat64(ctx, &y_scale, argv[4]);
	JS_ToFloat64(ctx, &rad, argv[5]);
	JS_ToFloat64(ctx, &center_x, argv[6]);
	JS_ToFloat64(ctx, &center_y, argv[7]);
	JS_ToUint32(ctx, &color, argv[8]);

	vita2d_draw_texture_tint_scale_rotate_hotspot(textures[textureId].texture, x, y, x_scale, y_scale, rad, center_x, center_y, color);

	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	JS_FreeValue(ctx, argv[3]);
	JS_FreeValue(ctx, argv[4]);
	JS_FreeValue(ctx, argv[5]);
	JS_FreeValue(ctx, argv[6]);
	JS_FreeValue(ctx, argv[7]);
	JS_FreeValue(ctx, argv[8]);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_texture_tint_scale_rotate(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 7)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected seven (textureId: number, x: number, y: number, x_scale: number, y_scale: number, rad: number, color: number)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	float x, y, x_scale, y_scale, rad;
	unsigned int color;
	JS_ToFloat64(ctx, &x, argv[1]);
	JS_ToFloat64(ctx, &y, argv[2]);
	JS_ToFloat64(ctx, &x_scale, argv[3]);
	JS_ToFloat64(ctx, &y_scale, argv[4]);
	JS_ToFloat64(ctx, &rad, argv[5]);
	JS_ToUint32(ctx, &color, argv[6]);

	vita2d_draw_texture_tint_scale_rotate(textures[textureId].texture, x, y, x_scale, y_scale, rad, color);

	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	JS_FreeValue(ctx, argv[3]);
	JS_FreeValue(ctx, argv[4]);
	JS_FreeValue(ctx, argv[5]);
	JS_FreeValue(ctx, argv[6]);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_texture_part_tint_scale_rotate(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 11)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected eleven (textureId: number, x: number, y: number, tex_x: number, tex_y: number, tex_w: number, tex_h: number, x_scale: number, y_scale: number, rad: number, color: number)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	float x, y, tex_x, tex_y, tex_w, tex_h, x_scale, y_scale, rad;
	unsigned int color;
	JS_ToFloat64(ctx, &x, argv[1]);
	JS_ToFloat64(ctx, &y, argv[2]);
	JS_ToFloat64(ctx, &tex_x, argv[3]);
	JS_ToFloat64(ctx, &tex_y, argv[4]);
	JS_ToFloat64(ctx, &tex_w, argv[5]);
	JS_ToFloat64(ctx, &tex_h, argv[6]);
	JS_ToFloat64(ctx, &x_scale, argv[7]);
	JS_ToFloat64(ctx, &y_scale, argv[8]);
	JS_ToFloat64(ctx, &rad, argv[9]);
	JS_ToUint32(ctx, &color, argv[10]);

	vita2d_draw_texture_part_tint_scale_rotate(textures[textureId].texture, x, y, tex_x, tex_y, tex_w, tex_h, x_scale, y_scale, rad, color);

	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	JS_FreeValue(ctx, argv[3]);
	JS_FreeValue(ctx, argv[4]);
	JS_FreeValue(ctx, argv[5]);
	JS_FreeValue(ctx, argv[6]);
	JS_FreeValue(ctx, argv[7]);
	JS_FreeValue(ctx, argv[8]);
	JS_FreeValue(ctx, argv[9]);
	JS_FreeValue(ctx, argv[10]);
	return JS_UNDEFINED;
}

static JSValue vitajs_draw_array_textured(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 4)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected four (textureId, mode, color: number, vertices: object)");

	int textureId = 0;

	JS_ToUint32(ctx, textureId, argv[0]);
	JS_FreeValue(ctx, argv[0]);

	if (textureId < 0 || textureId > MAX_TEXTURES)
		return JS_ThrowRangeError(ctx, "textureId must be greater than 0 and lesser than %i.", MAX_TEXTURES);

	if (!get_texture_data(textureId).texture)
		return JS_ThrowInternalError(ctx, "trying to access a null texture, id %i.", textureId);

	uint32_t mode;
	vita2d_texture_vertex *vertices;
	uint32_t count;
	uint32_t color;

	textures[textureId].texture = JS_GetOpaque2(ctx, argv[0], js_texture_class_id);

	if (textures[textureId].texture == NULL)
		return JS_EXCEPTION;

	JS_ToUint32(ctx, &mode, argv[1]);
	JS_FreeValue(ctx, argv[1]);

	JSValue buffer = argv[3];
	uint8_t *data = JS_GetArrayBuffer(ctx, &count, buffer);
	JS_FreeValue(ctx, argv[3]);

	if (!data)
		return JS_EXCEPTION;

	// Calculate the number of vertices based on the size of the buffer and the size of each vertex
	count /= sizeof(vita2d_texture_vertex);

	vertices = (vita2d_texture_vertex *)data;

	JS_ToUint32(ctx, &color, argv[2]);
	JS_FreeValue(ctx, argv[2]);

	vita2d_draw_array_textured(textures[textureId].texture, (SceGxmPrimitiveType)mode, vertices, count, color);
	return JS_UNDEFINED;
}

static JSValue vitajs_load_PNG_file(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (filename: string)");

	const char *filename = JS_ToCString(ctx, argv[0]);
	if (!filename)
		return JS_EXCEPTION;

	vita2d_texture *texture = vita2d_load_PNG_file(filename);
	JS_FreeCString(ctx, filename);

	if (!texture)
		return JS_ThrowInternalError(ctx, "failed to create png image (%s).", filename);

	int textureId = new_texture(texture);
	if (textureId < 0)
		return JS_ThrowInternalError(ctx, "failed to create png texture (%i)", textureId);

	return JS_NewUint32(ctx, textureId);
}

static JSValue vitajs_load_JPEG_file(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (filename: string)");

	const char *filename = JS_ToCString(ctx, argv[0]);
	if (!filename)
		return JS_EXCEPTION;

	vita2d_texture *texture = vita2d_load_JPEG_file(filename);
	JS_FreeCString(ctx, filename);

	if (!texture)
		return JS_ThrowInternalError(ctx, "failed to create jpg image.");

	int textureId = 0;
	if (textureId = new_texture(texture) < 0)
		return JS_ThrowInternalError(ctx, "failed to create jpg texture.");

	return JS_NewUint32(ctx, textureId);
}

static JSValue vitajs_load_BMP_file(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (filename: string)");

	const char *filename = JS_ToCString(ctx, argv[0]);
	if (!filename)
		return JS_EXCEPTION;

	vita2d_texture *texture = vita2d_load_BMP_file(filename);
	JS_FreeCString(ctx, filename);

	if (!texture)
		return JS_ThrowInternalError(ctx, "failed to create bmp image.");

	int textureId = 0;
	if (textureId = new_texture(texture) < 0)
		return JS_ThrowInternalError(ctx, "failed to create bmp texture.");

	return JS_NewUint32(ctx, textureId);
}

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("clear", 4, vitajs_clear),
	JS_CFUNC_DEF("start_drawing", 0, vitajs_start_drawing),
	JS_CFUNC_DEF("start_drawing_advanced", 2, vitajs_start_drawing_advanced),
	JS_CFUNC_DEF("end_drawing", 0, vitajs_end_drawing),
	JS_CFUNC_DEF("common_dialog_update", 0, vitajs_common_dialog_update),
	JS_CFUNC_DEF("swap_buffers", 0, vitajs_swap_buffers),
	JS_CFUNC_DEF("wait_rendering_done", 0, vitajs_wait_rendering_done),
	JS_CFUNC_DEF("wait_vblank", 0, vitajs_wait_vblank_start),
	JS_CFUNC_DEF("print", 1, vitajs_print),						// REMOVE?
	JS_CFUNC_DEF("gmx_get_context", 0, vitajs_gmx_get_context), // NOT TESTED
	JS_CFUNC_DEF("set_region_clip", 5, vitajs_set_region_clip),
	JS_CFUNC_DEF("disable_clipping", 0, vitajs_disable_clipping),
	JS_CFUNC_DEF("get_clipping_enabled", 0, vitajs_get_clipping_enabled),
	JS_CFUNC_DEF("set_clip_rectangle", 4, vitajs_set_clip_rectangle),
	JS_CFUNC_DEF("get_clip_rectangle", 4, vitajs_get_clip_rectangle),
	JS_CFUNC_DEF("set_blend_mode_add", 1, vitajs_set_blend_mode_add),
	JS_CFUNC_DEF("draw_pixel", 3, vitajs_draw_pixel),
	JS_CFUNC_DEF("draw_line", 5, vitajs_draw_line),
	JS_CFUNC_DEF("draw_rectangle", 5, vitajs_draw_rectangle),
	JS_CFUNC_DEF("draw_fill_circle", 4, vitajs_draw_fill_circle),
	JS_CFUNC_DEF("draw_array", 6, vitajs_draw_array1),	// NOT TESTED
	JS_CFUNC_DEF("draw_array2", 3, vitajs_draw_array2), // NOT TESTED
	JS_CFUNC_DEF("create_empty_texture", 3, vitajs_create_empty_texture),
	JS_CFUNC_DEF("create_empty_texture_format", 3, vitajs_create_empty_texture_format),
	JS_CFUNC_DEF("create_empty_texture_rendertarget", 3, vitajs_create_empty_texture_rendertarget),
	JS_CFUNC_DEF("free_texture", 1, vitajs_free_texture),
	JS_CFUNC_DEF("texture_get_width", 1, vitajs_texture_get_width),
	JS_CFUNC_DEF("texture_get_height", 1, vitajs_texture_get_height),
	JS_CFUNC_DEF("texture_get_stride", 1, vitajs_texture_get_stride),
	JS_CFUNC_DEF("texture_get_format", 1, vitajs_texture_get_format),
	JS_CFUNC_DEF("texture_get_datap", 2, vitajs_texture_get_datap),
	JS_CFUNC_DEF("texture_get_palette", 2, vitajs_texture_get_palette),
	JS_CFUNC_DEF("texture_get_min_filter", 1, vitajs_texture_get_min_filter),
	JS_CFUNC_DEF("texture_get_mag_filter", 1, vitajs_texture_get_mag_filter),
	JS_CFUNC_DEF("texture_set_filters", 3, vitajs_texture_set_filters),
	JS_CFUNC_DEF("draw_texture", 3, vitajs_draw_texture),
	JS_CFUNC_DEF("draw_texture_rotate", 4, vitajs_draw_texture_rotate),
	JS_CFUNC_DEF("draw_texture_tint_rotate_hotspot", 7, vitajs_draw_texture_tint_rotate_hotspot),
	JS_CFUNC_DEF("draw_texture_tint_scale", 6, vitajs_draw_texture_tint_scale),
	JS_CFUNC_DEF("draw_texture_tint_part", 8, vitajs_draw_texture_tint_part),
	JS_CFUNC_DEF("draw_texture_tint_part_scale", 10, vitajs_draw_texture_tint_part_scale),
	JS_CFUNC_DEF("draw_texture_tint_scale_rotate_hotspot", 9, vitajs_draw_texture_tint_scale_rotate_hotspot),
	JS_CFUNC_DEF("draw_texture_tint_scale_rotate", 7, vitajs_draw_texture_tint_scale_rotate),
	JS_CFUNC_DEF("draw_texture_part_tint_scale_rotate", 11, vitajs_draw_texture_part_tint_scale_rotate),
	JS_CFUNC_DEF("draw_array_textured", 4, vitajs_draw_array_textured),
	JS_CFUNC_DEF("load_png_file", 1, vitajs_load_PNG_file),
	JS_CFUNC_DEF("load_jpg_file", 1, vitajs_load_JPEG_file),
	JS_CFUNC_DEF("load_bmp_file", 1, vitajs_load_BMP_file),
};

static int screen_init(JSContext *ctx, JSModuleDef *m)
{
	for (int i = 0; i < MAX_TEXTURES; i++)
	{
		textures[i].id = -1;
		textures[i].texture = NULL;
	}

	return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *vitajs_screen_init(JSContext *ctx)
{
	return vitajs_push_module(ctx, screen_init, module_funcs, countof(module_funcs), "Screen");
}
