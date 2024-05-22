#include <psp2/kernel/sysmem.h>
#include <psp2/appmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <string.h>

#include "env.h"

#define TRUE 1

// get_used_memory
unsigned int get_used_memory()
{
    SceAppMgrBudgetInfo info;
    info.size = sizeof(SceAppMgrBudgetInfo);
    sceAppMgrGetBudgetInfo(&info);
    return info.total_user_rw_mem - info.free_user_rw;
}

// get_free_memory ( = 536870912?)
unsigned int get_free_memory()
{
    SceAppMgrBudgetInfo info;
    info.size = sizeof(SceAppMgrBudgetInfo);
    sceAppMgrGetBudgetInfo(&info);
    return info.total_user_rw_mem;
}

// get_used_vram
unsigned int get_used_vram()
{
    SceAppMgrBudgetInfo info;
    info.size = sizeof(SceAppMgrBudgetInfo);
    sceAppMgrGetBudgetInfo(&info);
    return info.total_cdram_mem - info.free_cdram_mem;
}

// get_free_vram ( = 134217728?)
unsigned int get_free_vram()
{
    SceAppMgrBudgetInfo info;
    info.size = sizeof(SceAppMgrBudgetInfo);
    sceAppMgrGetBudgetInfo(&info);
    return info.total_cdram_mem;
}

static JSContext *JS_NewCustomContext(JSRuntime *rt)
{
    JSContext *ctx;
    ctx = JS_NewContext(rt);
    if (!ctx)
        return NULL;

    /* system modules */
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");

    vitajs_system_init(ctx);
    vitajs_screen_init(ctx);
    vitajs_pads_init(ctx);

    /*
    vitajs_archive_init(ctx);
    vitajs_timer_init(ctx);
    vitajs_task_init(ctx);
    vitajs_sound_init(ctx);
    vitajs_font_init(ctx);
    vitajs_camera_init(ctx);
    vitajs_network_init(ctx);
    */

    return ctx;
}

static char error_buf[4096];

unsigned int isButtonPressed()
{
    return ctrl.buttons;
}

JSModuleDef *vitajs_push_module(JSContext *ctx, JSModuleInitFunc *func, const JSCFunctionListEntry *func_list, int len, const char *module_name)
{
    JSModuleDef *m;
    m = JS_NewCModule(ctx, module_name, func);
    if (!m)
        return NULL;
    JS_AddModuleExportList(ctx, m, func_list, len);

    printf("VitaJS: %s module pushed at 0x%x\n", module_name, m);
    return m;
}

const char *runScript(const char *script)
{
    JSRuntime *rt = JS_NewRuntime();
    if (!rt)
        return "Runtime creation failed.";

    js_std_set_worker_new_context_func(JS_NewCustomContext);
    js_std_init_handlers(rt);

    JSContext *ctx = JS_NewCustomContext(rt);
    if (!ctx)
        return "Context creation failed.";

    JS_SetModuleLoaderFunc(rt, NULL, js_module_loader, NULL);

    int s = qjs_handle_file(ctx, script);

    js_std_loop(ctx);

    if (s < 0)
    {
        JSValue exception_val = JS_GetException(ctx);
        const char *exception = JS_ToCString(ctx, exception_val);
        JSValue stack_val = JS_GetPropertyStr(ctx, exception_val, "stack");
        const char *stack = JS_ToCString(ctx, stack_val);
        JS_FreeValue(ctx, exception_val);
        JS_FreeValue(ctx, stack_val);

        strcpy(error_buf, exception);
        strcat(error_buf, "\n");
        strcat(error_buf, stack);

        js_std_free_handlers(rt);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);

        printf("%s\n", error_buf);
        return error_buf;
    }

    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return NULL;
}

static int qjs_handle_file(JSContext *ctx, const char *filename)
{
    SceUID *f = NULL;
    int retval = -1;

    f = sceIoOpen(filename, SCE_O_RDONLY, 0777);
    if (!f)
    {
        fprintf(stderr, "failed to open source file: %s\n", filename);
        fflush(stderr);
        return retval;
    }

    retval = qjs_handle_fh(ctx, f, filename);

    sceIoClose(f);
    return retval;
}

static int qjs_handle_fh(JSContext *ctx, SceUID f, const char *filename)
{
    printf("qjs_handle_fh: iniciado\n");
    char *buf = NULL;
    size_t bufsz = 1024;
    size_t bufoff = 0;
    int rc;
    int retval = -1;

    buf = (char *)malloc(bufsz);

    if (!buf)
    {
        if (buf)
        {
            free(buf);
            buf = NULL;
        }
        return retval;
    }

    for (;;)
    {
        size_t avail = bufsz - bufoff;

        if (avail < 1024)
        {
            printf("qjs_handle_fh: if avail < 1024 = %i\n", avail);

            size_t newsz = bufsz + (bufsz >> 2) + 1024; /* +25% and some extra */
            char *buf_new;

            if (newsz < bufsz)
            {
                if (buf)
                {
                    free(buf);
                    buf = NULL;
                }
                return retval;
            }
            buf_new = (char *)realloc(buf, newsz);
            if (!buf_new)
            {
                if (buf)
                {
                    free(buf);
                    buf = NULL;
                }
                return retval;
            }
            buf = buf_new;
            bufsz = newsz;
        }

        avail = bufsz - bufoff;

        size_t got = sceIoRead(f, (void *)(buf + bufoff), avail);

        if (got == 0)
        {
            break;
        }
        bufoff += got;
    }

    buf[bufoff++] = 0;

    js_std_add_helpers(ctx, 0, NULL);

    const char *str =
        "import * as std from 'std';\n"
        "import * as os from 'os';\n"
        "import * as System from 'System';\n"
        "import * as Screen from 'Screen';\n"
        "import * as Pads from 'Pads';\n"

        "globalThis.std = std;\n"
        "globalThis.os = os;\n"
        "globalThis.System = System;\n"
        "globalThis.Screen = Screen;\n"
        "globalThis.Pads = Pads;\n";

    rc = qjs_eval_buf(ctx, str, strlen(str), "<input>", JS_EVAL_TYPE_MODULE);

    if (rc != 0)
    {
        free(buf);
        return retval;
    }

    rc = qjs_eval_buf(ctx, (void *)buf, bufoff - 1, filename, JS_EVAL_TYPE_MODULE);

    free(buf);

    if (rc != 0)
    {
        return retval;
    }
    else
    {
        return 0;
    }
}

static int qjs_eval_buf(JSContext *ctx, const void *buf, int buf_len, const char *filename, int eval_flags)
{
    JSValue val;
    int ret;

    if ((eval_flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE)
    {
        /* for the modules, we compile then run to be able to set import.meta */
        val = JS_Eval(ctx, buf, buf_len, filename, eval_flags | JS_EVAL_FLAG_COMPILE_ONLY);
        if (!JS_IsException(val))
        {
            js_module_set_import_meta(ctx, val, TRUE, TRUE);
            val = JS_EvalFunction(ctx, val);
        }
    }
    else
    {
        val = JS_Eval(ctx, buf, buf_len, filename, eval_flags);
    }
    if (JS_IsException(val))
    {
        ret = -1;
    }
    else
    {
        ret = 0;
    }
    JS_FreeValue(ctx, val);
    return ret;
}

void delay(int timer)
{
    sceKernelDelayThread(timer * 1000 * 1000);
}

void delayMiliseconds(int timer)
{
    sceKernelDelayThread(timer * 1000);
}
