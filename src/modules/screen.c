#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "../graphics.h"
#include "../env.h"

static JSValue vitajs_clear2(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	uint32_t color = 0x00;
	if (argc == 1)
		JS_ToInt32(ctx, &color, argv[0]);
	clearScreenWithColor(color);
	return JS_UNDEFINED;
}

static JSValue vitajs_clear(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	uint32_t colorR = 0x00;
	uint32_t colorG = 0x00;
	uint32_t colorB = 0x00;
	uint32_t colorA = 0x00;

	if (argc > 4)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected at most four (colorR?, colorG?, colorB?, colorA?) [0-255]");

	JS_ToUint32(ctx, &colorR, argv[0]);
	JS_ToUint32(ctx, &colorG, argv[1]);
	JS_ToUint32(ctx, &colorB, argv[2]);
	JS_ToUint32(ctx, &colorA, argv[3]);

	// clearScreenWithColor(RGBA8(colorR, colorG, colorB, colorA));
	clearScreenWithColor(RGBA8(colorR, colorG, colorB, colorA));

	return JS_UNDEFINED;
}

static JSValue vitajs_start_drawing(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	start_drawing();
	return JS_UNDEFINED;
}

static JSValue vitajs_end_drawing(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	end_drawing();
	waitVblankStart();
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

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("clear", 4, vitajs_clear),
	JS_CFUNC_DEF("print", 1, vitajs_print),
	JS_CFUNC_DEF("start_drawing", 1, vitajs_start_drawing),
	JS_CFUNC_DEF("end_drawing", 1, vitajs_end_drawing),
};

static int screen_init(JSContext *ctx, JSModuleDef *m)
{
	return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *vitajs_screen_init(JSContext *ctx)
{
	return vitajs_push_module(ctx, screen_init, module_funcs, countof(module_funcs), "Screen");
}
