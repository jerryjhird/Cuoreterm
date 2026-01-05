#include <stdint.h> // uint32_t, uint8_t
#include "cuoreterm.h"
#include "kfont.h"

#define FONT_W 8
#define FONT_H 14

// memory helpers

static void *h_memset(void *dst, uint8_t v, uint32_t n) {
    uint8_t *p = (uint8_t *)dst;
    while (n--)
        *p++ = v;
    return dst;
}

static void *h_memmove(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dst;
}

// fb/glyph code

static void fb_put_pixel(
    struct terminal *term,
    uint32_t x,
    uint32_t y,
    uint32_t color
) {
    if (x >= term->fb_width || y >= term->fb_height)
        return;

    uint32_t bpp = term->fb_bpp / 8;
    uint8_t *p =
        (uint8_t *)term->fb_addr +
        y * term->fb_pitch +
        x * bpp;

    *(uint32_t *)p = color;
}

static void draw_glyph(
    struct terminal *term,
    uint32_t x,
    uint32_t y,
    const uint8_t *glyph,
    uint32_t rows,
    uint32_t fg,
    uint32_t bg
) {
    for (uint32_t r = 0; r < rows; r++) {
        uint8_t bits = glyph[r];
        for (uint32_t c = 0; c < 8; c++) {
            uint32_t color =
                (bits & (1 << (7 - c))) ? fg : bg;
            fb_put_pixel(term, x + c, y + r, color);
        }
    }
}

// main logic :3
// cuoreterm_init
// cuoreterm_draw_char
// cuoreterm_write

void cuoreterm_init(struct terminal *term,
                    void *fb_addr,
                    uint32_t fb_width,
                    uint32_t fb_height,
                    uint32_t fb_pitch,
                    uint32_t fb_bpp)
{
    term->fb_addr      = fb_addr;
    term->fb_width  = fb_width;
    term->fb_height = fb_height;
    term->fb_pitch  = fb_pitch;
    term->fb_bpp    = fb_bpp;

    term->cursor_x = 0;
    term->cursor_y = 0;
    term->cols = fb_width  / FONT_W;
    term->rows = fb_height / FONT_H;
}

static void term_scroll(struct terminal *term) {
    uint32_t row_bytes = term->fb_pitch * FONT_H;
    uint32_t total     = term->fb_pitch * term->fb_height;

    h_memmove(
        term->fb_addr,
        (uint8_t *)term->fb_addr + row_bytes,
        total - row_bytes
    );

    h_memset(
        (uint8_t *)term->fb_addr + total - row_bytes,
        0,
        row_bytes
    );

    if (term->cursor_y > 0)
        term->cursor_y--;
}

void cuoreterm_draw_char(
    struct terminal *term,
    char c,
    uint32_t fg,
    uint32_t bg
) {
    if (c == '\n') {
        term->cursor_x = 0;
        term->cursor_y++;
        if (term->cursor_y >= term->rows)
            term_scroll(term);
        return;
    }

    uint32_t px = term->cursor_x * FONT_W;
    uint32_t py = term->cursor_y * FONT_H;

    const uint8_t *glyph =
        iso10_f14_psf + 4 + ((uint8_t)c * FONT_H);

    draw_glyph(term, px, py, glyph, FONT_H, fg, bg);

    term->cursor_x++;
    if (term->cursor_x >= term->cols) {
        term->cursor_x = 0;
        term->cursor_y++;
        if (term->cursor_y >= term->rows)
            term_scroll(term);
    }
}

void cuoreterm_write(void *ctx, const char *msg, uint64_t len) {
    struct terminal *term = (struct terminal *)ctx;

    for (uint64_t i = 0; i < len; i++) {
        char c = msg[i];

        if (c == '\n') {
            term->cursor_x = 0;
            term->cursor_y++;
            if (term->cursor_y >= term->rows)
                term_scroll(term);
        }
        else if (c == '\b') {
            if (term->cursor_x == 0) {
                if (term->cursor_y == 0)
                    continue;
                term->cursor_y--;
                term->cursor_x = term->cols - 1;
            } else {
                term->cursor_x--;
            }

            uint32_t px = term->cursor_x * FONT_W;
            uint32_t py = term->cursor_y * FONT_H;

            const uint8_t *glyph =
                iso10_f14_psf + 4 + (' ' * FONT_H);

            draw_glyph(
                term,
                px,
                py,
                glyph,
                FONT_H,
                0x00FFFFFF,
                0x00000000
            );
        }
        else {
            cuoreterm_draw_char(
                term,
                c,
                0x00FFFFFF,
                0x00000000
            );
        }
    }
}
