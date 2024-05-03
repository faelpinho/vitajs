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
    init_graphics();
    printf("graphics initialized.");

    loadFont("app0:/assets/segoeui.ttf");
    printf("font loaded.");

    const char *errMsg = NULL;

    uint32_t lastButtonState = 0;
    uint32_t currentButtonState = 0;

    uint32_t lastTime = sceKernelGetProcessTimeLow();
    uint32_t currentTime = lastTime;
    uint32_t deltaTime = 0;

    printf("VitaJS starting...");

    do
    {
        errMsg = runScript("app0:/assets/main.js");
    } while (errMsg != NULL);

    start_drawing();

    vitajs_error_screen(errMsg);

    end_drawing();

    printf("while start is not pressed...");

    while (ctrl.buttons != SCE_CTRL_START)
    {
        return -1;
    }

    freeFont();
    end_graphics();

    return 0;
}
