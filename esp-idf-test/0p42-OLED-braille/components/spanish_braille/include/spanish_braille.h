/**
 * Spanish Braille (Grade 1) - scalable cell drawing for U8g2.
 * Pattern: 6-bit byte, bit0 = position 1, ..., bit5 = position 6.
 */

#ifndef SPANISH_BRAILLE_H
#define SPANISH_BRAILLE_H

#include <stdint.h>
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Decode first UTF-8 character from \a utf8, look up Braille pattern, write to \a pattern_out.
 * \a pattern_out is one byte: bit 0 = dot 1, ..., bit 5 = dot 6 (1 = raised).
 * \return Number of bytes consumed from \a utf8 (1 or 2 for supported chars), or 0 if no mapping.
 */
int braille_es_char_to_pattern(const char *utf8, uint8_t *pattern_out);

/**
 * Draw a single Braille cell at buffer position (\a x, \a y).
 * \a pattern: 6-bit byte (bit 0 = position 1, ..., bit 5 = position 6).
 * \a dot_radius: radius in pixels of each dot (scalable).
 * Uses a fixed gap of 2 pixels between dots. Use braille_es_cell_size() to get dimensions.
 */
void braille_es_draw_cell(u8g2_t *u8g2, int x, int y, uint8_t pattern, int dot_radius);

/**
 * Get cell width and height in pixels for given \a dot_radius and \a gap.
 * Layout: 2 columns x 3 rows of dots; step = 2*dot_radius + gap.
 * \a width = 2 * step, \a height = 3 * step (or pass NULL to ignore).
 */
void braille_es_cell_size(int dot_radius, int gap, int *width, int *height);

#ifdef __cplusplus
}
#endif

#endif /* SPANISH_BRAILLE_H */
