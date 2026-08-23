#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../graphics.h"
#include "../env.h"

/*
 * libvita2d stores glyphs in a font-local atlas keyed only by glyph index.
 * If one vita2d_font instance is used with multiple pixel sizes, a glyph
 * rasterized for the first requested size can later be scaled for another
 * size. This is especially visible on small UI text.
 *
 * VitaJS works around that behavior without modifying libvita2d itself:
 * every requested pixel size gets its own vita2d_font instance and therefore
 * its own glyph atlas.
 *
 * Public JavaScript API stays unchanged:
 *
 *   const font = Font.load_font_file("app0:/assets/font.ttf");
 *   Font.font_draw_text(font, x, y, color, 18, "Small");
 *   Font.font_draw_text(font, x, y, color, 34, "Large");
 *
 * Internally those two calls use two independent vita2d_font instances.
 */

typedef struct FontSizeCache
{
    uint32_t size;
    vita2d_font *font;
    struct FontSizeCache *next;
} FontSizeCache;

typedef struct FontData
{
    char *filename;
    FontSizeCache *sizes;
} FontData;

static JSClassID js_font_class_id = 0;

static void vitajs_free_font_cache(JSRuntime *rt, FontData *data)
{
    if (!data)
        return;

    FontSizeCache *entry = data->sizes;

    while (entry)
    {
        FontSizeCache *next = entry->next;

        if (entry->font)
        {
            vita2d_free_font(entry->font);
            entry->font = NULL;
        }

        js_free_rt(rt, entry);
        entry = next;
    }

    data->sizes = NULL;

    if (data->filename)
    {
        js_free_rt(rt, data->filename);
        data->filename = NULL;
    }
}

static void vitajs_font_finalizer(JSRuntime *rt, JSValue val)
{
    FontData *data = JS_GetOpaque(val, js_font_class_id);

    if (!data)
        return;

    vitajs_free_font_cache(rt, data);
    js_free_rt(rt, data);
}

static JSClassDef js_font_class = {
    .class_name = "VitaJSFont",
    .finalizer = vitajs_font_finalizer,
};

static FontData *vitajs_get_font_data(JSContext *ctx, JSValueConst value)
{
    FontData *data = JS_GetOpaque2(ctx, value, js_font_class_id);

    if (!data)
        return NULL;

    if (!data->filename)
    {
        JS_ThrowTypeError(ctx, "Font has already been freed");
        return NULL;
    }

    return data;
}

/*
 * Returns a vita2d_font dedicated to one exact pixel size.
 * A new instance is created lazily the first time that size is used.
 */
static vita2d_font *vitajs_get_sized_font(
    JSContext *ctx,
    FontData *data,
    uint32_t size)
{
    if (size == 0)
    {
        JS_ThrowRangeError(ctx, "Font size must be greater than zero");
        return NULL;
    }

    FontSizeCache *entry = data->sizes;

    while (entry)
    {
        if (entry->size == size)
            return entry->font;

        entry = entry->next;
    }

    entry = js_mallocz(ctx, sizeof(*entry));

    if (!entry)
        return NULL;

    entry->font = vita2d_load_font_file(data->filename);

    if (!entry->font)
    {
        js_free(ctx, entry);
        JS_ThrowInternalError(
            ctx,
            "Unable to create font cache for size %u",
            (unsigned int)size);
        return NULL;
    }

    entry->size = size;
    entry->next = data->sizes;
    data->sizes = entry;

    return entry->font;
}

static JSValue vitajs_load_font_file(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(
            ctx,
            "load_font_file(filename: string) expects exactly one argument");

    const char *filename = JS_ToCString(ctx, argv[0]);

    if (!filename)
        return JS_EXCEPTION;

    /*
     * Preserve the previous API behavior: invalid/unreadable font files fail
     * immediately in load_font_file(), rather than on the first draw call.
     */
    vita2d_font *probe = vita2d_load_font_file(filename);

    if (!probe)
    {
        JS_FreeCString(ctx, filename);
        return JS_ThrowInternalError(ctx, "Unable to load font file");
    }

    vita2d_free_font(probe);

    JSValue obj = JS_NewObjectClass(ctx, js_font_class_id);

    if (JS_IsException(obj))
    {
        JS_FreeCString(ctx, filename);
        return obj;
    }

    FontData *data = js_mallocz(ctx, sizeof(*data));

    if (!data)
    {
        JS_FreeCString(ctx, filename);
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }

    size_t filename_len = strlen(filename);

    data->filename = js_malloc(ctx, filename_len + 1);

    if (!data->filename)
    {
        JS_FreeCString(ctx, filename);
        js_free(ctx, data);
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }

    memcpy(data->filename, filename, filename_len + 1);
    data->sizes = NULL;

    JS_FreeCString(ctx, filename);

    JS_SetOpaque(obj, data);

    return obj;
}

