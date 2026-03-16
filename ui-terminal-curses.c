/* This file is included from ui-terminal.c */
#include <curses.h>

#define UI_TERMKEY_FLAGS (TERMKEY_FLAG_UTF8|TERMKEY_FLAG_NOTERMIOS)

#define CELL_COLOR_BLACK   COLOR_BLACK
#define CELL_COLOR_RED     COLOR_RED
#define CELL_COLOR_GREEN   COLOR_GREEN
#define CELL_COLOR_YELLOW  COLOR_YELLOW
#define CELL_COLOR_BLUE    COLOR_BLUE
#define CELL_COLOR_MAGENTA COLOR_MAGENTA
#define CELL_COLOR_CYAN    COLOR_CYAN
#define CELL_COLOR_WHITE   COLOR_WHITE
#define CELL_COLOR_DEFAULT (-1)

#ifndef A_ITALIC
#define A_ITALIC A_NORMAL
#endif
#define CELL_ATTR_NORMAL    A_NORMAL
#define CELL_ATTR_UNDERLINE A_UNDERLINE
#define CELL_ATTR_REVERSE   A_REVERSE
#define CELL_ATTR_BLINK     A_BLINK
#define CELL_ATTR_BOLD      A_BOLD
#define CELL_ATTR_ITALIC    A_ITALIC
#define CELL_ATTR_DIM       A_DIM

#ifdef NCURSES_VERSION
# ifndef NCURSES_EXT_COLORS
#  define NCURSES_EXT_COLORS 0
# endif
# if !NCURSES_EXT_COLORS
#  define MAX_COLOR_PAIRS MIN(COLOR_PAIRS, 256)
# endif
#endif
#ifndef MAX_COLOR_PAIRS
# define MAX_COLOR_PAIRS COLOR_PAIRS
#endif

#define MAX_COLOR_CLOBBER 240

typedef struct {
	short *color2palette;
	short color_pair_current;
	int change_colors;
	short default_fg;
	short default_bg;
} CursesData;

static inline bool cell_color_equal(CellColor c1, CellColor c2) {
	return c1 == c2;
}

/* Calculate r,g,b components of one of the standard upper 240 colors */
static void get_6cube_rgb(unsigned int n, int *r, int *g, int *b)
{
	if (n < 16) {
		return;
	} else if (n < 232) {
		n -= 16;
		*r = (n / 36) ? (n / 36) * 40 + 55 : 0;
		*g = ((n / 6) % 6) ? ((n / 6) % 6) * 40 + 55 : 0;
		*b = (n % 6) ? (n % 6) * 40 + 55 : 0;
	} else if (n < 256) {
		n -= 232;
		*r = n * 10 + 8;
		*g = n * 10 + 8;
		*b = n * 10 + 8;
	}
}

/* Reset color palette to default values using OSC 104 */
static void undo_palette(void)
{
	fputs("\033]104;\a", stderr);
	fflush(stderr);
}

static short color_clobber_idx = 0;
static uint32_t clobbering_colors[MAX_COLOR_CLOBBER];

