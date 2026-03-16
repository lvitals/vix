#include "vix.h"
#include "vix-core.h"
#include "text.h"
#include "util.h"
#include "text-util.h"

#ifndef DEBUG_UI
#define DEBUG_UI 0
#endif

#if DEBUG_UI
#define debug(...) do { printf(__VA_ARGS__); fflush(stdout); } while (0)
#else
#define debug(...) do { } while (0)
#endif

#if CONFIG_CURSES
#include "ui-terminal-curses.c"
#else
#include "ui-terminal-vt100.c"
#endif

/* helper macro for handling UiTerm.cells */
#define CELL_AT_POS(UI, X, Y) (((UI)->cells) + (X) + ((Y) * (UI)->width));

#define CELL_STYLE_DEFAULT (CellStyle){.fg = CELL_COLOR_DEFAULT, .bg = CELL_COLOR_DEFAULT, .attr = CELL_ATTR_NORMAL}

static bool is_default_fg(CellColor c) {
	return is_default_color(c);
}

static bool is_default_bg(CellColor c) {
	return is_default_color(c);
}

void ui_die(Ui *tui, const char *msg, va_list ap) {
	ui_terminal_free(tui);
	vfprintf(stderr, msg, ap);
	exit(EXIT_FAILURE);
}

static void ui_die_msg(Ui *ui, const char *msg, ...) {
	va_list ap;
	va_start(ap, msg);
	ui_die(ui, msg, ap);
	va_end(ap);
}

static void ui_window_resize(Win *win, int width, int height) {
	debug("ui-win-resize[%s]: %dx%d\n", win->file->name ? win->file->name : "noname", width, height);
	bool status = win->options & UI_OPTION_STATUSBAR;
	win->width  = width;
	win->height = height;
	if (!win->vix->ui.layout_only) {
		view_resize(&win->view, width - win->sidebar_width, status ? height - 1 : height);
	}
}

static void ui_window_move(Win *win, int x, int y) {
	debug("ui-win-move[%s]: (%d, %d)\n", win->file->name ? win->file->name : "noname", x, y);
	win->x = x;
	win->y = y;
}

static bool color_fromstring(Ui *ui, CellColor *color, const char *s)
{
	if (!s) {
		return false;
	}
	if (*s == '#' && strlen(s) == 7) {
		const char *cp;
		unsigned char r, g, b;
		for (cp = s + 1; isxdigit((unsigned char)*cp); cp++);
		if (*cp != '\0') {
			return false;
		}
		int n = sscanf(s + 1, "%2hhx%2hhx%2hhx", &r, &g, &b);
		if (n != 3) {
			return false;
		}
		*color = color_rgb(ui, r, g, b);
		return true;
	} else if ('0' <= *s && *s <= '9') {
		int index = atoi(s);
		if (index <= 0 || index > 255) {
			return false;
		}
		*color = color_terminal(ui, index);
		return true;
	}

	static const struct {
		const char *name;
		CellColor color;
	} color_names[] = {
		{ "black",   CELL_COLOR_BLACK   },
		{ "red",     CELL_COLOR_RED     },
		{ "green",   CELL_COLOR_GREEN   },
		{ "yellow",  CELL_COLOR_YELLOW  },
		{ "blue",    CELL_COLOR_BLUE    },
		{ "magenta", CELL_COLOR_MAGENTA },
		{ "cyan",    CELL_COLOR_CYAN    },
		{ "white",   CELL_COLOR_WHITE   },
		{ "default", CELL_COLOR_DEFAULT },
	};

	for (size_t i = 0; i < LENGTH(color_names); i++) {
		if (strcasecmp(color_names[i].name, s) == 0) {
			*color = color_names[i].color;
			return true;
		}
	}

	return false;
}

