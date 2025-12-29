#include <stdint.h> // uint32_t, uint8_t
#include "kfont.h"

#define FONT_W 8
#define FONT_H 14

struct framebuffer {
    void    *addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
};

struct terminal {
    struct framebuffer *fb;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t cols;
    uint32_t rows;
};

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
    struct framebuffer *fb,
    uint32_t x,
    uint32_t y,
    uint32_t color
) {
    if (x >= fb->width || y >= fb->height)
        return;

    uint32_t bpp = fb->bpp / 8;
    uint8_t *p =
        (uint8_t *)fb->addr +
        y * fb->pitch +
        x * bpp;

    *(uint32_t *)p = color;
}

static void draw_glyph(
    struct framebuffer *fb,
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
            fb_put_pixel(fb, x + c, y + r, color);
        }
    }
}

// main logic :3
// cuoreterm_init
// cuoreterm_draw_char
// cuoreterm_write

void cuoreterm_init(struct terminal *term, struct framebuffer *fb) {
    term->fb = fb;
    term->cursor_x = 0;
    term->cursor_y = 0;
    term->cols = fb->width  / FONT_W;
    term->rows = fb->height / FONT_H;
}

static void term_scroll(struct terminal *term) {
    struct framebuffer *fb = term->fb;

    uint32_t row_bytes = fb->pitch * FONT_H;
    uint32_t total = fb->pitch * fb->height;

    h_memmove(
        fb->addr,
        (uint8_t *)fb->addr + row_bytes,
        total - row_bytes
    );

    h_memset(
        (uint8_t *)fb->addr + total - row_bytes,
        0,
        row_bytes
    );

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

    draw_glyph(term->fb, px, py, glyph, FONT_H, fg, bg);

    term->cursor_x++;
    if (term->cursor_x >= term->cols) {
        term->cursor_x = 0;
        term->cursor_y++;
        if (term->cursor_y >= term->rows)
            term_scroll(term);
    }
}

void cuoreterm_write(void *ctx, const char *msg, uint32_t len) {
    struct terminal *term = (struct terminal *)ctx;

    for (uint32_t i = 0; i < len; i++) {
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
                term->fb,
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
