#include <stdio.h>
#include <stdlib.h>
#include <vitasdk.h>
#include "../duktape/src/duktape.h"

void execute_js_script(const char *script) {
    duk_context *ctx = duk_create_heap_default();
    if (!ctx) {
        printf("Failed to create Duktape context\n");
        return;
    }

    // Evaluate the JavaScript code
    if (duk_peval_string(ctx, script) != 0) {
        printf("Error: %s\n", duk_safe_to_string(ctx, -1));
    }

    // Cleanup
    duk_destroy_heap(ctx);
}


int main(int argc, char* argv[]) {
    while (1) {
        // Execute JavaScript script
        execute_js_script("print('Hello, World from JavaScript!'); alert('teste');");
        
        // Break out of the loop after one iteration (just for testing)
        break;
    }
    
    return 0;
}