bool ui_style_define(Win *win, int id, const char *style) {
	Ui *tui = &win->vix->ui;
	if (id >= UI_STYLE_MAX) {
		return false;
	}
	if (!style) {
		return true;
	}

	CellStyle cell_style = CELL_STYLE_DEFAULT;
	char *style_copy = strdup(style), *option = style_copy;
	while (option) {
		while (*option == ' ') {
			option++;
		}
		char *next = (strchr)(option, ',');
		if (next) {
			*next++ = '\0';
		}
		char *value = (strchr)(option, ':');
		if (value) {
			for (*value++ = '\0'; *value == ' '; value++);
		}
		if (!strcasecmp(option, "reverse")) {
			cell_style.attr |= CELL_ATTR_REVERSE;
		} else if (!strcasecmp(option, "notreverse")) {
			cell_style.attr &= CELL_ATTR_REVERSE;
		} else if (!strcasecmp(option, "bold")) {
			cell_style.attr |= CELL_ATTR_BOLD;
		} else if (!strcasecmp(option, "notbold")) {
			cell_style.attr &= ~CELL_ATTR_BOLD;
		} else if (!strcasecmp(option, "dim")) {
			cell_style.attr |= CELL_ATTR_DIM;
		} else if (!strcasecmp(option, "notdim")) {
			cell_style.attr &= ~CELL_ATTR_DIM;
		} else if (!strcasecmp(option, "italics")) {
			cell_style.attr |= CELL_ATTR_ITALIC;
		} else if (!strcasecmp(option, "notitalics")) {
			cell_style.attr &= ~CELL_ATTR_ITALIC;
		} else if (!strcasecmp(option, "underlined")) {
			cell_style.attr |= CELL_ATTR_UNDERLINE;
		} else if (!strcasecmp(option, "notunderlined")) {
			cell_style.attr &= ~CELL_ATTR_UNDERLINE;
		} else if (!strcasecmp(option, "blink")) {
			cell_style.attr |= CELL_ATTR_BLINK;
		} else if (!strcasecmp(option, "notblink")) {
			cell_style.attr &= ~CELL_ATTR_BLINK;
		} else if (!strcasecmp(option, "fore")) {
			color_fromstring(&win->vix->ui, &cell_style.fg, value);
		} else if (!strcasecmp(option, "back")) {
			color_fromstring(&win->vix->ui, &cell_style.bg, value);
		}
		option = next;
	}
	tui->styles[win->id * UI_STYLE_MAX + id] = cell_style;
	if (win->id != 0) {
		tui->styles[id] = cell_style;
	}
	free(style_copy);
	return true;
}

static void ui_draw_line(Ui *tui, int x, int y, char c, int win_id, enum UiStyle style_id) {
	if (x < 0 || x >= tui->width || y < 0 || y >= tui->height) {
		return;
	}
	CellStyle style = tui->styles[win_id * UI_STYLE_MAX + style_id];
	Cell *cells = tui->cells + y * tui->width;
	while (x < tui->width) {
		cells[x].data[0] = c;
		cells[x].data[1] = '\0';
		cells[x].style = style;
		x++;
	}
}

static void ui_draw_string(Ui *tui, int x, int y, int max_x, const char *str, int win_id, enum UiStyle style_id) {
	debug("draw-string: [%d][%d]\n", y, x);
	if (x < 0 || x >= tui->width || y < 0 || y >= tui->height) {
		return;
	}
	if (max_x < 0 || max_x > tui->width) {
		max_x = tui->width;
	}

	/* NOTE: the style that style_id refers to may contain unset values; we need to properly
	 * clear the cell first then go through ui_window_style_set to get the correct style */
	CellStyle default_style = tui->styles[UI_STYLE_MAX * win_id + UI_STYLE_DEFAULT];
	// FIXME: does not handle double width characters etc, share code with view.c?
	Cell *cells = tui->cells + y * tui->width;
	const size_t cell_size = sizeof(cells[0].data)-1;
	for (const char *next = str; *str && x < max_x; str = next) {
		do next++; while (!ISUTF8(*next));
		size_t len = next - str;
		if (!len) {
			break;
		}
		len = MIN(len, cell_size);
		unsigned char ch = (unsigned char)*str;
		if (len == 1 && (ch < 32 || ch == 127)) {
			memcpy(cells[x].data, " ", 1);
			cells[x].data[1] = '\0';
		} else {
			size_t n = MIN(len, cell_size);
			memcpy(cells[x].data, str, n);
			cells[x].data[n] = '\0';
		}
		cells[x].style = default_style;
		ui_window_style_set(tui, win_id, cells + x++, style_id, false);
	}
}

