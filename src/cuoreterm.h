#ifndef CUORETERM_H
#define CUORETERM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct terminal {
    void *fb_addr;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t fb_bpp;

    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t cols;
    uint32_t rows;

    const uint8_t *font_data; // pointer to PSF font
    uint32_t font_width;
    uint32_t font_height;
};

void cuoreterm_init(
    struct terminal *term,
    void *fb_addr,
    uint32_t fb_width,
    uint32_t fb_height,
    uint32_t fb_pitch,
    uint32_t fb_bpp,
    const uint8_t *font,
    uint32_t font_w,
    uint32_t font_h
);

void cuoreterm_write(void *ctx, const char *msg, uint64_t len);
void cuoreterm_draw_char(
    struct terminal *term,
    char c,
    uint32_t fg,
    uint32_t bg
);

void cuoreterm_set_font(
    struct terminal *term,
    const uint8_t *font,
    uint32_t font_w,
    uint32_t font_h
);

void cuoreterm_clear(struct terminal *term);

#ifdef __cplusplus
}
#endif

#endif // CUORETERM_H