static JSValue vitajs_free_font(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(
            ctx,
            "free_font(font) expects exactly one argument");

    FontData *data = JS_GetOpaque2(ctx, argv[0], js_font_class_id);

    if (!data)
        return JS_EXCEPTION;

    if (data->filename)
    {
        JSRuntime *rt = JS_GetRuntime(ctx);
        vitajs_free_font_cache(rt, data);
    }

    /*
     * Do NOT JS_FreeValue(argv[0]) here.
     * argv values are borrowed from QuickJS.
     * The object remains valid but represents an explicitly freed font.
     */
    return JS_UNDEFINED;
}

static JSValue vitajs_font_draw_text(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 6)
        return JS_ThrowSyntaxError(
            ctx,
            "font_draw_text(font, x, y, color, size, text) expects six arguments");

    FontData *data = vitajs_get_font_data(ctx, argv[0]);

    if (!data)
        return JS_EXCEPTION;

    int32_t x;
    int32_t y;
    uint32_t color;
    uint32_t size;

    if (JS_ToInt32(ctx, &x, argv[1]))
        return JS_EXCEPTION;

    if (JS_ToInt32(ctx, &y, argv[2]))
        return JS_EXCEPTION;

    if (JS_ToUint32(ctx, &color, argv[3]))
        return JS_EXCEPTION;

    if (JS_ToUint32(ctx, &size, argv[4]))
        return JS_EXCEPTION;

    vita2d_font *font = vitajs_get_sized_font(ctx, data, size);

    if (!font)
        return JS_EXCEPTION;

    const char *text = JS_ToCString(ctx, argv[5]);

    if (!text)
        return JS_EXCEPTION;

    vita2d_font_draw_text(
        font,
        x,
        y,
        color,
        size,
        text);

    JS_FreeCString(ctx, text);

    return JS_UNDEFINED;
}

static JSValue vitajs_font_draw_text_ls(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 7)
        return JS_ThrowSyntaxError(
            ctx,
            "font_draw_text_ls(font, x, y, lineSpace, color, size, text) expects seven arguments");

    FontData *data = vitajs_get_font_data(ctx, argv[0]);

    if (!data)
        return JS_EXCEPTION;

    int32_t x;
    int32_t y;
    float line_space;
    uint32_t color;
    uint32_t size;

    if (JS_ToInt32(ctx, &x, argv[1]))
        return JS_EXCEPTION;

    if (JS_ToInt32(ctx, &y, argv[2]))
        return JS_EXCEPTION;

    if (JS_ToFloat32(ctx, &line_space, argv[3]))
        return JS_EXCEPTION;

    if (JS_ToUint32(ctx, &color, argv[4]))
        return JS_EXCEPTION;

    if (JS_ToUint32(ctx, &size, argv[5]))
        return JS_EXCEPTION;

    vita2d_font *font = vitajs_get_sized_font(ctx, data, size);

    if (!font)
        return JS_EXCEPTION;

    const char *text = JS_ToCString(ctx, argv[6]);

    if (!text)
        return JS_EXCEPTION;

    vita2d_font_draw_text_ls(
        font,
        x,
        y,
        line_space,
        color,
        size,
        text);

    JS_FreeCString(ctx, text);

    return JS_UNDEFINED;
}

