/* This file is included from ui-terminal.c
 *
 * The goal is *not* to reimplement curses. Instead we aim to provide the
 * simplest possible drawing backend for VT-100 compatible terminals.
 * This is useful for debugging and fuzzing purposes as well as for environments
 * with no curses support.
 *
 * A front/back buffer scheme is used to avoid re-emitting the whole screen
 * on every draw: only cells which actually changed since the last blit are
 * written out.
 *
 * The following terminal escape sequences are used:
 *
 *  - CSI ? 1049 h             Save cursor and use Alternate Screen Buffer (DECSET)
 *  - CSI ? 1049 l             Use Normal Screen Buffer and restore cursor (DECRST)
 *  - CSI ? 25 l               Hide Cursor (DECTCEM)
 *  - CSI ? 25 h               Show Cursor (DECTCEM)
 *  - CSI 2 J                  Erase in Display (ED)
 *  - CSI row ; column H       Cursor Position (CUP)
 *  - CSI ... m                Character Attributes (SGR)
 *    - CSI 0 m                     Normal
 *    - CSI 1 m                     Bold
 *    - CSI 3 m                     Italicized
 *    - CSI 4 m                     Underlined
 *    - CSI 5 m                     Blink
 *    - CSI 7 m                     Inverse
 *    - CSI 22 m                    Normal (not bold)
 *    - CSI 23 m                    Not italicized
 *    - CSI 24 m                    Not underlined
 *    - CSI 25 m                    Not blinking
 *    - CSI 27 m                    Not inverse
 *    - CSI 39                      Set default foreground color
 *    - CSI 38 ; 5 ; I m            Set indexed foreground color
 *    - CSI 38 ; 2 ; R ; G ; B m    Set RGB foreground color
 *    - CSI 49                      Set default background color
 *    - CSI 48 ; 5 ; I m            Set indexed background color
 *    - CSI 48 ; 2 ; R ; G ; B m    Set RGB background color
 *
 * See http://invisible-island.net/xterm/ctlseqs/ctlseqs.txt
 * for further information.
 */
#include "buffer.h"

#define UI_TERMKEY_FLAGS TERMKEY_FLAG_UTF8

static CellStyle ui_backend_style_default(Ui *ui) {
	CellStyle result = {0};
	result.properties |= CELL_STYLE_FG_SET|CELL_STYLE_FG_DEFAULT;
	result.properties |= CELL_STYLE_BG_SET|CELL_STYLE_BG_DEFAULT;
	return result;
}

/* vt100 backend private state, stored as Ui.ctx */
typedef struct {
	Buffer output;        /* escape-sequence staging buffer, reused across blits */
	Cell *front;          /* front buffer: cell contents as last actually written to the terminal */
	size_t front_size;    /* #bytes allocated for front (grows only, mirrors Ui.cells_size) */
	bool flush_terminal;  /* force a full redraw, e.g. because the terminal content may have
	                          changed externally (resume from suspend, explicit redraw request) */
} Vt100Ctx;

static void output(const char *data, size_t len) {
	if (write(STDERR_FILENO, data, len) == -1) {
		/* ignore error */
	}
}

static void output_literal(const char *data) {
	output(data, strlen(data));
}

static void screen_alternate(bool alternate) {
	output_literal(alternate ? "\x1b[?1049h" : "\x1b[0m" "\x1b[?1049l" "\x1b[0m" );
}

static void cursor_visible(bool visible) {
	output_literal(visible ? "\x1b[?25h" : "\x1b[?25l");
}

static bool cell_equal(const Cell *a, const Cell *b) {
	return memcmp(a, b, sizeof(*a)) == 0;
}

/* grow (never shrink) vt->front to hold width*height cells, mirroring
 * ui_resize()'s handling of Ui.cells. Called from both blit() and resize()
 * so the front buffer is guaranteed valid before blit() ever dereferences
 * it, regardless of whether resize() has run yet. */
static bool vt100_front_ensure(Vt100Ctx *vt, int width, int height) {
	size_t size = (size_t)width * (size_t)height * sizeof(Cell);
	if (size > vt->front_size) {
		Cell *front = realloc(vt->front, size);
		if (!front) {
			return false;
		}
		memset((char *)front + vt->front_size, 0, size - vt->front_size);
		vt->front_size = size;
		vt->front = front;
	}
	return true;
}

