#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "../graphics.h"
#include "../ath_env.h"

static JSValue athena_flip(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	// flipScreen();
	return JS_UNDEFINED;
}

static JSValue athena_clear(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	uint32_t color = 0x00;
	if (argc == 1)
		JS_ToInt32(ctx, &color, argv[0]);
	clearScreenWithColor(color);
	return JS_UNDEFINED;
}

static JSValue athena_vblank(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	waitVblankStart();
	return JS_UNDEFINED;
}

static JSValue athena_getFPS(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	return JS_NewFloat32(ctx, (float)*getRefreshRate());
}

static char *str_buf = NULL;
static size_t str_len = 0;
static size_t buf_len = 0;

#define SCRLOG_LENGTH 72

/*
static JSValue athena_scrlog(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	unsigned int old_len, len = 0;
	const char *in_str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str_buf)
	{
		str_buf = malloc(512);
		buf_len = 512;
		memset(str_buf, 0, buf_len);
	}

	old_len = str_len;
	str_len += len + 1;

	while (buf_len < str_len)
	{
		buf_len += 512;
		str_buf = realloc(str_buf, buf_len);
	}

	if (old_len > 0)
	{
		str_buf[old_len - 1] = '\n';
	}

	strcat(str_buf, in_str);

	clearScreen(GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00));

	if (str_len > 0)
	{
		printFontMText(str_buf, 0, 0, 0.5f, 0x80808080);
	}

	flipScreen();

	return JS_UNDEFINED;
}
*/

/*
static JSValue athena_cls(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (str_len > 0)
	{
		memset(str_buf, 0, str_len);
		str_len = 0;
	}

	return JS_UNDEFINED;
}
*/

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("flip", 0, athena_flip),
	JS_CFUNC_DEF("clear", 1, athena_clear),
	// JS_CFUNC_DEF("getFreeVRAM", 0, athena_getFreeVRAM),
	JS_CFUNC_DEF("getFPS", 1, athena_getFPS),
	JS_CFUNC_DEF("waitVblankStart", 0, athena_vblank),
	// JS_CFUNC_DEF("setFrameCounter", 1, athena_fcount),
	// JS_CFUNC_DEF("log", 1, athena_scrlog),
	// JS_CFUNC_DEF("cls", 0, athena_cls),
};

static int screen_init(JSContext *ctx, JSModuleDef *m)
{
	return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_screen_init(JSContext *ctx)
{
	return athena_push_module(ctx, screen_init, module_funcs, countof(module_funcs), "Screen");
}