static void ui_window_draw(Win *win) {
	if (win->vix->headless) {
		return;
	}
	Ui *ui = &win->vix->ui;
	View *view = &win->view;
	const Line *line = win->view.topline;

	bool status  = win->options & UI_OPTION_STATUSBAR;
	bool nu      = win->options & UI_OPTION_LINE_NUMBERS_ABSOLUTE;
	bool rnu     = win->options & UI_OPTION_LINE_NUMBERS_RELATIVE;
	bool sidebar = nu || rnu;

	int width = win->width, height = win->height;
	int sidebar_width = sidebar ? snprintf(NULL, 0, "%zd ", line->lineno + height - 2) : 0;
	if (sidebar_width != win->sidebar_width) {
		view_resize(view, width - sidebar_width, status ? height - 1 : height);
		win->sidebar_width = sidebar_width;
	}
	vix_window_draw(win);

	Selection *sel = view_selections_primary_get(view);
	size_t prev_lineno = 0, cursor_lineno = sel->line->lineno;
	char buf[(sizeof(size_t) * CHAR_BIT + 2) / 3 + 1 + 1];
	int x = win->x, y = win->y;
	int view_width = view->width;
	
	if (x + sidebar_width + view_width > ui->width) {
		view_width = ui->width - x - sidebar_width;
	}
	if (view_width < 0) view_width = 0;

	int max_y = win->y + height;
	if (status) {
		max_y--;
	}

	for (const Line *l = line; l && y < max_y; l = l->next, y++) {
		if (y < 0 || y >= ui->height) continue;
		
		if (sidebar) {
			if (!l->lineno || !l->len || l->lineno == prev_lineno) {
				memset(buf, ' ', sizeof(buf));
				buf[sidebar_width] = '\0';
			} else {
				size_t number = l->lineno;
				if (rnu) {
					number = (win->options & UI_OPTION_LARGE_FILE) ? 0 : l->lineno;
					if (l->lineno > cursor_lineno) {
						number = l->lineno - cursor_lineno;
					} else if (l->lineno < cursor_lineno) {
						number = cursor_lineno - l->lineno;
					}
				}
				snprintf(buf, sizeof buf, "%*zu ", sidebar_width-1, number);
			}
			ui_draw_string(ui, x, y, x + sidebar_width, buf, win->id,
				       (l->lineno == cursor_lineno) ? UI_STYLE_LINENUMBER_CURSOR :
				                                      UI_STYLE_LINENUMBER);
			prev_lineno = l->lineno;
		}
		
		if (x + sidebar_width < ui->width && view_width > 0) {
			int draw_x = x + sidebar_width;
			int draw_w = view_width;
			if (draw_x < 0) {
				draw_w += draw_x;
				draw_x = 0;
			}
			if (draw_x + draw_w > ui->width) {
				draw_w = ui->width - draw_x;
			}
			if (draw_w > 0) {
				Cell *dest = ui->cells + y * ui->width + draw_x;
				memcpy(dest, l->cells, sizeof(Cell) * draw_w);
			}
		}
	}
}

void ui_window_style_set(Ui *tui, int win_id, Cell *cell, enum UiStyle id, bool keep_non_default) {
	if (tui->vix->headless) {
		return;
	}
	CellStyle set = tui->styles[win_id * UI_STYLE_MAX + id];

	if (id != UI_STYLE_DEFAULT) {
		if (keep_non_default) {
			CellStyle default_style = tui->styles[win_id * UI_STYLE_MAX + UI_STYLE_DEFAULT];
			if (!cell_color_equal(cell->style.fg, default_style.fg)) {
				set.fg = cell->style.fg;
			}
			if (!cell_color_equal(cell->style.bg, default_style.bg)) {
				set.bg = cell->style.bg;
			}
		}
		set.fg = is_default_fg(set.fg)? cell->style.fg : set.fg;
		set.bg = is_default_bg(set.bg)? cell->style.bg : set.bg;
		set.attr = cell->style.attr | set.attr;
	}

	cell->style = set;
}

bool ui_window_style_set_pos(Win *win, int x, int y, enum UiStyle id, bool keep_non_default) {
	Ui *tui = &win->vix->ui;
	if (x < 0 || y < 0 || y >= win->height || x >= win->width) {
		return false;
	}
	Cell *cell = CELL_AT_POS(tui, win->x + x, win->y + y);
	ui_window_style_set(tui, win->id, cell, id, keep_non_default);
	return true;
}

