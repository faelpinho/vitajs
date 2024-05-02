#include <stdbool.h>
#include <psp2common/ctrl.h>

#include "../quickjs/quickjs.h"
#include "../quickjs/quickjs-libc.h"
#include "../quickjs/cutils.h"

static SceCtrlData ctrl;

unsigned int isButtonPressed();

static JSContext *JS_NewCustomContext(JSRuntime *rt);

JSModuleDef *athena_push_module(JSContext *ctx, JSModuleInitFunc *func, const JSCFunctionListEntry *func_list, int len, const char *module_name);
JSModuleDef *athena_system_init(JSContext *ctx);
JSModuleDef *athena_render_init(JSContext *ctx);
JSModuleDef *athena_screen_init(JSContext *ctx);

const char *runScript(const char *script);

static int qjs_handle_file(JSContext *ctx, const char *filename, const char *bytecode_filename);
static int qjs_handle_fh(JSContext *ctx, FILE *f, const char *filename, const char *bytecode_filename);
static int qjs_eval_buf(JSContext *ctx, const void *buf, int buf_len, const char *filename, int eval_flags);

unsigned int get_used_memory();

void delay(int timer);
void delayMiliseconds(int timer);