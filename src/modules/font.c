#include "../graphics.h"
#include "../env.h"

#define MAX_FONTS 10

static JSClassID js_font_class_id;
static JSClassID js_system_pvf_config_class_id;
static JSClassID js_pvf_class_id;

typedef struct FontData
{
    int id;
    vita2d_font *font;
} FontData;

static FontData fonts[MAX_FONTS];

static int new_font(const vita2d_font *font)
{
    int id = -1;

    for (int i = 0; i < MAX_FONTS; i++)
    {
        if (fonts[i].id == -1 && !fonts[i].font)
        {
            fonts[i].id = i;
            fonts[i].font = font;
            id = i;
            break;
        }
    }

    return id;
}

static FontData get_font_data(int fontId)
{
    FontData data;
    data.id = -1;
    data.font = NULL;

    if (fontId < MAX_FONTS && fonts[fontId].id != -1 && fonts[fontId].font)
    {
        data.id = fonts[fontId].id;
        data.font = fonts[fontId].font;
    }

    return data;
}

static int del_font(int fontId)
{
    int ret = -1;

    for (int i = 0; i < MAX_FONTS; i++)
    {
        if (fonts[i].id == fontId)
        {
            vita2d_free_font(fonts[i].font);
            fonts[i].id = -1;
            fonts[i].font = NULL;
            ret = 0;
            break;
        }
    }

    return ret;
}

static JSValue vitajs_load_font_file(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    if (argc != 1)
        return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (filename: string)");

    const char *filename = JS_ToCString(ctx, argv[0]);
    if (!filename)
        return JS_EXCEPTION;

    vita2d_font *font = vita2d_load_font_file(filename);
    JS_FreeCString(ctx, filename);

    if (!new_font(font))
        return JS_EXCEPTION;

    JSValue obj = JS_NewObjectClass(ctx, JS_NewClassID(js_font_class_id));
    if (JS_IsException(obj))
    {
        vita2d_free_font(font);
        return obj;
    }

    JS_SetOpaque(obj, font);
    return obj;
}

static JSValue vitajs_free_font(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    if (argc != 1)
        return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (font: Vita2DFont)");

    vita2d_font *font = JS_GetOpaque2(ctx, argv[0], JS_NewClassID(js_font_class_id));
    if (!font)
        return JS_EXCEPTION;

    vita2d_free_font(font);
    JS_FreeValue(ctx, argv[0]);

    return JS_UNDEFINED;
}

static JSValue vitajs_font_draw_text(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    if (argc != 6)
        return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected six (font: vita2d_font, x, y, color, size: number, text: string)");

    vita2d_font *font;
    int x, y;
    unsigned int color, size;
    const char *text;

    JS_GetOpaque2(ctx, this_val, &font);
    if (!font)
        return JS_EXCEPTION;

    JS_ToInt32(ctx, &x, argv[1]);
    JS_ToInt32(ctx, &y, argv[2]);
    JS_ToUint32(ctx, &color, argv[3]);
    JS_ToUint32(ctx, &size, argv[4]);
    text = JS_ToCString(ctx, argv[5]);
    if (!text)
        return JS_EXCEPTION;

    vita2d_font_draw_text(font, x, y, color, size, text);
    JS_FreeCString(ctx, text);

    return JS_UNDEFINED;
}

static JSValue vitajs_font_draw_text_ls(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    if (argc < 7)
        return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected at least seven (font: vita2d_font, x, y, linespace, color, size: number, text: string)");

    vita2d_font *font;
    int x, y;
    float linespace;
    unsigned int color, size;
    const char *text;

    JS_GetOpaque2(ctx, this_val, &font);
    if (!font)
        return JS_EXCEPTION;

    JS_ToInt32(ctx, &x, argv[1]);
    JS_ToInt32(ctx, &y, argv[2]);
    JS_ToFloat64(ctx, &linespace, argv[3]);
    JS_ToUint32(ctx, &color, argv[4]);
    JS_ToUint32(ctx, &size, argv[5]);
    text = JS_ToCString(ctx, argv[6]);
    if (!text)
        return JS_EXCEPTION;

    vita2d_font_draw_text_ls(font, x, y, linespace, color, size, text);
    JS_FreeCString(ctx, text);

    return JS_UNDEFINED;
}

static JSValue vitajs_font_text_dimensions(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    if (argc != 3)
        return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected three (font: vita2d_font, size: number, text: string)");

    vita2d_font *font = JS_GetOpaque2(ctx, this_val, js_font_class_id);
    if (!font)
        return JS_EXCEPTION;

    uint32_t size;
    const char *text;

    if (JS_ToUint32(ctx, &size, argv[1]) || JS_ToCString(ctx, argv[2]))
    {
        return JS_EXCEPTION;
    }

    int width, height;
    vita2d_font_text_dimensions(font, size, text, &width, &height);

    JSValue obj = JS_NewObject(ctx);
    if (JS_IsException(obj))
        return obj;

    JS_SetPropertyStr(ctx, obj, "width", JS_NewInt32(ctx, width));
    JS_SetPropertyStr(ctx, obj, "height", JS_NewInt32(ctx, height));

    return obj;
}

