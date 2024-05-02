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
#include "ath_env.h"
#include "graphics.h"

int main(int argc, char *argv[])
{
    init_graphics();
    printf("graphics initialized.");

    loadFont("app0:/assets/segoeui.ttf");
    printf("font loaded.");

    const char *result = NULL;

    const char *list[8] = {
        "EvalError",
        "SyntaxError",
        "TypeError",
        "ReferenceError",
        "RangeError",
        "InternalError",
        "URIError",
        "AggregateError"};

    int index = 0;

    // sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    uint32_t lastButtonState = 0;
    uint32_t currentButtonState = 0;

    uint32_t lastTime = sceKernelGetProcessTimeLow();
    uint32_t currentTime = lastTime;
    uint32_t deltaTime = 0;

    printf("preparing to enter in loop...");

    while (ctrl.buttons != SCE_CTRL_START)
    {
        sceCtrlPeekBufferPositive(0, &ctrl, 1);

        currentButtonState = ctrl.buttons;
        if (currentButtonState & SCE_CTRL_START && !(lastButtonState & SCE_CTRL_START))
        {
            break;
        }
        lastButtonState = currentButtonState;

        currentTime = sceKernelGetProcessTimeLow();
        deltaTime = currentTime - lastTime;

        if (deltaTime >= (5 * 1000 * 1000))
        {
            index = (index + 1) % 8;
            lastTime = currentTime;
        }

        start_drawing_and_clear();

        athena_error_screen(list[index]);

        end_drawing();
        waitVblankStart();
    }

    freeFont();
    end_graphics();

    return 0;
}