void ui_window_status(Win *win, const char *status) {
	if (!(win->options & UI_OPTION_STATUSBAR)) {
		return;
	}
	Ui *ui = &win->vix->ui;
	enum UiStyle style = (ui->seltab && ui->seltab->selwin == win) ? UI_STYLE_STATUS_FOCUSED : UI_STYLE_STATUS;
	ui_draw_string(ui, win->x, win->y + win->height - 1, win->x + win->width, status, win->id, style);
}

void ui_arrange(Ui *tui, enum UiLayout layout) {
	debug("ui-arrange\n");
	if (!tui->seltab) return;
	TabPage *tab = tui->seltab;
	tab->layout = layout;
	
	int n = 0, m = !!tui->info[0], x = 0, y = 0;

	long total_weight = 0;
	for (Win *win = tab->windows; win; win = win->next) {
		if (win->options & UI_OPTION_ONELINE) {
			m++;
		} else {
			n++;
			total_weight += MAX(1, win->weight);
		}
	}

	bool show_tabs = (tui->tabview && n > 1) || (!tui->tabview && tui->tabpages && tui->tabpages->next);
	if (show_tabs) {
		y = 1; /* Reserve first line for tabs */
		m++;
	}

	if (tui->tabview && tab->selwin) {
		for (Win *win = tab->windows; win; win = win->next) {
			if (win == tab->selwin) {
				ui_window_resize(win, tui->width, tui->height - m);
				ui_window_move(win, 0, y);
			} else {
				ui_window_resize(win, 0, 0);
				ui_window_move(win, -1, -1);
			}
		}
		y = tui->height - m + (show_tabs ? 1 : 0);
		for (Win *win = tab->windows; win; win = win->next) {
			if (win->options & UI_OPTION_ONELINE) {
				ui_window_resize(win, tui->width, 1);
				ui_window_move(win, 0, y++);
			}
		}
		return;
	}

	int max_height = tui->height - m;
	if (max_height <= 0 || n == 0) return;

	int windows_left = n;
	int total_dim = (layout == UI_LAYOUT_HORIZONTAL) ? max_height : (tui->width - (n > 1 ? n - 1 : 0));
	int allocated_dim = 0;

	for (Win *win = tab->windows; win; win = win->next) {
		if (win->options & UI_OPTION_ONELINE) {
			continue;
		}
		windows_left--;

		int size;
		if (windows_left == 0) {
			size = total_dim - allocated_dim;
		} else {
			size = (int)((long)total_dim * win->weight / total_weight);
			if (size < 1) size = 1;
			int remaining_room = total_dim - allocated_dim - windows_left;
			if (size > remaining_room) size = remaining_room;
		}
		if (size < 1) size = 1;
		allocated_dim += size;

		if (layout == UI_LAYOUT_HORIZONTAL) {
			ui_window_resize(win, tui->width, size);
			ui_window_move(win, x, y);
			y += size;
		} else {
			ui_window_resize(win, size, max_height);
			ui_window_move(win, x, y);
			x += size;

			if (windows_left > 0 && x >= 0 && x < tui->width) {
				for (int i = y; i < y + max_height; i++) {
					if (i >= 0 && i < tui->height) {
						int idx = i * tui->width + x;
						if (idx >= 0 && idx < tui->width * tui->height) {
							Cell *cell = &tui->cells[idx];
							memcpy(cell->data, "│", 3);
							cell->data[3] = '\0';
							cell->style = tui->styles[UI_STYLE_SEPARATOR];
						}
					}
				}
				x++;
			}
		}
	}

	if (layout == UI_LAYOUT_VERTICAL) {
		y = tui->height - m + (show_tabs ? 1 : 0);
	}

	for (Win *win = tab->windows; win; win = win->next) {
		if (!(win->options & UI_OPTION_ONELINE)) {
			continue;
		}
		ui_window_resize(win, tui->width, 1);
		ui_window_move(win, 0, y++);
	}
}

