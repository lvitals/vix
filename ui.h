#ifndef UI_H
#define UI_H

/* enable large file optimization for files larger than: */
#define UI_LARGE_FILE_SIZE (1 << 25)
/* enable large file optimization for files containing lines longer than: */
#define UI_LARGE_FILE_LINE_SIZE (1 << 16)

#define UI_MAX_WIDTH  1024
#define UI_MAX_HEIGHT 1024

enum UiLayout {
	UI_LAYOUT_HORIZONTAL,
	UI_LAYOUT_VERTICAL,
};

enum UiOption {
	UI_OPTION_NONE = 0,
	UI_OPTION_LINE_NUMBERS_ABSOLUTE = 1 << 0,
	UI_OPTION_LINE_NUMBERS_RELATIVE = 1 << 1,
	UI_OPTION_SYMBOL_SPACE = 1 << 2,
	UI_OPTION_SYMBOL_TAB = 1 << 3,
	UI_OPTION_SYMBOL_TAB_FILL = 1 << 4,
	UI_OPTION_SYMBOL_EOL = 1 << 5,
	UI_OPTION_SYMBOL_EOF = 1 << 6,
	UI_OPTION_CURSOR_LINE = 1 << 7,
	UI_OPTION_STATUSBAR = 1 << 8,
	UI_OPTION_ONELINE = 1 << 9,
	UI_OPTION_LARGE_FILE = 1 << 10,
};

enum UiStyle {
	UI_STYLE_LEXER_MAX = 64,
	UI_STYLE_DEFAULT,
	UI_STYLE_CURSOR,
	UI_STYLE_CURSOR_PRIMARY,
	UI_STYLE_CURSOR_LINE,
	UI_STYLE_SELECTION,
	UI_STYLE_LINENUMBER,
	UI_STYLE_LINENUMBER_CURSOR,
	UI_STYLE_COLOR_COLUMN,
	UI_STYLE_STATUS,
	UI_STYLE_STATUS_FOCUSED,
	UI_STYLE_TAB,
	UI_STYLE_TAB_FOCUSED,
	UI_STYLE_SEPARATOR,
	UI_STYLE_INFO,
	UI_STYLE_EOF,
	UI_STYLE_WHITESPACE,
	UI_STYLE_MAX,
};

#if CONFIG_CURSES
typedef uint64_t CellAttr;
typedef short CellColor;
#else
typedef uint8_t CellAttr;
typedef struct {
	uint8_t r, g, b;
	uint8_t index;
} CellColor;
#endif

typedef struct {
	CellAttr attr;
	CellColor fg, bg;
} CellStyle;

typedef struct {
	char data[16];      /* utf8 encoded character displayed in this cell (might be more than
	                       one Unicode codepoint. might also not be the same as in the
	                       underlying text, for example tabs get expanded */
	size_t len;         /* number of bytes the character displayed in this cell uses, for
	                       characters which use more than 1 column to display, their length
	                       is stored in the leftmost cell whereas all following cells
	                       occupied by the same character have a length of 0. */
	int width;          /* display width i.e. number of columns occupied by this character */
	CellStyle style;    /* colors and attributes used to display this cell */
} Cell;

struct Win;
struct Vix;

typedef struct TabPage TabPage;
struct TabPage {
	struct Win *windows;      /* windows belonging to this tab */
	struct Win *selwin;       /* focused window in this tab */
	enum UiLayout layout;     /* layout for this tab's windows */
	TabPage *next, *prev;     /* linked list pointers */
};

typedef struct {
	struct Vix *vix;          /* editor instance to which this ui belongs */
	TabPage *tabpages;        /* all tab pages managed by this ui */
	TabPage *seltab;          /* the currently active tab page */
	char info[UI_MAX_WIDTH];  /* info message displayed at the bottom of the screen */
	int width, height;        /* terminal dimensions available for all windows */
	int cur_row, cur_col;     /* active cursor's (0-based) position on the terminal */
	enum UiLayout layout;     /* default layout for new tabs */
	TermKey *termkey;         /* libtermkey instance to handle keyboard input (stdin or /dev/tty) */
	size_t ids;               /* bit mask of in use window ids */
	size_t styles_size;       /* #bytes allocated for styles array */
	CellStyle *styles;        /* each window has UI_STYLE_MAX different style definitions */
	size_t cells_size;        /* #bytes allocated for 2D grid (grows only) */
	Cell *cells;              /* 2D grid of cells, at least as large as current terminal size */
	bool doupdate;            /* Whether to update the screen after refreshing contents */
	void *ctx;                /* Any additional data needed by the backend */
	void *backend_data;       /* Any additional persistent data needed by the backend */
	bool layout_only;         /* Whether to skip expensive view operations during arrangement */
	bool tabview;             /* Whether to show only the focused window (tabbed mode) */
	TabPage *tab_view_offset; /* first visible tab in tab bar */
	Win *win_view_offset;     /* first visible window in tab bar */
	bool is_tty;              /* Whether the input is a TTY */
} Ui;

#include "view.h"
#include "vix.h"
#include "text.h"

VIX_INTERNAL bool ui_terminal_init(Ui*);
VIX_INTERNAL int  ui_terminal_colors(void);
VIX_INTERNAL void ui_terminal_free(Ui*);
VIX_INTERNAL void ui_terminal_restore(Ui*);
VIX_INTERNAL void ui_terminal_resume(Ui*);
VIX_INTERNAL void ui_terminal_save(Ui*, bool fscr);
VIX_INTERNAL void ui_terminal_suspend(Ui*);

VIX_INTERNAL void ui_die(Ui *, const char *, va_list);
VIX_INTERNAL bool ui_init(Ui *, Vix *);
VIX_INTERNAL void ui_arrange(Ui*, enum UiLayout);
VIX_INTERNAL void ui_draw(Ui*);
VIX_INTERNAL void ui_info_hide(Ui *);
VIX_INTERNAL void ui_info_show(Ui *, const char *, va_list);
VIX_INTERNAL void ui_redraw(Ui*);
VIX_INTERNAL void ui_resize(Ui*);

VIX_INTERNAL bool ui_window_init(Ui *, Win *, enum UiOption);
VIX_INTERNAL void ui_window_focus(Win *);
/* removes a window from the list of open windows */
VIX_INTERNAL void ui_window_release(Ui *, Win *);
VIX_INTERNAL void ui_window_swap(Win *, Win *);

VIX_INTERNAL void ui_tab_new(Ui*);
VIX_INTERNAL void ui_tab_next(Ui*);
VIX_INTERNAL void ui_tab_prev(Ui*);

VIX_INTERNAL bool ui_getkey(Ui *, TermKeyKey *);

VIX_INTERNAL bool ui_style_define(Win *win, int id, const char *style);
VIX_INTERNAL void ui_window_style_set(Ui *ui, int win_id, Cell *cell, enum UiStyle id, bool keep_non_default);
VIX_INTERNAL bool ui_window_style_set_pos(Win *win, int x, int y, enum UiStyle id, bool keep_non_default);

VIX_INTERNAL void ui_window_options_set(Win *win, enum UiOption options);
VIX_INTERNAL void ui_window_status(Win *win, const char *status);

VIX_INTERNAL int ui_terminal_colors(void);

#endif