/* Work out the nearest color from the 256 color set, or perhaps exactly. */
static CellColor color_rgb(Ui *ui, uint8_t r, uint8_t g, uint8_t b)
{
	CursesData *data = ui->backend_data;

	if (!data) {
		data = calloc(1, sizeof(CursesData));
		if (!data) {
			return CELL_COLOR_DEFAULT;
		}
		ui->backend_data = data;
		data->change_colors = -1;
	}

	if (data->change_colors == -1) {
		data->change_colors = ui->vix->change_colors && can_change_color() && COLORS >= 256;
	}
	if (data->change_colors) {
		uint32_t hexrep = (r << 16) | (g << 8) | b;
		for (short i = 0; i < MAX_COLOR_CLOBBER; ++i) {
			if (clobbering_colors[i] == hexrep) {
				return i + 16;
			} else if (!clobbering_colors[i]) {
				break;
			}
		}

		short i = color_clobber_idx;
		clobbering_colors[i] = hexrep;
		init_color(i + 16, (r * 1000) / 0xff, (g * 1000) / 0xff,
		           (b * 1000) / 0xff);

		/* in the unlikely case a user requests this many colors, reuse old slots */
		if (++color_clobber_idx >= MAX_COLOR_CLOBBER) {
			color_clobber_idx = 0;
		}

		return i + 16;
	}

	static const unsigned char color_256_to_16[256] = {
		 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
		 0,  4,  4,  4, 12, 12,  2,  6,  4,  4, 12, 12,  2,  2,  6,  4,
		12, 12,  2,  2,  2,  6, 12, 12, 10, 10, 10, 10, 14, 12, 10, 10,
		10, 10, 10, 14,  1,  5,  4,  4, 12, 12,  3,  8,  4,  4, 12, 12,
		 2,  2,  6,  4, 12, 12,  2,  2,  2,  6, 12, 12, 10, 10, 10, 10,
		14, 12, 10, 10, 10, 10, 10, 14,  1,  1,  5,  4, 12, 12,  1,  1,
		 5,  4, 12, 12,  3,  3,  8,  4, 12, 12,  2,  2,  2,  6, 12, 12,
		10, 10, 10, 10, 14, 12, 10, 10, 10, 10, 10, 14,  1,  1,  1,  5,
		12, 12,  1,  1,  1,  5, 12, 12,  1,  1,  1,  5, 12, 12,  3,  3,
		 3,  7, 12, 12, 10, 10, 10, 10, 14, 12, 10, 10, 10, 10, 10, 14,
		 9,  9,  9,  9, 13, 12,  9,  9,  9,  9, 13, 12,  9,  9,  9,  9,
		13, 12,  9,  9,  9,  9, 13, 12, 11, 11, 11, 11,  7, 12, 10, 10,
		10, 10, 10, 14,  9,  9,  9,  9,  9, 13,  9,  9,  9,  9,  9, 13,
		 9,  9,  9,  9,  9, 13,  9,  9,  9,  9,  9, 13,  9,  9,  9,  9,
		 9, 13, 11, 11, 11, 11, 11, 15,  0,  0,  0,  0,  0,  0,  8,  8,
		 8,  8,  8,  8,  7,  7,  7,  7,  7,  7, 15, 15, 15, 15, 15, 15
	};

	int i = 0;
	if ((!r || (r - 55) % 40 == 0) &&
	    (!g || (g - 55) % 40 == 0) &&
	    (!b || (b - 55) % 40 == 0)) {
		i = 16;
		i += r ? ((r - 55) / 40) * 36 : 0;
		i += g ? ((g - 55) / 40) * 6 : 0;
		i += g ? ((b - 55) / 40) : 0;
	} else if (r == g && g == b && (r - 8) % 10 == 0 && r < 239) {
		i = 232 + ((r - 8) / 10);
	} else {
		unsigned lowest = UINT_MAX;
		for (int j = 16; j < 256; ++j) {
			int jr = 0, jg = 0, jb = 0;
			get_6cube_rgb(j, &jr, &jg, &jb);
			int dr = jr - r;
			int dg = jg - g;
			int db = jb - b;
			unsigned int distance = dr * dr + dg * dg + db * db;
			if (distance < lowest) {
				lowest = distance;
				i = j;
			}
		}
	}

	if (COLORS <= 16) {
		return color_256_to_16[i];
	}
	return i;
}

static CellColor color_terminal(Ui *ui, uint8_t index) {
	return index;
}

static inline unsigned int color_pair_hash(short fg, short bg) {
	if (fg == CELL_COLOR_DEFAULT) {
		fg = COLORS;
	}
	if (bg == CELL_COLOR_DEFAULT) {
		bg = COLORS + 1;
	}
	return fg * (COLORS + 2) + bg;
}

static short color_pair_get(Ui *ui, short fg, short bg) {
	static bool has_default_colors;
	CursesData *data = ui->backend_data;

	if (!data) {
		data = calloc(1, sizeof(CursesData));
		if (!data) {
			return 0;
		}
		ui->backend_data = data;
		data->change_colors = -1;
		pair_content(0, &data->default_fg, &data->default_bg);
		has_default_colors = (use_default_colors() == OK);
		if (COLORS) {
			data->color2palette = calloc((COLORS + 2) * (COLORS + 2), sizeof(short));
		}
	}

	if (fg >= COLORS) {
		fg = data->default_fg;
	}
	if (bg >= COLORS) {
		bg = data->default_bg;
	}

	if (!has_default_colors) {
		if (fg == -1) {
			fg = data->default_fg;
		}
		if (bg == -1) {
			bg = data->default_bg;
		}
	}

	if (!data->color2palette) {
		return 0;
	}

	unsigned int index = color_pair_hash(fg, bg);
	if (data->color2palette[index] == 0) {
		short oldfg, oldbg;
		short color_pairs_max = MIN(MAX_COLOR_PAIRS, SHRT_MAX);
		if (++data->color_pair_current >= color_pairs_max) {
			data->color_pair_current = 1;
		}
		pair_content(data->color_pair_current, &oldfg, &oldbg);
		unsigned int old_index = color_pair_hash(oldfg, oldbg);
		if (init_pair(data->color_pair_current, fg, bg) == OK) {
			data->color2palette[old_index] = 0;
			data->color2palette[index] = data->color_pair_current;
		}
	}

	return data->color2palette[index];
}