static void ui_tab_draw(Ui *ui) {
	if (!ui->tabpages) return;
	
	int win_id = 0;
	if (ui->seltab->selwin) {
		Win *w = ui->seltab->selwin;
		if (w->options & UI_OPTION_ONELINE) {
			for (Win *win = ui->seltab->windows; win; win = win->next) {
				if (!(win->options & UI_OPTION_ONELINE)) {
					w = win;
					break;
				}
			}
		}
		win_id = w->id;
	}
	int x = 0;

	int n = 0;
	if (ui->tabview) {
		for (Win *win = ui->seltab->windows; win; win = win->next) {
			if (!(win->options & UI_OPTION_ONELINE)) n++;
		}
	}

	if (ui->tabview && n > 1) {
		/* Show windows of the current TabPage as tabs, ignoring internal oneline windows */
		for (Win *win = ui->seltab->windows; win; win = win->next) {
			if (win->options & UI_OPTION_ONELINE) continue;
			char name[64];
			const char *filename = win->file->name ? strrchr(win->file->name, '/') : NULL;
			filename = filename ? filename + 1 : (win->file->name ? win->file->name : "[No Name]");
			int len = snprintf(name, sizeof(name), " %s ", filename);
			enum UiStyle style = (win == ui->seltab->selwin) ? UI_STYLE_TAB_FOCUSED : UI_STYLE_TAB;
			ui_draw_string(ui, x, 0, ui->width, name, win->id, style);
			x += len;
			if (x >= ui->width) break;
		}
	} else if (!ui->tabview && ui->tabpages->next) {
		/* Show TabPages as tabs */
		for (TabPage *tab = ui->tabpages; tab; tab = tab->next) {
			char name[64];
			const char *filename = "[No Name]";
			int tid = 0;
			if (tab->selwin && tab->selwin->file->name) {
				filename = strrchr(tab->selwin->file->name, '/');
				filename = filename ? filename + 1 : tab->selwin->file->name;
				tid = tab->selwin->id;
			}
			int len = snprintf(name, sizeof(name), " %s ", filename);
			enum UiStyle style = (tab == ui->seltab) ? UI_STYLE_TAB_FOCUSED : UI_STYLE_TAB;
			ui_draw_string(ui, x, 0, ui->width, name, tid, style);
			x += len;
			if (x >= ui->width) break;
		}
	} else {
		return;
	}

	if (x < ui->width) {
		ui_draw_line(ui, x, 0, ' ', win_id, UI_STYLE_TAB);
	}
}

void ui_draw(Ui *tui) {
	if (tui->vix->headless || !tui->seltab) {
		return;
	}
	debug("ui-draw\n");
	ui_arrange(tui, tui->seltab->layout);
	ui_tab_draw(tui);

	int dx = 0, dy = 0, parent_height = 0;
	for (Win *win = tui->seltab->windows; win; win = win->next) {
		ui_window_draw(win);
		/* determine primary cursor's position */
		View *view = &win->view;
		if (win == tui->seltab->selwin) {
			view_coord_get(view, view_cursor_get(view), NULL, &tui->cur_row, &tui->cur_col);
			if (win->parent) {
				parent_height = win->parent->height;
			} else {
				tui->cur_col += win->sidebar_width + dx;
				tui->cur_row += dy;
			}
		}
		if (tui->seltab->layout == UI_LAYOUT_HORIZONTAL) {
			dy += win->height;
		} else {
			dx += win->width + 1; /* +1 for '|' separator */
		}
	}

	switch (tui->vix->prompt_state) {
	case PROMPTSTATE_NONE:
	case PROMPTSTATE_MULTILINE:
		break;
	case PROMPTSTATE_ONELINE:
	case PROMPTSTATE_COMMAND:
		if (tui->seltab->layout == UI_LAYOUT_HORIZONTAL) {
			tui->cur_row = dy - 1;
		} else {
			tui->cur_row = parent_height;
		}
		break;
	}

	if (tui->info[0]) {
		ui_draw_line(tui, 0, tui->height-1, ' ', 0, UI_STYLE_INFO);
		ui_draw_string(tui, 0, tui->height-1, tui->width, tui->info, 0, UI_STYLE_INFO);
	}
	vix_event_emit(tui->vix, VIX_EVENT_UI_DRAW);
	ui_term_backend_blit(tui);
}

void ui_redraw(Ui *tui) {
	if (!tui->seltab) return;
	ui_term_backend_clear(tui);
	for (Win *win = tui->seltab->windows; win; win = win->next) {
		win->view.need_update = true;
	}
}

