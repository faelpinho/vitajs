#include "script.h"
#include <psp2/kernel/sysmem.h>
#include <psp2/appmgr.h>

#define TRUE 1

// get_used_memory
unsigned int get_used_memory()
{
    SceAppMgrBudgetInfo info;
    info.size = sizeof(SceAppMgrBudgetInfo);
    sceAppMgrGetBudgetInfo(&info);
    return info.total_user_rw_mem - info.free_user_rw;
}

// GetMemorySize ( = 536870912?)
unsigned int GetMemorySize()
{
    SceAppMgrBudgetInfo info;
    info.size = sizeof(SceAppMgrBudgetInfo);
    sceAppMgrGetBudgetInfo(&info);
    return info.total_user_rw_mem;
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

    /*
    athena_system_init(ctx);
    athena_archive_init(ctx);
    athena_timer_init(ctx);
    athena_task_init(ctx);
    athena_pads_init(ctx);
    */

#ifdef ATHENA_AUDIO
    athena_sound_init(ctx);
#endif

#ifdef ATHENA_GRAPHICS
    athena_color_init(ctx);
    athena_image_init(ctx);
    athena_imagelist_init(ctx);
    athena_font_init(ctx);
    athena_shape_init(ctx);
    athena_screen_init(ctx);
    athena_render_init(ctx);
#else
#ifdef ATHENA_CLI
    athena_console_init(ctx);
#endif
#endif

#ifdef ATHENA_KEYBOARD
    athena_keyboard_init(ctx);
#endif

#ifdef ATHENA_MOUSE
    athena_mouse_init(ctx);
#endif

#ifdef ATHENA_CAMERA
    athena_camera_init(ctx);
#endif

#ifdef ATHENA_NETWORK
    athena_network_init(ctx);
    athena_request_init(ctx);
    athena_socket_init(ctx);
    athena_ws_init(ctx);
#endif

    return ctx;
}

static char error_buf[4096];