static inline attr_t style_to_attr(Ui *ui, CellStyle *style) {
	return style->attr | COLOR_PAIR(color_pair_get(ui, style->fg, style->bg));
}

static void ui_term_backend_blit(Ui *tui) {
	if (tui->vix->headless) {
		return;
	}
	int w = tui->width, h = tui->height;
	Cell *cell = tui->cells;
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; ) {
			attr_t attr = style_to_attr(tui, &cell->style);
			attrset(attr);
			int start_x = x;
			char buf[4096] = "";
			size_t buf_len = 0;
			while (x < w && style_to_attr(tui, &cell->style) == attr) {
				size_t len = strlen(cell->data);
				if (buf_len + len < sizeof(buf)) {
					memcpy(buf + buf_len, cell->data, len);
					buf_len += len;
					x++;
					cell++;
				} else {
					break;
				}
			}
			buf[buf_len] = '\0';
			mvaddstr(y, start_x, buf);
		}
	}
	move(tui->cur_row, tui->cur_col);
	wnoutrefresh(stdscr);
	if (tui->doupdate) {
		doupdate();
	}
}

static void ui_term_backend_clear(Ui *tui) {
	if (tui->vix->headless) {
		return;
	}
	clear();
}

static bool ui_term_backend_resize(Ui *tui, int width, int height) {
	if (tui->vix->headless) {
		return true;
	}
	return resizeterm(height, width) == OK &&
	       wresize(stdscr, height, width) == OK;
}

static void ui_term_backend_save(Ui *tui, bool fscr) {
	if (tui->vix->headless) {
		return;
	}
	if (fscr) {
		def_prog_mode();
		endwin();
	} else if (tui->is_tty) {
		reset_shell_mode();
	}
}

static void ui_term_backend_restore(Ui *tui) {
	if (tui->vix->headless) {
		return;
	}
	if (tui->is_tty) {
		reset_prog_mode();
	}

	CursesData *data = tui->backend_data;
	if (data && data->change_colors) {
		for (short i = 0; i < color_clobber_idx; ++i) {
			uint32_t hexrep = clobbering_colors[i];
			uint8_t r = (hexrep >> 16) & 0xff;
			uint8_t g = (hexrep >> 8) & 0xff;
			uint8_t b = hexrep & 0xff;
			init_color(i + 16, (r * 1000) / 0xff, (g * 1000) / 0xff,
			           (b * 1000) / 0xff);
		}
	}

	redrawwin(stdscr);
	wclear(stdscr);
}

int ui_terminal_colors(void) {
	return COLORS;
}

static bool ui_term_backend_init(Ui *tui, char *term) {
	SCREEN *screen = newterm(term, stderr, stdin);
	if (!screen) {
		snprintf(tui->info, sizeof(tui->info), "Warning: unknown term `%s'", term);
		screen = newterm(strstr(term, "-256color") ? "xterm-256color" : "xterm", stderr, stdin);
		if (!screen) {
			return false;
		}
	}
	tui->ctx = screen;
	start_color();
	use_default_colors();
	cbreak();
	noecho();
	nonl();
	if (tui->is_tty) {
		keypad(stdscr, TRUE);
	}
	meta(stdscr, TRUE);
	scrollok(stdscr, FALSE);
	if (tui->is_tty) {
		curs_set(0);
	}
	return true;
}

static bool ui_backend_init(Ui *ui) {
	return true;
}

void ui_terminal_resume(Ui *term) { }

static void ui_term_backend_suspend(Ui *term) {
	CursesData *data = term->backend_data;
	if (data && data->change_colors == 1) {
		undo_palette();
	}
	if (term->is_tty) {
		curs_set(1);
	}
}

static void ui_term_backend_free(Ui *term) {
	ui_term_backend_suspend(term);
	CursesData *data = term->backend_data;
	if (data) {
		free(data->color2palette);
		free(data);
		term->backend_data = NULL;
	}
	endwin();
	if (term->ctx) {
		delscreen(term->ctx);
		term->ctx = NULL;
	}
}

static bool is_default_color(CellColor c) {
	return c == CELL_COLOR_DEFAULT;
}
