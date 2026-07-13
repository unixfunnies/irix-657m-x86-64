/*
 * irix/kern/ml/X86_64/fb.c
 *
 * Framebuffer text console.  Limine hands us a linear framebuffer (and,
 * on hardware/VBox, has already set a video mode); we render an 8x16 VGA
 * font into it so boot output is visible on screen, not just on COM1.
 * kprintf fans out to both this and the serial port (see console_putc).
 *
 * Scope-appropriate: a simple green-on-black scrolling glyph grid, no
 * cursor, no escape sequences.  The real IRIX console/tty (gfx or
 * VT-on-serial) arrives much later; this is a boot console.
 */

#include <limine.h>
#include "x86_64.h"
#include "font8x16.h"

static volatile __u32	*fb;
static __u64		fb_w, fb_h, fb_pitch_px;
static __u32		fb_fg = 0x00c0c0c0;	/* light grey  */
static __u32		fb_bg = 0x00000000;	/* black       */
static __u32		cur_col, cur_row, cols, rows;

int
fb_init(struct limine_framebuffer *lf)
{
	if (lf == 0 || lf->address == 0 || lf->bpp != 32)
		return 0;			/* no usable FB: serial only */

	fb = (volatile __u32 *)lf->address;
	fb_w = lf->width;
	fb_h = lf->height;
	fb_pitch_px = lf->pitch / 4;
	cols = fb_w / FONT_W;
	rows = fb_h / FONT_H;
	cur_col = cur_row = 0;

	{
		__u64 i, n = fb_pitch_px * fb_h;
		for (i = 0; i < n; i++)
			fb[i] = fb_bg;
	}
	return 1;
}

static void
fb_draw_glyph(char c, __u32 col, __u32 row)
{
	const unsigned char *g = font8x16[(unsigned char)c];
	__u32 x0 = col * FONT_W, y0 = row * FONT_H;
	int gx, gy;

	for (gy = 0; gy < FONT_H; gy++) {
		volatile __u32 *line = fb + (y0 + gy) * fb_pitch_px + x0;
		unsigned char bits = g[gy];
		for (gx = 0; gx < FONT_W; gx++)
			line[gx] = (bits & (0x80 >> gx)) ? fb_fg : fb_bg;
	}
}

static void
fb_scroll(void)
{
	__u64 row_px = (__u64)FONT_H * fb_pitch_px;
	__u64 move = (rows - 1) * row_px;
	__u64 i;

	for (i = 0; i < move; i++)
		fb[i] = fb[i + row_px];
	for (i = move; i < (__u64)rows * row_px; i++)
		fb[i] = fb_bg;
}

void
fb_putc(char c)
{
	if (fb == 0)
		return;
	if (c == '\n') {
		cur_col = 0;
		cur_row++;
	} else if (c == '\r') {
		cur_col = 0;
	} else if (c == '\t') {
		cur_col = (cur_col + 8) & ~7u;
	} else {
		fb_draw_glyph(c, cur_col, cur_row);
		cur_col++;
	}
	if (cur_col >= cols) {
		cur_col = 0;
		cur_row++;
	}
	if (cur_row >= rows) {
		fb_scroll();
		cur_row = rows - 1;
	}
}