static JSValue vitajs_font_text_dimensions(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 3)
        return JS_ThrowSyntaxError(
            ctx,
            "font_text_dimensions(font, size, text) expects three arguments");

    FontData *data = vitajs_get_font_data(ctx, argv[0]);

    if (!data)
        return JS_EXCEPTION;

    uint32_t size;

    if (JS_ToUint32(ctx, &size, argv[1]))
        return JS_EXCEPTION;

    vita2d_font *font = vitajs_get_sized_font(ctx, data, size);

    if (!font)
        return JS_EXCEPTION;

    const char *text = JS_ToCString(ctx, argv[2]);

    if (!text)
        return JS_EXCEPTION;

    int width = 0;
    int height = 0;

    vita2d_font_text_dimensions(
        font,
        size,
        text,
        &width,
        &height);

    JS_FreeCString(ctx, text);

    JSValue result = JS_NewObject(ctx);

    if (JS_IsException(result))
        return result;

    if (JS_SetPropertyStr(
            ctx,
            result,
            "width",
            JS_NewInt32(ctx, width)) < 0)
    {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }

    if (JS_SetPropertyStr(
            ctx,
            result,
            "height",
            JS_NewInt32(ctx, height)) < 0)
    {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }

    return result;
}

static JSValue vitajs_font_text_width(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 3)
        return JS_ThrowSyntaxError(
            ctx,
            "font_text_width(font, size, text) expects three arguments");

    FontData *data = vitajs_get_font_data(ctx, argv[0]);

    if (!data)
        return JS_EXCEPTION;

    uint32_t size;

    if (JS_ToUint32(ctx, &size, argv[1]))
        return JS_EXCEPTION;

    vita2d_font *font = vitajs_get_sized_font(ctx, data, size);

    if (!font)
        return JS_EXCEPTION;

    const char *text = JS_ToCString(ctx, argv[2]);

    if (!text)
        return JS_EXCEPTION;

    int width = vita2d_font_text_width(
        font,
        size,
        text);

    JS_FreeCString(ctx, text);

    return JS_NewInt32(ctx, width);
}

static JSValue vitajs_font_text_height(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 3)
        return JS_ThrowSyntaxError(
            ctx,
            "font_text_height(font, size, text) expects three arguments");

    FontData *data = vitajs_get_font_data(ctx, argv[0]);

    if (!data)
        return JS_EXCEPTION;

    uint32_t size;

    if (JS_ToUint32(ctx, &size, argv[1]))
        return JS_EXCEPTION;

    vita2d_font *font = vitajs_get_sized_font(ctx, data, size);

    if (!font)
        return JS_EXCEPTION;

    const char *text = JS_ToCString(ctx, argv[2]);

    if (!text)
        return JS_EXCEPTION;

    int height = vita2d_font_text_height(
        font,
        size,
        text);

    JS_FreeCString(ctx, text);

    return JS_NewInt32(ctx, height);
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("load_font_file", 1, vitajs_load_font_file),
    JS_CFUNC_DEF("free_font", 1, vitajs_free_font),
    JS_CFUNC_DEF("font_draw_text", 6, vitajs_font_draw_text),
    JS_CFUNC_DEF("font_draw_text_ls", 7, vitajs_font_draw_text_ls),
    JS_CFUNC_DEF("font_text_dimensions", 3, vitajs_font_text_dimensions),
    JS_CFUNC_DEF("font_text_width", 3, vitajs_font_text_width),
    JS_CFUNC_DEF("font_text_height", 3, vitajs_font_text_height),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    if (js_font_class_id == 0)
        JS_NewClassID(&js_font_class_id);

    /*
     * Class IDs are global, class registrations are per JSRuntime.
     * This also makes the module safe if a second context is created
     * in the same runtime.
     */
    if (!JS_IsRegisteredClass(rt, js_font_class_id))
    {
        if (JS_NewClass(rt, js_font_class_id, &js_font_class) < 0)
            return -1;
    }

    JSValue proto = JS_NewObject(ctx);

    if (JS_IsException(proto))
        return -1;

    JS_SetClassProto(ctx, js_font_class_id, proto);

    return JS_SetModuleExportList(
        ctx,
        m,
        module_funcs,
        countof(module_funcs));
}

JSModuleDef *vitajs_font_init(JSContext *ctx)
{
    return vitajs_push_module(
        ctx,
        module_init,
        module_funcs,
        countof(module_funcs),
        "Font");
}
