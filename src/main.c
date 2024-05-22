#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <psp2/ctrl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>
#include <vita2d.h>
#include <vitasdk.h>
#include <stdbool.h>
#include "common/debugScreen.h"
#include "env.h"
#include "graphics.h"

int main(int argc, char *argv[])
{
    vita2d_init();
    printf("graphics initialized.");

    const char *errMsg = NULL;

    uint32_t lastButtonState = 0;
    uint32_t currentButtonState = 0;

    uint32_t lastTime = sceKernelGetProcessTimeLow();
    uint32_t currentTime = lastTime;
    uint32_t deltaTime = 0;

    printf("Starting VitaJS...\n");

    do
    {
        errMsg = runScript("app0:/assets/main.js");

        if (strstr(errMsg, "System.exitVitaJS") != NULL)
            break;

        vitajs_error_screen(errMsg);

    } while (errMsg != NULL);

    while (ctrl.buttons != SCE_CTRL_START)
    {
        sceCtrlPeekBufferPositive(0, &ctrl, 1);
        printf('loop\n');
    }

    // freeFont();
    vita2d_fini();

    return 0;
}