static void ui_term_backend_blit(Ui *tui) {
	Vt100Ctx *vt = tui->ctx;
	if (!vt100_front_ensure(vt, tui->width, tui->height)) {
		return;
	}
	Buffer *buf = &vt->output;
	buf->len    = 0;
	CellAttr attr = CELL_ATTR_NORMAL;
	CellStyle style_prev = ui_backend_style_default(tui);
	int w = tui->width, h = tui->height;
	Cell *cell = tui->cells;
	Cell *front = vt->front;
	int cursor_x = 0, cursor_y = 0;
	/* reposition cursor, reset attributes */
	buffer_append0(buf, "\x1b[H" "\x1b[0m");
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++, cell++, front++) {
			if (y == h - 1 && x == w - 1) {
				/* never write to the bottom right cell: doing so can
				 * trigger an unwanted scroll/auto-wrap on some terminals.
				 * still record it in front so front keeps meaning "last
				 * cell buffer we processed" rather than silently going
				 * stale at this one position. */
				*front = *cell;
				continue;
			}
			if (!vt->flush_terminal && cell_equal(cell, front)) {
				continue;
			}

			if (cursor_x != x || cursor_y != y) {
				buffer_appendf(buf, "\x1b[%d;%dH", y + 1, x + 1);
			}

			CellStyle *style = &cell->style;
			if (style->attr != attr) {

				static const struct {
					CellAttr attr;
					char on[4], off[4];
				} cell_attrs[] = {
					{ CELL_ATTR_BOLD, "1", "22" },
					{ CELL_ATTR_DIM, "2", "22" },
					{ CELL_ATTR_ITALIC, "3", "23" },
					{ CELL_ATTR_UNDERLINE, "4", "24" },
					{ CELL_ATTR_BLINK, "5", "25" },
					{ CELL_ATTR_REVERSE, "7", "27" },
				};

				for (size_t i = 0; i < LENGTH(cell_attrs); i++) {
					CellAttr a = cell_attrs[i].attr;
					if ((style->attr & a) == (attr & a)) {
						continue;
					}
					buffer_appendf(buf, "\x1b[%sm",
					               style->attr & a ?
					               cell_attrs[i].on :
					               cell_attrs[i].off);
				}

				attr = style->attr;
			}

			if (!cell_style_fg_equal(style_prev, *style)) {
				if (style->properties & CELL_STYLE_FG_DEFAULT) {
					buffer_append0(buf, "\x1b[39m");
				} else if (style->properties & CELL_STYLE_FG_INDEXED) {
					buffer_appendf(buf, "\x1b[38;5;%um", (unsigned)CellStyleFGIndexGet(style));
				} else {
					buffer_appendf(buf, "\x1b[38;2;%d;%d;%dm",
					               style->fg_r, style->fg_g, style->fg_b);
				}
				cell_style_copy_fg(&style_prev, *style);
			}

			if (!cell_style_bg_equal(style_prev, *style)) {
				if (style->properties & CELL_STYLE_BG_DEFAULT) {
					buffer_append0(buf, "\x1b[49m");
				} else if (style->properties & CELL_STYLE_BG_INDEXED) {
					buffer_appendf(buf, "\x1b[48;5;%um", (unsigned)CellStyleBGIndexGet(style));
				} else {
					buffer_appendf(buf, "\x1b[48;2;%d;%d;%dm",
					               style->bg_r, style->bg_g, style->bg_b);
				}
				cell_style_copy_bg(&style_prev, *style);
			}

			buffer_append0(buf, cell->data);
			*front = *cell;

			if (cell->data[0]) {
				/* we printed a real glyph: the terminal's own cursor
				 * advances by its column width, so our tracking can
				 * trust that without another explicit reposition */
				cursor_x = x + (cell->width > 0 ? cell->width : 1);
				cursor_y = y;
				if (cursor_x >= w) {
					cursor_x = 0;
					cursor_y++;
				}
			} else {
				/* nothing was actually printed (e.g. a wide character's
				 * continuation cell going blank): the real cursor did not
				 * move, but we don't know where that leaves it relative
				 * to what we'd otherwise assume, so invalidate our guess
				 * and force the next write to reposition explicitly */
				cursor_x = -1;
				cursor_y = -1;
			}
		}
	}
	vt->flush_terminal = false;
	/* move cursor */
	buffer_appendf(buf, "\x1b[%d;%dH", tui->cur_row + 1, tui->cur_col + 1);
	output(buf->data, buffer_length0(buf));
}

static void ui_term_backend_clear(Ui *tui) {
	Vt100Ctx *vt = tui->ctx;
	if (vt) {
		vt->flush_terminal = true;
	}
}

static bool ui_term_backend_resize(Ui *tui, int width, int height) {
	Vt100Ctx *vt = tui->ctx;
	if (!vt100_front_ensure(vt, width, height)) {
		return false;
	}
	/* the terminal itself may have reflowed, cleared, or repositioned
	 * content on its own in response to the physical resize (SIGWINCH),
	 * so the front buffer can no longer be trusted to match reality */
	vt->flush_terminal = true;
	/* home the cursor, then erase from there to end of screen: clears any
	 * leftover content beyond the new dimensions, e.g. when shrinking */
	output_literal("\x1b[H" "\x1b[J");
	return true;
}

static void ui_term_backend_save(Ui *tui, bool fscr) {
}

static void ui_term_backend_restore(Ui *tui) {
	/* the terminal contents may have changed while we weren't drawing
	 * (e.g. another program ran while we were suspended), so the front
	 * buffer can no longer be trusted: force a full redraw */
	Vt100Ctx *vt = tui->ctx;
	if (vt) {
		vt->flush_terminal = true;
	}
}

int ui_terminal_colors(void) {
	char *term = getenv("TERM");
	return (term && (strstr)(term, "-256color")) ? 256 : 16;
}

static void ui_term_backend_suspend(Ui *tui) {
	if (!tui->termkey) {
		return;
	}
	termkey_stop(tui->termkey);
	if (tui->is_tty) {
		cursor_visible(true);
		screen_alternate(false);
	}
}

void ui_terminal_resume(Ui *tui) {
	if (tui->is_tty) {
		screen_alternate(true);
		cursor_visible(false);
	}
	termkey_start(tui->termkey);
}

static bool ui_term_backend_init(Ui *tui, char *term) {
	ui_terminal_resume(tui);
	return true;
}

static bool ui_backend_init(Ui *ui) {
	Vt100Ctx *vt = calloc(1, sizeof(Vt100Ctx));
	if (!vt) {
		return false;
	}
	ui->ctx = vt;
	return true;
}

static void ui_term_backend_free(Ui *tui) {
	Vt100Ctx *vt = tui->ctx;
	ui_term_backend_suspend(tui);
	buffer_release(&vt->output);
	free(vt->front);
	free(vt);
}
