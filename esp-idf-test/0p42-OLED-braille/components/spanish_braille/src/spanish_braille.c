/*
 * Spanish Braille (Grade 1) - lookup table and scalable cell drawing.
 * See SPANISH_BRAILLE_REFERENCE.md for dot positions (1-6) and character mapping.
 * Pattern byte: bit0=pos1, bit1=pos2, bit2=pos3, bit3=pos4, bit4=pos5, bit5=pos6.
 */

#include "spanish_braille.h"
#include <stddef.h>

#define BRAILLE_GAP 2

/* Returns Braille pattern (6-bit) for Unicode codepoint, or 0 if no mapping. */
static uint8_t codepoint_to_pattern(uint32_t cp)
{
    switch (cp) {
    /* Letters a-z */
    case 0x61: return 0x01;   /* a: 1 */
    case 0x62: return 0x03;   /* b: 1-2 */
    case 0x63: return 0x09;   /* c: 1-4 */
    case 0x64: return 0x19;   /* d: 1-4-5 */
    case 0x65: return 0x11;   /* e: 1-5 */
    case 0x66: return 0x0B;   /* f: 1-2-4 */
    case 0x67: return 0x1B;   /* g: 1-2-4-5 */
    case 0x68: return 0x13;   /* h: 1-2-5 */
    case 0x69: return 0x0A;   /* i: 2-4 */
    case 0x6A: return 0x1A;   /* j: 2-4-5 */
    case 0x6B: return 0x05;   /* k: 1-3 */
    case 0x6C: return 0x07;   /* l: 1-2-3 */
    case 0x6D: return 0x0D;   /* m: 1-3-4 */
    case 0x6E: return 0x1D;   /* n: 1-3-4-5 */
    case 0x6F: return 0x15;   /* o: 1-3-5 */
    case 0x70: return 0x0F;   /* p: 1-2-3-4 */
    case 0x71: return 0x1F;   /* q: 1-2-3-4-5 */
    case 0x72: return 0x17;   /* r: 1-2-3-5 */
    case 0x73: return 0x0E;   /* s: 2-3-4 */
    case 0x74: return 0x1E;   /* t: 2-3-4-5 */
    case 0x75: return 0x25;   /* u: 1-3-6 */
    case 0x76: return 0x23;   /* v: 1-2-3-6 */
    case 0x77: return 0x3A;   /* w: 2-4-5-6 */
    case 0x78: return 0x29;   /* x: 1-3-4-6 */
    case 0x79: return 0x39;   /* y: 1-3-4-5-6 */
    case 0x7A: return 0x31;   /* z: 1-3-5-6 */
    /* Uppercase A-Z: same pattern as lowercase (capital sign not drawn in single-cell demo) */
    case 0x41: return 0x01;   /* A */
    case 0x42: return 0x03;   /* B */
    case 0x43: return 0x09;   /* C */
    case 0x44: return 0x19;   /* D */
    case 0x45: return 0x11;   /* E */
    case 0x46: return 0x0B;   /* F */
    case 0x47: return 0x1B;   /* G */
    case 0x48: return 0x13;   /* H */
    case 0x49: return 0x0A;   /* I */
    case 0x4A: return 0x1A;   /* J */
    case 0x4B: return 0x05;   /* K */
    case 0x4C: return 0x07;   /* L */
    case 0x4D: return 0x0D;   /* M */
    case 0x4E: return 0x1D;   /* N */
    case 0x4F: return 0x15;   /* O */
    case 0x50: return 0x0F;   /* P */
    case 0x51: return 0x1F;   /* Q */
    case 0x52: return 0x17;   /* R */
    case 0x53: return 0x0E;   /* S */
    case 0x54: return 0x1E;   /* T */
    case 0x55: return 0x25;   /* U */
    case 0x56: return 0x23;   /* V */
    case 0x57: return 0x3A;   /* W */
    case 0x58: return 0x29;   /* X */
    case 0x59: return 0x39;   /* Y */
    case 0x5A: return 0x31;   /* Z */
    /* Spanish ñ and accented */
    case 0xF1: return 0x3B;   /* ñ: 1-2-4-5-6 (U+00F1) */
    case 0xE1: return 0x21;   /* á: 1-6 (U+00E1) */
    case 0xE9: return 0x2E;   /* é: 2-3-4-6 (U+00E9) */
    case 0xED: return 0x0C;   /* í: 3-4 (U+00ED) */
    case 0xF3: return 0x2C;   /* ó: 3-4-6 (U+00F3) */
    case 0xFA: return 0x36;   /* ú: 2-3-5-6 (U+00FA) */
    case 0xFC: return 0x33;   /* ü: 1-2-5-6 (U+00FC) */
    /* Digits 0-9 (same patterns as a-j) */
    case 0x30: return 0x1A;   /* 0 -> j */
    case 0x31: return 0x01;   /* 1 -> a */
    case 0x32: return 0x03;   /* 2 -> b */
    case 0x33: return 0x09;   /* 3 -> c */
    case 0x34: return 0x19;   /* 4 -> d */
    case 0x35: return 0x11;   /* 5 -> e */
    case 0x36: return 0x0B;   /* 6 -> f */
    case 0x37: return 0x1B;   /* 7 -> g */
    case 0x38: return 0x13;   /* 8 -> h */
    case 0x39: return 0x0A;   /* 9 -> i */
    /* Punctuation */
    case 0x2E: return 0x32;   /* . : 2-5-6 */
    case 0x2C: return 0x02;   /* , : 2 */
    case 0x3F: return 0x22;   /* ? : 2-6 */
    case 0xBF: return 0x22;   /* ¿ : same as ? (U+00BF) */
    case 0x21: return 0x16;   /* ! : 2-3-5 */
    case 0xA1: return 0x16;   /* ¡ : same as ! (U+00A1) */
    case 0x3B: return 0x06;   /* ; : 2-3 */
    case 0x3A: return 0x12;   /* : : 2-5 */
    case 0x2D: return 0x24;   /* - : 3-6 */
    case 0x27: return 0x04;   /* ' : 3 */
    case 0x22: return 0x26;   /* " : opening 2-3-6 (use same for both in demo) */
    default:   return 0;
    }
}

