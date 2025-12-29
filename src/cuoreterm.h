#ifndef GRAPHICS_H
#define GRAPHICS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void cuoreterm_init(struct terminal *term, struct framebuffer *fb);

void cuoreterm_write(void *ctx, const char *msg, uint32_t len);
void cuoreterm_draw_char(;
    struct terminal *term,
    char c,
    uint32_t fg,
    uint32_t bg
);

#ifdef __cplusplus
}
#endif

#endif // GRAPHICS_H