void ui_resize(Ui *tui) {
	struct winsize ws;
	int width = 80, height = 24;

	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) != -1) {
		if (ws.ws_col > 0) {
			width = ws.ws_col;
		}
		if (ws.ws_row > 0) {
			height = ws.ws_row;
		}
	}

	width  = MIN(width,  UI_MAX_WIDTH);
	height = MIN(height, UI_MAX_HEIGHT);
	if (!ui_term_backend_resize(tui, width, height)) {
		return;
	}

	size_t size = width*height*sizeof(Cell);
	if (size > tui->cells_size) {
		Cell *cells = realloc(tui->cells, size);
		if (!cells) {
			return;
		}
		memset((char*)cells+tui->cells_size, 0, size - tui->cells_size);
		tui->cells_size = size;
		tui->cells = cells;
	}
	tui->width = width;
	tui->height = height;
}

void ui_window_release(Ui *tui, Win *win) {
	if (!win || !tui->seltab) {
		return;
	}
	TabPage *tab = tui->seltab;
	if (tab->windows == win) {
		tab->windows = win->next;
		tui->vix->windows = win->next;
	}
	if (win->next) {
		win->next->prev = win->prev;
	}
	if (win->prev) {
		win->prev->next = win->next;
	}
	if (tab->selwin == win) {
		tab->selwin = win->next ? win->next : win->prev;
		tui->vix->win = tab->selwin;
	}
	tui->ids &= ~(1UL << win->id);

	/* If this was the last window in the tab, close the tab */
	if (!tab->windows && (tui->tabpages->next)) {
		if (tab->prev) tab->prev->next = tab->next;
		if (tab->next) tab->next->prev = tab->prev;
		if (tui->tabpages == tab) tui->tabpages = tab->next;
		
		TabPage *target = tab->prev ? tab->prev : tab->next;
		tui->seltab = target;
		tui->vix->windows = target->windows;
		tui->vix->win = target->selwin;
		
		free(tab);
		ui_draw(tui);
	}
}

void ui_window_focus(Win *new) {
	if (!new->vix->ui.seltab) return;
	Win *old = new->vix->ui.seltab->selwin;
	if ((new->options & UI_OPTION_STATUSBAR) || (new->options & UI_OPTION_ONELINE)) {
		new->vix->ui.seltab->selwin = new;
	}
	if (old) {
		old->view.need_update = true;
	}
	new->view.need_update = true;
}

void ui_window_options_set(Win *win, enum UiOption options) {
	win->options = options;
	if (options & UI_OPTION_ONELINE) {
		/* move the new window to the end of the list */
		Ui *tui = &win->vix->ui;
		if (tui->seltab && win->next) {
			TabPage *tab = tui->seltab;
			if (tab->windows == win) {
				tab->windows = win->next;
			}
			if (win->prev) {
				win->prev->next = win->next;
			}
			if (win->next) {
				win->next->prev = win->prev;
			}
			Win *last = tab->windows;
			while (last->next) {
				last = last->next;
			}
			last->next = win;
			win->prev = last;
			win->next = NULL;
		}
	}
	ui_draw(&win->vix->ui);
}

void ui_window_swap(Win *a, Win *b) {
	if (a == b || !a || !b || !a->vix->ui.seltab) {
		return;
	}
	Ui *tui = &a->vix->ui;
	TabPage *tab = tui->seltab;
	if (tab->windows == a) {
		tab->windows = b;
	} else if (tab->windows == b) {
		tab->windows = a;
	}
	if (tab->selwin == a) {
		ui_window_focus(b);
	} else if (tab->selwin == b) {
		ui_window_focus(a);
	}
}

