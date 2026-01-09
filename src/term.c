#include <stdint.h>
#include <stdbool.h>
#include "cuoreterm.h"

// memory helpers

static void *h_memset(void *dst, uint8_t v, uint32_t n) {
    uint8_t *p = (uint8_t *)dst;
    while (n--) *p++ = v;
    return dst;
}

static void *h_memmove(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

// fb / glyph

static void fb_put_pixel(struct terminal *term, uint32_t x, uint32_t y, uint32_t color) {
    if (x >= term->fb_width || y >= term->fb_height) return;

    uint32_t bpp = term->fb_bpp / 8;
    uint8_t *p = (uint8_t *)term->fb_addr + y * term->fb_pitch + x * bpp;

    if (bpp == 4) {
        *(uint32_t *)p = color;
    } else if (bpp == 3) {
        p[0] = (color >> 16) & 0xFF;
        p[1] = (color >> 8) & 0xFF;
        p[2] = color & 0xFF;
    }
}

static void draw_glyph(struct terminal *term, uint32_t x, uint32_t y,
                       const uint8_t *glyph, uint32_t rows,
                       uint32_t fg) {
    for (uint32_t r = 0; r < rows; r++) {
        uint8_t bits = glyph[r];
        for (uint32_t c = 0; c < 8; c++) {
            if (bits & (1 << (7 - c))) fb_put_pixel(term, x + c, y + r, fg);
        }
    }
}


// init
void cuoreterm_init(struct terminal *term, void *fb_addr,
                     uint32_t fb_width, uint32_t fb_height,
                     uint32_t fb_pitch, uint32_t fb_bpp,
                     const uint8_t *font, uint32_t font_w, uint32_t font_h) {
    term->fb_addr   = fb_addr;
    term->fb_width  = fb_width;
    term->fb_height = fb_height;
    term->fb_pitch  = fb_pitch;
    term->fb_bpp    = fb_bpp;

    term->fgcol = 0xFFFFFFFF;

    term->cursor_x = 0;
    term->cursor_y = 0;
    term->font_data   = font;
    term->font_width  = font_w;
    term->font_height = font_h;
    term->cols = fb_width  / font_w;
    term->rows = fb_height / font_h;
}

static void term_scroll(struct terminal *term) {
    uint32_t row_bytes = term->fb_pitch * term->font_height;
    uint32_t total     = term->fb_pitch * term->fb_height;

    h_memmove(term->fb_addr,
              (uint8_t *)term->fb_addr + row_bytes,
              total - row_bytes);

    h_memset((uint8_t *)term->fb_addr + total - row_bytes, 0, row_bytes);

    if (term->cursor_y > 0) term->cursor_y--;
}

void cuoreterm_draw_char(struct terminal *term, char c, uint32_t fg) {
    if (c == '\n') {
        term->cursor_x = 0;
        term->cursor_y++;
        if (term->cursor_y >= term->rows) term_scroll(term);
        return;
    }

    uint32_t px = term->cursor_x * term->font_width;
    uint32_t py = term->cursor_y * term->font_height;

    const uint8_t *glyph = term->font_data + 4 + ((uint8_t)c * term->font_height);

    draw_glyph(term, px, py, glyph, term->font_height, fg);

    term->cursor_x++;
    if (term->cursor_x >= term->cols) {
        term->cursor_x = 0;
        term->cursor_y++;
        if (term->cursor_y >= term->rows) term_scroll(term);
    }
}

// color support (a mix of ansi and hex color codes)
// example: "\x1b[#FF0000mthis is red\x1b[0m and this is white\n",

static uint32_t hex_digit(char c) {
    if (c >= '0' && c <= '9') return (uint32_t)(c - '0');
    if (c >= 'a' && c <= 'f') return 10 + (uint32_t)(c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (uint32_t)(c - 'A');
    return 0;
}

static uint32_t parse_hex_color(const char *s) {
    uint32_t color = 0xFF000000; // default alpha
    uint32_t digits = 0;

    while (*s && digits < 6) {
        color = (color << 4) | hex_digit(*s);
        s++;
        digits++;
    }

    if (digits < 6) color <<= 4 * (6 - digits);
    return color | 0xFF000000; // alpha = 255
}

static void handle_hex_ansi(struct terminal *term, char **p_c) {
    char *c = *p_c;
    if (!c || *c != '[') return;
    c++; // skip [

    // reset shortcut
    if (*c == '0' && (*(c+1) == 'm' || *(c+1) == '\0')) {
        term->fgcol = 0xFFFFFFFF; // default fg
        *p_c = c + 2;
        return;
    }

    // fg hex color
    if (*c == '#') {
        c++;
        term->fgcol = parse_hex_color(c);
        for (uint32_t i = 0; i < 6; i++) {
            char x = *c;
            if ((x >= '0' && x <= '9') ||
                (x >= 'a' && x <= 'f') ||
                (x >= 'A' && x <= 'F')) c++;
        }
    }

    if (*c == 'm') c++;
    *p_c = c;
}

// write text
void cuoreterm_write(void *ctx, const char *msg, uint64_t len) {
    struct terminal *term = (struct terminal *)ctx;
    char *p_c = (char *)msg;
    char *end = p_c + len;

    while (p_c < end) {
        char c = *p_c++;

        if (c == '\n') {
            term->cursor_x = 0;
            term->cursor_y++;
            if (term->cursor_y >= term->rows) term_scroll(term);
        }
        else if (c == '\b') {
            if (term->cursor_x == 0) {
                if (term->cursor_y == 0) continue;
                term->cursor_y--;
                term->cursor_x = term->cols - 1;
            } else {
                term->cursor_x--;
            }
            uint32_t px = term->cursor_x * term->font_width;
            uint32_t py = term->cursor_y * term->font_height;
            const uint8_t *glyph = term->font_data + 4 + (' ' * term->font_height);
            draw_glyph(term, px, py, glyph, term->font_height, 0x00FFFFFF);
        }
        else if (c == '\x1b') {
            if (p_c < end && *p_c == '[') handle_hex_ansi(term, &p_c);
        }
        else {
            cuoreterm_draw_char(term, c, term->fgcol);
        }
    }
}

// runtime font switching
void cuoreterm_set_font(struct terminal *term, const uint8_t *font, uint32_t font_w, uint32_t font_h) {
    term->font_data   = font;
    term->font_width  = font_w;
    term->font_height = font_h;
    term->cols = term->fb_width / font_w;
    term->rows = term->fb_height / font_h;
}

// clear all text on screen
void cuoreterm_clear(struct terminal *term) {
    uint32_t total_bytes = term->fb_pitch * term->fb_height;
    h_memset(term->fb_addr, 0x00, total_bytes);
    term->cursor_x = 0;
    term->cursor_y = 0;
}