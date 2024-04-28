#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>
#include <vita2d.h>
#include <vitasdk.h>
#include "common/debugScreen.h"
#include "../duktape/src/duktape.h"

// Para exibir no display do console/emu e no stdout do vita3k.
#define print(...)                         \
    do                                     \
    {                                      \
        psvDebugScreenPrintf(__VA_ARGS__); \
        printf(__VA_ARGS__);               \
    } while (0)

duk_ret_t duk_print(duk_context *ctx);
void draw_text(vita2d_font *fonte, int x, int y, unsigned int color, int size, const char *text);
void delay(int timer);
void delayMiliseconds(int timer);

int main(int argc, char *argv[])
{
    psvDebugScreenInit();
    print("INICIADO.\n");

    delay(1);

    vita2d_init();
    vita2d_set_clear_color(RGBA8(0, 0, 0, 255));

    const char *fontPath = "app0:/assets/segoeui.ttf";
    vita2d_font *fonte = vita2d_load_font_file(fontPath);
    if (!fonte)
    {
        vita2d_fini();
        psvDebugScreenInit();
        print("Falha ao carregar fonte: %s.\nEncerrando...", fontPath);
        delay(5);
        return -1;
    }

    duk_context *ctx = duk_create_heap_default();
    if (!ctx)
    {
        duk_destroy_heap(ctx);
        vita2d_fini();
        psvDebugScreenInit();
        print("Erro ao criar context duktape.\nEncerrando...");
        return -1;
    }

    // Registra função 'print' no duktape
    duk_push_c_function(ctx, duk_print, DUK_VARARGS);
    duk_put_global_string(ctx, "print");

    // Executa script
    const char *js_code = "var x = 10; var y = 'Hello world menó! x =' + x; print(y); 'Hello world menó! Quanto é x?'";

    if (duk_peval_string(ctx, js_code) != 0)
    {
        duk_destroy_heap(ctx);
        vita2d_fini();
        psvDebugScreenInit();
        print("Erro: %s\nEncerrando...", duk_safe_to_string(ctx, -1));
        delay(5);
        return -1;
    }

    // Retorna 'teste1' em result.
    const char *result = duk_to_string(ctx, -1);

    SceCtrlData ctrl;

    vita2d_start_drawing();
    vita2d_clear_screen();

    draw_text(fonte, 50, 50, RGBA8(0, 200, 200, 255), 20, result);

    vita2d_end_drawing();
    vita2d_common_dialog_update();
    vita2d_swap_buffers();
    sceDisplayWaitVblankStart();

    do
    {
        sceCtrlPeekBufferPositive(0, &ctrl, 1);
    } while (ctrl.buttons != SCE_CTRL_START);

    duk_destroy_heap(ctx);
    vita2d_fini();

    return 0;
}

duk_ret_t duk_print(duk_context *ctx)
{
    const char *text = duk_to_string(ctx, -1);
    printf("%s\nTESTE", text);
    return 0;
}

void draw_text(vita2d_font *fonte, int x, int y, unsigned int color, int size, const char *text)
{
    vita2d_font_draw_text(fonte, x, y, color, size, text);
}

void delay(int timer)
{
    sceKernelDelayThread(timer * 1000 * 1000);
}

void delayMiliseconds(int timer)
{
    sceKernelDelayThread(timer * 1000);
}