bool ui_window_init(Ui *tui, Win *w, enum UiOption options) {
	if (tui->vix->headless) {
		win_options_set(w, options & UI_OPTION_ONELINE);
		return true;
	}
	/* get rightmost zero bit, i.e. highest available id */
	size_t bit = ~tui->ids & (tui->ids + 1);
	size_t id = 0;
	for (size_t tmp = bit; tmp >>= 1; id++);
	if (id >= sizeof(size_t) * 8) {
		return false;
	}
	size_t styles_size = (id + 1) * UI_STYLE_MAX * sizeof(CellStyle);
	if (styles_size > tui->styles_size) {
		CellStyle *styles = realloc(tui->styles, styles_size);
		if (!styles) {
			return false;
		}
		tui->styles = styles;
		tui->styles_size = styles_size;
	}

	tui->ids |= bit;
	w->id = id;

	CellStyle *styles = &tui->styles[w->id * UI_STYLE_MAX];
	for (int i = 0; i < UI_STYLE_MAX; i++) {
		styles[i] = CELL_STYLE_DEFAULT;
	}

	styles[UI_STYLE_CURSOR].attr |= CELL_ATTR_REVERSE;
	styles[UI_STYLE_CURSOR_PRIMARY].attr |= CELL_ATTR_REVERSE|CELL_ATTR_BLINK;
	styles[UI_STYLE_SELECTION].attr |= CELL_ATTR_REVERSE;
	styles[UI_STYLE_COLOR_COLUMN].attr |= CELL_ATTR_REVERSE;
	styles[UI_STYLE_STATUS].attr |= CELL_ATTR_REVERSE;
	styles[UI_STYLE_STATUS_FOCUSED].attr |= CELL_ATTR_REVERSE|CELL_ATTR_BOLD;
	styles[UI_STYLE_TAB].attr |= CELL_ATTR_REVERSE;
	styles[UI_STYLE_TAB_FOCUSED].attr |= CELL_ATTR_REVERSE|CELL_ATTR_BOLD;
	styles[UI_STYLE_INFO].attr |= CELL_ATTR_BOLD;

	if (text_size(w->file->text) > UI_LARGE_FILE_SIZE) {
		options |= UI_OPTION_LARGE_FILE;
		options &= ~UI_OPTION_LINE_NUMBERS_ABSOLUTE;
	}

	win_options_set(w, options);

	return true;
}

void ui_info_show(Ui *tui, const char *msg, va_list ap) {
	ui_draw_line(tui, 0, tui->height-1, ' ', 0, UI_STYLE_INFO);
	vsnprintf(tui->info, sizeof(tui->info), msg, ap);
}

void ui_info_hide(Ui *tui) {
	if (tui->info[0]) {
		tui->info[0] = '\0';
	}
}

static TermKey *ui_termkey_new(int fd) {
	TermKey *termkey = termkey_new(fd, UI_TERMKEY_FLAGS);
	if (termkey) {
		termkey_set_canonflags(termkey, TERMKEY_CANON_DELBS);
	}
	return termkey;
}

static TermKey *ui_termkey_reopen(Ui *ui, int fd) {
	int tty = open("/dev/tty", O_RDWR);
	if (tty == -1) {
		return NULL;
	}
	if (tty != fd && dup2(tty, fd) == -1) {
		close(tty);
		return NULL;
	}
	close(tty);
	return ui_termkey_new(fd);
}

void ui_terminal_suspend(Ui *tui) {
	ui_term_backend_suspend(tui);
	kill(0, SIGTSTP);
}

bool ui_getkey(Ui *tui, TermKeyKey *key) {
	TermKeyResult ret = termkey_getkey(tui->termkey, key);

	if (ret == TERMKEY_RES_EOF) {
		termkey_destroy(tui->termkey);
		errno = 0;
		if (!(tui->termkey = ui_termkey_reopen(tui, STDIN_FILENO))) {
			ui_die_msg(tui, "Failed to re-open stdin as /dev/tty: %s\n", errno != 0 ? strerror(errno) : "");
		}
		return false;
	}

	if (ret == TERMKEY_RES_AGAIN) {
		struct pollfd fd;
		fd.fd = STDIN_FILENO;
		fd.events = POLLIN;
		if (poll(&fd, 1, termkey_get_waittime(tui->termkey)) == 0) {
			ret = termkey_getkey_force(tui->termkey, key);
		}
	}

	return ret == TERMKEY_RES_KEY;
}

void ui_terminal_save(Ui *tui, bool fscr) {
	ui_term_backend_save(tui, fscr);
	termkey_stop(tui->termkey);
}

void ui_terminal_restore(Ui *tui) {
	termkey_start(tui->termkey);
	ui_term_backend_restore(tui);
}

