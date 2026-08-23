#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>
#include <vita2d.h>
#include <vitasdk.h>
#include <stdbool.h>

#include "common/debugScreen.h"
#include "env.h"
#include "graphics.h"

/*
 * CommonDialog/AppUtil must be initialized before vita2d/GXM on real hardware.
 * The implementations live in src/modules/dialog.c.
 */
int vitajs_dialog_system_init(void);
void vitajs_dialog_system_shutdown(void);

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    int dialog_result = vitajs_dialog_system_init();
    if (dialog_result < 0)
    {
        printf("CommonDialog/AppUtil init failed: 0x%08X\n", (unsigned int)dialog_result);
        return dialog_result;
    }

    int graphics_result = vita2d_init();
    if (graphics_result < 0)
    {
        printf("vita2d_init failed: 0x%08X\n", (unsigned int)graphics_result);
        vitajs_dialog_system_shutdown();
        return graphics_result;
    }

    printf("graphics initialized.\n");

    const char *errMsg = NULL;

    printf("Starting VitaJS...\n");

    do
    {
        errMsg = runScript("app0:/assets/main.js");
    } while (errMsg != NULL);

    vitajs_error_screen(errMsg);

    do
    {
        sceCtrlPeekBufferPositive(0, &ctrl, 1);
    } while (ctrl.buttons != SCE_CTRL_START);

    vita2d_fini();
    vitajs_dialog_system_shutdown();

    return 0;
}
