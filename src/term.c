#include <stdint.h> // uint32_t, uint8_t
#include "cuoreterm.h"

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
) {
    term->fb_addr   = fb_addr;
    term->fb_width  = fb_width;
    term->fb_height = fb_height;
    term->fb_pitch  = fb_pitch;
    term->fb_bpp    = fb_bpp;

    term->fgcol = 0xFFFFFFFF;
    term->bgcol = 0x000000FF;

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

    uint32_t px = term->cursor_x * term->font_width;
    uint32_t py = term->cursor_y * term->font_height;

    const uint8_t *glyph =
        term->font_data + 4 + ((uint8_t)c * term->font_height);

    draw_glyph(term, px, py, glyph, term->font_height, fg, bg);

    term->cursor_x++;
    if (term->cursor_x >= term->cols) {
        term->cursor_x = 0;
        term->cursor_y++;
        if (term->cursor_y >= term->rows)
            term_scroll(term);
    }
}

void ansi_to_rgb(uint32_t *col, uint8_t ansi)
{
    // hope i got the colors right
    switch (ansi) {
        case ANSI_RED:
            *col = 0xFFFF0000;
            break;
        case ANSI_GREEN:
            *col = 0xFF00FF00;
            break;
        case ANSI_YELLOW:
            *col = 0xFFFFFF00;
            break;
        case ANSI_BLUE:
            *col = 0xFF0000FF;
            break;
        case ANSI_PURPLE:
            *col = 0xFFFF00FF;
            break;
        case ANSI_CYAN:
            *col = 0xFF00FFFF;
            break;
        case ANSI_WHITE:
            *col = 0xFFFFFFFF;
            break;
        case ANSI_BLACK:
        default:
            *col = 0xFFFFFFFF;
            break;
    }
}

void handle_ansi(struct terminal * term, char **p_c, int *i)
{
    int where = 0;
    uint8_t col = 0;
    char *c = *p_c;
    if (*c <= '0' || *c >= '9') {
        c++;
        i++;
        return;
    }
    where = *c++ - 0x30;

    if (*c >= '0' && *c <= '9') {
        col += *c++ - 0x30;
        i++;
    }

    if (where == 3) {
        ansi_to_rgb((uint32_t *)&(term->bgcol), col);
    } else if (where == 4) {
        ansi_to_rgb((uint32_t *)&(term->fgcol), col);
    } else if (where == 0) {
        term->fgcol = 0xFFFFFFFF;
        term->bgcol = 0xFF000000;
    }

    // this technically allows '\x1b[30;30;30;30;30;30;30;30;30m' but whatever
    if (*c == ';') {
        handle_ansi(term, c, i);
    }
}

void cuoreterm_write(void *ctx, const char *msg, uint64_t len) {
    struct terminal *term = (struct terminal *)ctx;
    
    char *p_c = msg;
    int i = 0;
    
    for (int i = 0; i < len; i++, p_c++) {
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

            uint32_t px = term->cursor_x * term->font_width;
            uint32_t py = term->cursor_y * term->font_height;

            const uint8_t *glyph =
                term->font_data + 4 + (' ' * term->font_height);

            draw_glyph(
                term,
                px,
                py,
                glyph,
                term->font_height,
                0x00FFFFFF,   // fg
                0x00000000    // bg
            );
        }
        else if (c == '\x1b') {
            p_c += 2;
            i += 2;

            handle_ansi(term, &p_c, &i);
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

void cuoreterm_set_font(
    struct terminal *term,
    const uint8_t *font,
    uint32_t font_w,
    uint32_t font_h
) {
    term->font_data   = font;
    term->font_width  = font_w;
    term->font_height = font_h;
    term->cols = term->fb_width / font_w;
    term->rows = term->fb_height / font_h;
}

void cuoreterm_clear(struct terminal *term) {
    uint32_t total_bytes = term->fb_pitch * term->fb_height;
    h_memset(term->fb_addr, 0x00, total_bytes);

    term->cursor_x = 0;
    term->cursor_y = 0;
}