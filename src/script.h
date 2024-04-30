#include <stdbool.h>
#include "../quickjs/quickjs.h"
#include "../quickjs/quickjs-libc.h"

static JSContext *JS_NewCustomContext(JSRuntime *rt);

const char *runScript(const char *script, bool isBuffer);

static int qjs_handle_file(JSContext *ctx, const char *filename, const char *bytecode_filename);

static int qjs_handle_fh(JSContext *ctx, FILE *f, const char *filename, const char *bytecode_filename);

static int qjs_eval_buf(JSContext *ctx, const void *buf, int buf_len, const char *filename, int eval_flags);

unsigned int get_used_memory();