bool ui_init(Ui *tui, Vix *vix) {
	tui->vix = vix;

	setlocale(LC_CTYPE, "");

	char *term = getenv("TERM");
	if (!term) {
		term = "xterm";
		setenv("TERM", term, 1);
	}

	errno = 0;
	if (vix->headless) {
		tui->termkey = termkey_new_abstract(term, UI_TERMKEY_FLAGS);
	} else if (!(tui->termkey = ui_termkey_new(STDIN_FILENO))) {
		/* work around libtermkey bug which fails if stdin is /dev/null */
		if (errno == EBADF) {
			errno = 0;
			if (!(tui->termkey = ui_termkey_reopen(tui, STDIN_FILENO)) && errno == ENXIO) {
				tui->termkey = termkey_new_abstract(term, UI_TERMKEY_FLAGS);
			}
		}
	}

	if (!tui->termkey) {
		goto err;
	}

	if (vix->headless) {
		tui->width = 80;
		tui->height = 24;
		return true;
	}

	if (!ui_term_backend_init(tui, term)) {
		goto err;
	}
	ui_resize(tui);

	/* Initialize the first tab page */
	TabPage *tab = calloc(1, sizeof(TabPage));
	if (!tab) goto err;
	tab->layout = UI_LAYOUT_HORIZONTAL;
	tui->tabpages = tui->seltab = tab;
	
	return true;
err:
	ui_die_msg(tui, "Failed to start curses interface: %s\n", errno != 0 ? strerror(errno) : "");
	return false;
}

bool ui_terminal_init(Ui *tui) {
	size_t styles_size = UI_STYLE_MAX * sizeof(CellStyle);
	CellStyle *styles = calloc(1, styles_size);
	if (!styles) {
		return false;
	}
	if (!ui_backend_init(tui)) {
		free(styles);
		return false;
	}
	tui->styles_size = styles_size;
	tui->styles = styles;
	tui->doupdate = true;
	tui->backend_data = NULL;
	tui->is_tty = isatty(STDIN_FILENO);
	return true;
}

void ui_tab_new(Ui *ui) {
	TabPage *new_tab = calloc(1, sizeof(TabPage));
	if (!new_tab) return;
	new_tab->layout = ui->layout;
	
	/* Link new tab */
	new_tab->next = ui->seltab->next;
	new_tab->prev = ui->seltab;
	if (ui->seltab->next) ui->seltab->next->prev = new_tab;
	ui->seltab->next = new_tab;
	
	/* Save current state */
	ui->seltab->selwin = ui->vix->win;
	ui->seltab->windows = ui->vix->windows;
	
	/* Switch to new tab */
	ui->seltab = new_tab;
	
	/* Create a default window in the new tab */
	Win *old_win = ui->vix->win;
	Win *new_win = window_new_file(ui->vix, old_win ? old_win->file : NULL, UI_OPTION_STATUSBAR);
	if (new_win) {
		ui->seltab->windows = ui->vix->windows = new_win;
		ui->seltab->selwin = ui->vix->win = new_win;
	}
	
	ui_draw(ui);
}

void ui_tab_next(Ui *ui) {
	if (!ui->tabpages || !ui->seltab) return;
	TabPage *next = ui->seltab->next ? ui->seltab->next : ui->tabpages;
	if (next == ui->seltab) return;
	
	/* Save current state */
	ui->seltab->selwin = ui->vix->win;
	ui->seltab->windows = ui->vix->windows;
	
	/* Switch to next tab */
	ui->seltab = next;
	ui->vix->win = next->selwin;
	ui->vix->windows = next->windows;
	
	ui_draw(ui);
}

void ui_tab_prev(Ui *ui) {
	if (!ui->tabpages || !ui->seltab) return;
	TabPage *prev = ui->seltab->prev;
	if (!prev) {
		prev = ui->tabpages;
		while (prev->next) prev = prev->next;
	}
	if (prev == ui->seltab) return;
	
	/* Save current state */
	ui->seltab->selwin = ui->vix->win;
	ui->seltab->windows = ui->vix->windows;
	
	/* Switch to prev tab */
	ui->seltab = prev;
	ui->vix->win = prev->selwin;
	ui->vix->windows = prev->windows;
	
	ui_draw(ui);
}

void ui_terminal_free(Ui *tui) {
	if (!tui) {
		return;
	}
	TabPage *tab = tui->tabpages;
	while (tab) {
		TabPage *next = tab->next;
		tui->seltab = tab;
		while (tab->windows) {
			ui_window_release(tui, tab->windows);
		}
		free(tab);
		tab = next;
	}
	ui_term_backend_free(tui);
	if (tui->termkey) {
		termkey_destroy(tui->termkey);
		tui->termkey = NULL;
	}
	free(tui->cells);
	free(tui->styles);
}