const char *runScript(const char *script, bool isBuffer)
{
    // size_t memoryLimit = (GetMemorySize() - get_used_memory()) >> 1;
    // size_t memoryLimit = 1024;

    printf("\nStarting AthenaEnv...\n");
    JSRuntime *rt = JS_NewRuntime();
    if (!rt)
    {
        return "Runtime creation";
    }
    js_std_set_worker_new_context_func(JS_NewCustomContext);
    js_std_init_handlers(rt);

    // JS_SetMemoryLimit(rt, memoryLimit);
    // JS_SetGCThreshold(rt, memoryLimit >> 3);

    JSContext *ctx = JS_NewCustomContext(rt);
    if (!ctx)
    {
        return "Context creation";
    }

    JS_SetModuleLoaderFunc(rt, NULL, js_module_loader, NULL);

    int s = qjs_handle_file(ctx, script, NULL);

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

static int qjs_handle_file(JSContext *ctx, const char *filename, const char *bytecode_filename)
{
    FILE *f = NULL;
    int retval;
    char fnbuf[256];

    snprintf(fnbuf, sizeof(fnbuf), "%s", filename);

    fnbuf[sizeof(fnbuf) - 1] = (char)0;

    f = fopen(fnbuf, "r");
    if (!f)
    {
        fprintf(stderr, "failed to open source file: %s\n", filename);
        fflush(stderr);
        return -1;
    }

    retval = qjs_handle_fh(ctx, f, filename, bytecode_filename);

    fclose(f);
    return retval;
}

static int qjs_handle_fh(JSContext *ctx, FILE *f, const char *filename, const char *bytecode_filename)
{
    char *buf = NULL;
    size_t bufsz;
    size_t bufoff;
    size_t got;
    int rc;
    int retval = -1;

    buf = (char *)malloc(1024);
    if (!buf)
    {
        if (buf)
        {
            free(buf);
            buf = NULL;
        }
        return retval;
    }
    bufsz = 1024;
    bufoff = 0;

    /* Read until EOF, avoid fseek/stat because it won't work with stdin. */
    for (;;)
    {
        size_t avail;

        avail = bufsz - bufoff;
        if (avail < 1024)
        {
            size_t newsz;
            char *buf_new;

            newsz = bufsz + (bufsz >> 2) + 1024; /* +25% and some extra */
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

        got = fread((void *)(buf + bufoff), (size_t)1, avail, f);

        if (got == 0)
        {
            break;
        }
        bufoff += got;
    }

    buf[bufoff++] = 0;
    js_std_add_helpers(ctx, 0, NULL);
    { // make 'std' and 'os' visible to non module code
        const char *str =
#ifdef ATHENA_AUDIO
            "import * as Sound from 'Sound';\n"
            "globalThis.Sound = Sound;\n"
#endif

#ifdef ATHENA_CAMERA
            "import * as Camera from 'Camera';\n"
            "globalThis.Camera = Camera;\n"
#endif

#ifdef ATHENA_KEYBOARD
            "import * as Keyboard from 'Keyboard';\n"
            "globalThis.Keyboard = Keyboard;\n"
#endif

#ifdef ATHENA_MOUSE
            "import * as Mouse from 'Mouse';\n"
            "globalThis.Mouse = Mouse;\n"
#endif

#ifdef ATHENA_NETWORK
            "import * as Network from 'Network';\n"
            "import * as Request from 'Request';\n"
            "import * as Socket from 'Socket';\n"
            "import * as SocketConst from 'SocketConst';\n"
            "import * as WebSocket from 'WebSocket';\n"
            "globalThis.Network = Network;\n"
            "globalThis.AF_INET = SocketConst.AF_INET;\n"
            "globalThis.SOCK_STREAM = SocketConst.SOCK_STREAM;\n"
            "globalThis.SOCK_DGRAM = SocketConst.SOCK_DGRAM;\n"
            "globalThis.SOCK_RAW = SocketConst.SOCK_RAW;\n"
            "globalThis.Socket = Socket.Socket;\n"
            "globalThis.Request = Request.Request;\n"
            "globalThis.WebSocket = WebSocket.WebSocket;\n"
#endif

#ifdef ATHENA_GRAPHICS
            "import * as Color from 'Color';\n"
            "import * as Screen from 'Screen';\n"
            "import * as Draw from 'Draw';\n"
            "import * as Font from 'Font';\n"
            "import * as Image from 'Image';\n"
            "import * as ImageList from 'ImageList';\n"
            "import * as Render from 'Render';\n"
            "import * as Lights from 'Lights';\n"
            "import * as Camera from 'Camera';\n"
            "globalThis.Color = Color;\n"
            "globalThis.Screen = Screen;\n"
            "globalThis.NTSC = Screen.NTSC;\n"
            "globalThis.DTV_480p = Screen.DTV_480p;\n"
            "globalThis.PAL = Screen.PAL;\n"
            "globalThis.DTV_576p = Screen.DTV_576p;\n"
            "globalThis.DTV_720p = Screen.DTV_720p;\n"
            "globalThis.DTV_1080i = Screen.DTV_1080i;\n"
            "globalThis.INTERLACED = Screen.INTERLACED;\n"
            "globalThis.PROGRESSIVE = Screen.PROGRESSIVE;\n"
            "globalThis.FIELD = Screen.FIELD;\n"
            "globalThis.FRAME = Screen.FRAME;\n"
            "globalThis.CT16 = Screen.CT16;\n"
            "globalThis.CT16S = Screen.CT16S;\n"
            "globalThis.CT24 = Screen.CT24;\n"
            "globalThis.CT32 = Screen.CT32;\n"
            "globalThis.Z16 = Screen.Z16;\n"
            "globalThis.Z16S = Screen.Z16S;\n"
            "globalThis.Z24 = Screen.Z24;\n"
            "globalThis.Z32 = Screen.Z32;\n"
            "globalThis.Draw = Draw;\n"
            "globalThis.Font = Font.Font;\n"

            "globalThis.NEAREST = 0;\n"
            "globalThis.LINEAR = 1;\n"
            "globalThis.VRAM = false;\n"
            "globalThis.RAM = true;\n"
            "globalThis.Image = Image.Image;\n"
            "globalThis.ImageList = ImageList.ImageList;\n"

            "globalThis.Render = Render;\n"

            "globalThis.Lights = Lights;\n"
            "globalThis.AMBIENT = Lights.AMBIENT;\n"
            "globalThis.DIRECTIONAL = Lights.DIRECTIONAL;\n"

            "globalThis.Camera = Camera;\n"

#else

#ifdef ATHENA_CLI
            "import * as Console from 'Console';\n"
            "globalThis.Console = Console;\n"
#endif
#endif
            /*
                "import * as std from 'std';\n"
                "import * as os from 'os';\n"
                "import * as Timer from 'Timer';\n"
                "import * as Tasks from 'Tasks';\n"
                "import * as Pads from 'Pads';\n"
                "import * as System from 'System';\n"
                "import * as IOP from 'IOP';\n"
                "import * as Archive from 'Archive';\n"

                "globalThis.std = std;\n"
                "globalThis.os = os;\n"
                "globalThis.Timer = Timer;\n"
                "globalThis.Tasks = Tasks;\n"
                "globalThis.Pads = Pads;\n"
                "globalThis.System = System;\n"
                "globalThis.Archive = Archive;\n"
                "globalThis.IOP = IOP;\n";
            */

            rc = qjs_eval_buf(ctx, str, strlen(str), "<input>", JS_EVAL_TYPE_MODULE);
        if (rc != 0)
        {
            return retval;
        }
    }

    rc = qjs_eval_buf(ctx, (void *)buf, bufoff - 1, filename, JS_EVAL_TYPE_MODULE);
    free(buf);
    buf = NULL;
    if (rc != 0)
    {
        return retval;
    }
    else
    {
        return 0;
    }
    return retval;
}

static int qjs_eval_buf(JSContext *ctx, const void *buf, int buf_len, const char *filename, int eval_flags)
{
    JSValue val;
    int ret;

    if ((eval_flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE)
    {
        /* for the modules, we compile then run to be able to set
           import.meta */
        val = JS_Eval(ctx, buf, buf_len, filename,
                      eval_flags | JS_EVAL_FLAG_COMPILE_ONLY);
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