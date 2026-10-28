/*
 * font_data.h - NanoUI 8x8 monospaced bitmap font (public interface)
 *
 * Every character lives in an 8x8 cell. The visible glyph is 5 columns x 7
 * rows, anchored at the cell's top-left corner. That leaves a free column on
 * the right and a free row at the bottom, which is exactly the room the
 * renderer needs for a 1px drop shadow (offset +1,+1) without ever touching
 * the neighbouring cell.
 *
 *   cell (8x8)                 columns 0-4 : glyph
 *   ##### . . .                column  5   : shadow of column 4
 *   ...                        columns 6-7 : letter spacing
 *   rows 0-6 : glyph, row 7 : shadow of row 6
 */
#ifndef FONT_DATA_H
#define FONT_DATA_H

#include <tonc.h>

#define FONT_CELL_W        8                      /* advance per character (px)   */
#define FONT_CELL_H        8                      /* cell height (px)             */
#define FONT_GLYPH_W       5                      /* visible glyph width (px)     */
#define FONT_GLYPH_H       7                      /* visible glyph height (px)    */
#define FONT_ROW_MSB       (1u << (FONT_GLYPH_W - 1)) /* leftmost pixel bit of a row */
#define FONT_TRAIL_PX      (FONT_CELL_W - FONT_GLYPH_W - 1) /* unused px after the
                                                               shadow of the last char */
#define FONT_FIRST_CHAR    32                     /* ' '                          */
#define FONT_LAST_CHAR     126                    /* '~'                          */
#define FONT_GLYPH_COUNT   (FONT_LAST_CHAR - FONT_FIRST_CHAR + 1)
#define FONT_FALLBACK_CHAR '?'                    /* shown for unsupported chars  */

/*
 * Returns a pointer to FONT_GLYPH_H bytes (one per row, top to bottom).
 * Bit (FONT_GLYPH_W-1) is the leftmost pixel, bit 0 the rightmost.
 * Never returns NULL: unknown characters map to FONT_FALLBACK_CHAR.
 */
const u8 *font_get_glyph(char c);

/*
 * Word-wrap helper. Looks at the text starting at `s` and decides what fits
 * on ONE line of at most `max_cols` characters.
 *   - '\n' forces a line break.
 *   - Lines break at the last space that fits; words longer than a whole
 *     line are hard-broken.
 *   - *out_len receives the number of characters to DRAW on this line
 *     (the break character itself is excluded).
 * Returns the number of source characters consumed (including the break
 * character). Always >= 1 when *s != '\0', so callers can loop safely.
 */
int font_wrap_line(const char *s, int max_cols, int *out_len);

#endif /* FONT_DATA_H */