/* Decode one UTF-8 character; return codepoint and advance *utf8 by consumed bytes. */
static int decode_utf8(const char **utf8, uint32_t *cp)
{
    const unsigned char *p = (const unsigned char *)*utf8;
    if (!p || !p[0]) {
        *cp = 0;
        return 0;
    }
    if (p[0] <= 0x7F) {
        *cp = p[0];
        *utf8 = (const char *)(p + 1);
        return 1;
    }
    if (p[0] >= 0xC2 && p[0] <= 0xDF && p[1] >= 0x80 && p[1] <= 0xBF) {
        *cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        *utf8 = (const char *)(p + 2);
        return 2;
    }
    *cp = 0;
    return 0;
}

int braille_es_char_to_pattern(const char *utf8, uint8_t *pattern_out)
{
    if (!utf8 || !pattern_out) {
        return 0;
    }
    uint32_t cp;
    int n = decode_utf8(&utf8, &cp);
    if (n == 0) {
        return 0;
    }
    uint8_t p = codepoint_to_pattern(cp);
    if (p == 0) {
        return 0;
    }
    *pattern_out = p & 0x3F;
    return n;
}

void braille_es_draw_cell(u8g2_t *u8g2, int x, int y, uint8_t pattern, int dot_radius)
{
    if (!u8g2 || dot_radius <= 0) {
        return;
    }
    int step = 2 * dot_radius + BRAILLE_GAP;
    pattern &= 0x3F;
    for (int i = 0; i < 6; i++) {
        if (!(pattern & (1u << i))) {
            continue;
        }
        int col = i / 3;
        int row = i % 3;
        /* Center of dot (col, row): (x + (col+0.5)*step, y + (row+0.5)*step) */
        int cx = x + (2 * col + 1) * step / 2;
        int cy = y + (2 * row + 1) * step / 2;
        u8g2_DrawDisc(u8g2, (u8g2_uint_t)cx, (u8g2_uint_t)cy, (u8g2_uint_t)dot_radius, U8G2_DRAW_ALL);
    }
}

void braille_es_cell_size(int dot_radius, int gap, int *width, int *height)
{
    int step = 2 * dot_radius + gap;
    if (width) {
        *width = 2 * step;
    }
    if (height) {
        *height = 3 * step;
    }
}