static JSValue vitajs_font_text_width(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    if (argc != 3)
        return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected three (font: Vita2DFont, size: number, text: string)");

    vita2d_font *font = JS_GetOpaque2(ctx, argv[0], js_font_class_id);
    if (!font)
        return JS_EXCEPTION;

    uint32_t size;
    const char *text;
    JS_ToUint32(ctx, &size, argv[1]);
    text = JS_ToCString(ctx, argv[2]);
    if (!text)
        return JS_EXCEPTION;

    int width = vita2d_font_text_width(font, size, text);
    JS_FreeCString(ctx, text);

    return JS_NewInt32(ctx, width);
}

static JSValue vitajs_font_text_height(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    if (argc != 3)
        return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected three (font: Vita2DFont, size: number, text: string)");

    vita2d_font *font = JS_GetOpaque2(ctx, argv[0], js_font_class_id);
    if (!font)
        return JS_EXCEPTION;

    uint32_t size;
    const char *text;
    JS_ToUint32(ctx, &size, argv[1]);
    text = JS_ToCString(ctx, argv[2]);
    if (!text)
        return JS_EXCEPTION;

    int height = vita2d_font_text_height(font, size, text);
    JS_FreeCString(ctx, text);

    return JS_NewInt32(ctx, height);
}

static JSValue vitajs_load_system_pvf(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    if (argc != 2)
        return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected two (numFonts: number, configs: Vita2DSystemPvfConfig)");

    uint32_t numFonts;
    vita2d_system_pvf_config *configs;

    JS_ToUint32(ctx, &numFonts, argv[0]);
    configs = JS_GetOpaque2(ctx, argv[1], js_system_pvf_config_class_id);
    if (!configs)
        return JS_EXCEPTION;

    vita2d_pvf *pvf = vita2d_load_system_pvf(numFonts, configs);
    if (!pvf)
        return JS_EXCEPTION;

    JSValue obj = JS_NewObjectClass(ctx, js_pvf_class_id);
    if (JS_IsException(obj))
    {
        vita2d_free_pvf(pvf);
        return obj;
    }

    JS_SetOpaque(obj, pvf);
    return obj;
}

static JSValue vitajs_load_default_pvf(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "no arguments expected.");

    vita2d_pvf *pvf = vita2d_load_default_pvf();
    if (!pvf)
        return JS_EXCEPTION;

    JSValue obj = JS_NewObjectClass(ctx, js_pvf_class_id);
    if (JS_IsException(obj))
    {
        vita2d_free_pvf(pvf);
        return obj;
    }

    JS_SetOpaque(obj, pvf);
    return obj;
}

static JSValue vitajs_load_custom_pvf(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    if (argc != 1)
        return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (path: string)");

    const char *path;
    path = JS_ToCString(ctx, argv[0]);
    if (!path)
        return JS_EXCEPTION;

    vita2d_pvf *pvf = vita2d_load_custom_pvf(path);
    JS_FreeCString(ctx, path);

    if (!pvf)
        return JS_EXCEPTION;

    JSValue obj = JS_NewObjectClass(ctx, js_pvf_class_id);
    if (JS_IsException(obj))
    {
        vita2d_free_pvf(pvf);
        return obj;
    }

    JS_SetOpaque(obj, pvf);
    return obj;
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("load_font_file", 1, vitajs_load_font_file),
    JS_CFUNC_DEF("free_font", 0, vitajs_free_font),
    JS_CFUNC_DEF("font_draw_text", 6, vitajs_font_draw_text),
    JS_CFUNC_DEF("font_draw_text_ls", 7, vitajs_font_draw_text_ls),
    JS_CFUNC_DEF("font_text_dimensions", 3, vitajs_font_text_dimensions),
    JS_CFUNC_DEF("font_text_width", 3, vitajs_font_text_width),
    JS_CFUNC_DEF("font_text_height", 3, vitajs_font_text_height),
    JS_CFUNC_DEF("load_system_pvf", 2, vitajs_load_system_pvf),
    JS_CFUNC_DEF("load_default_pvf", 0, vitajs_load_default_pvf),
    JS_CFUNC_DEF("load_custom_pvf", 1, vitajs_load_custom_pvf),
};

static int font_init(JSContext *ctx, JSModuleDef *m)
{
    for (int i = 0; i < MAX_FONTS; i++)
    {
        fonts[i].id = i;
        fonts[i].font = NULL;
    }

    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *vitajs_screen_init(JSContext *ctx)
{
    return vitajs_push_module(ctx, font_init, module_funcs, countof(module_funcs), "Font");
}
