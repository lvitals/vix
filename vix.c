#include "util.h"

#include <termkey.h>

#include "vix.h"
#include "text-util.h"
#include "text-motions.h"
#include "text-objects.h"
#include "vix-core.h"
#include "sam.h"
#include "ui.h"
#include "vix-subprocess.h"

#include "util.c"

#include "buffer.c"
#include "event-basic.c"
#include "map.c"
#include "sam.c"
#include "text.c"
#include "ui-terminal.c"
#include "view.c"
#include "vix-lua.c"
#include "vix-marks.c"
#include "vix-modes.c"
#include "vix-motions.c"
#include "vix-operators.c"
#include "vix-prompt.c"
#include "vix-registers.c"
#include "vix-subprocess.c"
#include "vix-text-objects.c"

/** window / file handling */

static void file_free(Vix *vix, File *file) {
	if (!file) {
		return;
	}
	if (file->refcount > 1) {
		--file->refcount;
		return;
	}
	vix_event_emit(vix, VIX_EVENT_FILE_CLOSE, file);
	for (size_t i = 0; i < LENGTH(file->marks); i++) {
		da_release(file->marks + i);
	}
	text_free(file->text);
	free((char*)file->name);

	if (file->prev) {
		file->prev->next = file->next;
	}
	if (file->next) {
		file->next->prev = file->prev;
	}
	if (vix->files == file) {
		vix->files = file->next;
	}
	free(file);
}

static File *file_new_text(Vix *vix, Text *text) {
	File *file = calloc(1, sizeof(*file));
	if (!file) {
		return NULL;
	}
	file->fd = -1;
	file->text = text;
	file->stat = text_stat(text);
	if (vix->files) {
		vix->files->prev = file;
	}
	file->next = vix->files;
	vix->files = file;
	return file;
}

static File *file_new(Vix *vix, const char *name, bool internal) {
	char *name_absolute = NULL;
	bool cmp_names = 0;
	struct stat new;

	if (name) {
		if (!(name_absolute = absolute_path(name))) {
			return NULL;
		}

		if (stat(name_absolute, &new)) {
			if (errno != ENOENT) {
				free(name_absolute);
				return NULL;
			}
			cmp_names = 1;
		}

		File *existing = NULL;
		/* try to detect whether the same file is already open in another window */
		for (File *file = vix->files; file; file = file->next) {
			if (file->name) {
				if ((cmp_names && strcmp(file->name, name_absolute) == 0) ||
				    (file->stat.st_dev == new.st_dev && file->stat.st_ino == new.st_ino)) {
					existing = file;
					break;
				}
			}
		}
		if (existing) {
			free(name_absolute);
			return existing;
		}
	}

	File *file = NULL;
	Text *text = text_load_method(vix, name, vix->load_method);
	if (!text && name && errno == ENOENT) {
		text = text_load(vix, 0);
	}
	if (!text) {
		goto err;
	}
	if (!(file = file_new_text(vix, text))) {
		goto err;
	}
	file->name = name_absolute;
	file->internal = internal;
	if (!internal) {
		vix_event_emit(vix, VIX_EVENT_FILE_OPEN, file);
	}
	return file;
err:
	free(name_absolute);
	text_free(text);
	file_free(vix, file);
	return NULL;
}

static File *file_new_internal(Vix *vix, const char *filename) {
	File *file = file_new(vix, filename, true);
	if (file) {
		file->refcount = 1;
	}
	return file;
}

void file_name_set(File *file, const char *name) {
	if (name == file->name) {
		return;
	}
	free((char*)file->name);
	file->name = absolute_path(name);
}

const char *file_name_get(File *file) {
	/* TODO: calculate path relative to working directory, cache result */
	if (!file->name) {
		return NULL;
	}
	char cwd[PATH_MAX];
	if (!getcwd(cwd, sizeof cwd)) {
		return file->name;
	}
	const char *path = (strstr)(file->name, cwd);
	if (path != file->name) {
		return file->name;
	}
	size_t cwdlen = strlen(cwd);
	return file->name[cwdlen] == '/' ? file->name+cwdlen+1 : file->name;
}

void window_selection_save(Win *win)
{
	Vix *vix = win->vix;
	FilerangeList sel = view_selections_get_all(vix, &win->view);
	vix_mark_set(vix, win, VIX_MARK_SELECTION, sel);
	da_release(&sel);
	vix_jumplist_save(vix);
}

static void window_free(Win *win) {
	if (!win) {
		return;
	}
	Vix *vix = win->vix;
	for (Win *other = vix->windows; other; other = other->next) {
		if (other->parent == win) {
			other->parent = NULL;
		}
	}
	ui_window_release(&vix->ui, win);
	view_free(&win->view);
	for (size_t i = 0; i < LENGTH(win->modes); i++) {
		map_free(win->modes[i].bindings);
	}
	for (int i = 0; i < VIX_MARK_SET_LRU_COUNT; i++) {
		da_release(win->mark_set_lru_regions + i);
	}
	da_release(&win->saved_selections);
	free(win);
}

static void window_draw_colorcolumn(Win *win) {
	int cc = win->view.colorcolumn;
	if (cc <= 0) {
		return;
	}
	size_t lineno = 0;
	int line_cols = 0; /* Track the number of columns we've passed on each line */
	bool line_cc_set = false; /* Has the colorcolumn attribute been set for this line yet */
	int width = win->view.width;

	for (Line *l = win->view.topline; l; l = l->next) {
		if (l->lineno != lineno) {
			line_cols = 0;
			line_cc_set = false;
			if (!(lineno = l->lineno)) {
				break;
			}
		}
		if (line_cc_set) {
			continue;
		}

		/* This screen line contains the cell we want to highlight */
		if (cc <= line_cols + width) {
			ui_window_style_set(&win->vix->ui, win->id, &l->cells[cc - 1 - line_cols], UI_STYLE_COLOR_COLUMN, false);
			line_cc_set = true;
		} else {
			line_cols += width;
		}
	}
}

static void window_draw_cursorline(Win *win) {
	Vix *vix = win->vix;
	enum UiOption options = win->options;
	if (!(options & UI_OPTION_CURSOR_LINE)) {
		return;
	}
	if (vix->mode->visual || vix->win != win) {
		return;
	}
	if (win->view.selection_count > 1) {
		return;
	}

	int width = win->view.width;
	Selection *sel = view_selections_primary_get(&win->view);
	size_t lineno = sel->line->lineno;
	for (Line *l = win->view.topline; l; l = l->next) {
		if (l->lineno == lineno) {
			for (int x = 0; x < width; x++) {
				ui_window_style_set(&vix->ui, win->id, &l->cells[x], UI_STYLE_CURSOR_LINE, true);
			}
		} else if (l->lineno > lineno) {
			break;
		}
	}
}

static void window_draw_selection(Win *win, Selection *cur) {
	View *view = &win->view;
	Filerange sel = view_selections_get(cur);
	if (!text_range_valid(&sel)) {
		return;
	}
	Line *start_line; int start_col;
	Line *end_line; int end_col;
	view_coord_get(view, sel.start, &start_line, NULL, &start_col);
	view_coord_get(view, sel.end, &end_line, NULL, &end_col);
	if (!start_line && !end_line) {
		return;
	}
	if (!start_line) {
		start_line = view->topline;
		start_col = 0;
	}
	if (!end_line) {
		end_line = view->lastline;
		end_col = end_line->width;
	}
	for (Line *l = start_line; l != end_line->next; l = l->next) {
		int col = (l == start_line) ? start_col : 0;
		int end = (l == end_line) ? end_col : l->width;
		while (col < end) {
			ui_window_style_set(&win->vix->ui, win->id, &l->cells[col++], UI_STYLE_SELECTION, false);
		}
	}
}

static void window_draw_cursor_matching(Win *win, Selection *cur) {
	if (win->vix->mode->visual) {
		return;
	}
	Line *line_match; int col_match;
	size_t pos = view_cursors_pos(cur);
	Filerange limits = VIEW_VIEWPORT_GET(win->view);
	size_t pos_match = text_bracket_match_symbol(win->vix, win->file->text, pos, "(){}[]\"'`", &limits);
	if (pos == pos_match) {
		return;
	}
	if (!view_coord_get(&win->view, pos_match, &line_match, NULL, &col_match)) {
		return;
	}
	ui_window_style_set(&win->vix->ui, win->id, &line_match->cells[col_match], UI_STYLE_SELECTION, false);
}

static void window_draw_cursor(Win *win, Selection *cur) {
	if (win->vix->win != win) {
		return;
	}
	Line *line = cur->line;
	if (!line) {
		return;
	}
	Selection *primary = view_selections_primary_get(&win->view);
	ui_window_style_set(&win->vix->ui, win->id, &line->cells[cur->col], primary == cur ? UI_STYLE_CURSOR_PRIMARY : UI_STYLE_CURSOR, false);
	window_draw_cursor_matching(win, cur);
	return;
}

static void window_draw_selections(Win *win) {
	Filerange viewport = VIEW_VIEWPORT_GET(win->view);
	Selection *sel = view_selections_primary_get(&win->view);
	for (Selection *s = view_selections_prev(sel); s; s = view_selections_prev(s)) {
		window_draw_selection(win, s);
		size_t pos = view_cursors_pos(s);
		if (pos < viewport.start) {
			break;
		}
		window_draw_cursor(win, s);
	}
	window_draw_selection(win, sel);
	window_draw_cursor(win, sel);
	for (Selection *s = view_selections_next(sel); s; s = view_selections_next(s)) {
		window_draw_selection(win, s);
		size_t pos = view_cursors_pos(s);
		if (pos > viewport.end) {
			break;
		}
		window_draw_cursor(win, s);
	}
}

static void window_draw_eof(Win *win) {
	View *view = &win->view;
	if (view->width == 0) {
		return;
	}
	for (Line *l = view->lastline->next; l; l = l->next) {
		strncpy(l->cells[0].data, view->symbols[SYNTAX_SYMBOL_EOF], sizeof(l->cells[0].data)-1);
		ui_window_style_set(&win->vix->ui, win->id, l->cells, UI_STYLE_EOF, false);
	}
}

void vix_window_draw(Win *win) {
	if (win->vix->headless) {
		return;
	}
	if (!view_update(&win->view)) {
		return;
	}
	Vix *vix = win->vix;
	vix_event_emit(vix, VIX_EVENT_WIN_HIGHLIGHT, win);

	window_draw_colorcolumn(win);
	window_draw_cursorline(win);
	if (!vix->win || vix->win == win || vix->win->parent == win) {
		window_draw_selections(win);
	}
	window_draw_eof(win);

	vix_event_emit(vix, VIX_EVENT_WIN_STATUS, win);
}


void vix_window_invalidate(Win *win) {
	for (Win *w = win->vix->windows; w; w = w->next) {
		if (w->file == win->file) {
			view_draw(&w->view);
		}
	}
}

Win *window_new_file(Vix *vix, File *file, enum UiOption options) {
	Win *win = calloc(1, sizeof(Win));
	if (!win) {
		return NULL;
	}
	win->vix = vix;
	win->file = file;
	if (!view_init(win, file->text)) {
		free(win);
		return NULL;
	}
	win->expandtab = false;
	if (!ui_window_init(&vix->ui, win, options)) {
		window_free(win);
		return NULL;
	}

	file->refcount++;
	win_options_set(win, win->options);

	win->next = NULL;
	if (!vix->windows) {
		win->prev = NULL;
		vix->windows = win;
	} else {
		Win *last = vix->windows;
		while (last->next) {
			last = last->next;
		}
		last->next = win;
		win->prev = last;
	}
	vix->ui.windows = vix->windows;

	vix->win = win;
	ui_window_focus(win);
	for (size_t i = 0; i < LENGTH(win->modes); i++) {
		win->modes[i].parent = &vix_modes[i];
	}
	vix_event_emit(vix, VIX_EVENT_WIN_OPEN, win);
	return win;
}

bool vix_window_reload(Win *win) {
	const char *name = win->file->name;
	if (!name) {
		return false; /* can't reload unsaved file */
	}
	/* temporarily unset file name, otherwise file_new returns the same File */
	win->file->name = NULL;
	File *file = file_new(win->vix, name, false);
	win->file->name = name;
	if (!file) {
		return false;
	}
	file_free(win->vix, win->file);
	file->refcount = 1;
	win->file = file;
	view_reload(&win->view, file->text);
	return true;
}

bool vix_window_change_file(Win *win, const char* filename) {
	File *file = file_new(win->vix, filename, false);
	if (!file) {
		return false;
	}
	file->refcount++;
	if (win->file) {
		file_free(win->vix, win->file);
	}
	win->file = file;
	view_reload(&win->view, file->text);
	return true;
}

bool vix_window_split(Win *original) {
	original->vix->ui.doupdate = false;
	Win *win = window_new_file(original->vix, original->file, UI_OPTION_STATUSBAR);
	if (!win) {
		return false;
	}
	for (size_t i = 0; i < LENGTH(win->modes); i++) {
		if (original->modes[i].bindings) {
			win->modes[i].bindings = map_new();
		}
		if (win->modes[i].bindings) {
			map_copy(win->modes[i].bindings, original->modes[i].bindings);
		}
	}
	win->file = original->file;
	win_options_set(win, original->options);
	view_cursors_to(win->view.selection, view_cursor_get(&original->view));
	win->vix->ui.doupdate = true;
	return true;
}

void vix_window_focus(Win *win) {
	if (!win) {
		return;
	}
	Vix *vix = win->vix;
	vix->win = win;
	ui_window_focus(win);
}

void vix_window_next(Vix *vix) {
	Win *sel = vix->win;
	if (!sel) {
		return;
	}
	vix_window_focus(sel->next ? sel->next : vix->windows);
}

void vix_window_prev(Vix *vix) {
	Win *sel = vix->win;
	if (!sel) {
		return;
	}
	sel = sel->prev;
	if (!sel) {
		for (sel = vix->windows; sel->next; sel = sel->next);
	}
	vix_window_focus(sel);
}

void vix_draw(Vix *vix) {
	if (vix->headless) {
		return;
	}
	for (Win *win = vix->windows; win; win = win->next) {
		view_draw(&win->view);
	}
}

void vix_redraw(Vix *vix) {
	if (vix->headless) {
		return;
	}
	ui_redraw(&vix->ui);
	ui_draw(&vix->ui);
}

bool vix_window_new(Vix *vix, const char *filename) {
	File *file = file_new(vix, filename, false);
	if (!file) {
		return false;
	}
	vix->ui.doupdate = false;
	enum UiOption options = UI_OPTION_STATUSBAR|UI_OPTION_SYMBOL_EOF;
	if (vix->headless) {
		options = 0;
	}
	Win *win = window_new_file(vix, file, options);
	if (!win) {
		file_free(vix, file);
		return false;
	}
	vix->ui.doupdate = true;

	return true;
}

bool vix_window_new_fd(Vix *vix, int fd) {
	if (fd == -1) {
		return false;
	}
	if (!vix_window_new(vix, NULL)) {
		return false;
	}
	vix->win->file->fd = fd;
	return true;
}

bool vix_window_closable(Win *win) {
	if (!win || !text_modified(win->file->text)) {
		return true;
	}
	return win->file->refcount > 1;
}

void vix_window_swap(Win *a, Win *b) {
	if (a == b || !a || !b) {
		return;
	}
	Vix *vix = a->vix;
	Win *tmp = a->next;
	a->next = b->next;
	b->next = tmp;
	if (a->next) {
		a->next->prev = a;
	}
	if (b->next) {
		b->next->prev = b;
	}
	tmp = a->prev;
	a->prev = b->prev;
	b->prev = tmp;
	if (a->prev) {
		a->prev->next = a;
	}
	if (b->prev) {
		b->prev->next = b;
	}
	if (vix->windows == a) {
		vix->windows = b;
	} else if (vix->windows == b) {
		vix->windows = a;
	}
	ui_window_swap(a, b);
	if (vix->win == a) {
		vix_window_focus(b);
	} else if (vix->win == b) {
		vix_window_focus(a);
	}
}

void vix_window_close(Win *win) {
	if (!win) {
		return;
	}
	Vix *vix = win->vix;
	vix_event_emit(vix, VIX_EVENT_WIN_CLOSE, win);
	file_free(vix, win->file);
	if (vix->win == win) {
		vix->win = win->next ? win->next : win->prev;
	}
	if (win == vix->message_window) {
		vix->message_window = NULL;
	}
	window_free(win);
	if (vix->win) {
		ui_window_focus(vix->win);
	}
	vix_draw(vix);
}

bool vix_init(Vix *vix)
{
	zero_struct(vix);

	if (setjmp(vix->oom_jmp_buf)) {
		/* NOTE: if we run out of memory here we haven't opened any real
		 * files yet so it is safe to just cleanup and return. */
		vix_cleanup(vix);
		return false;
	}

	vix->exit_status = -1;
	if (!vix->headless) {
		if (!ui_terminal_init(&vix->ui)) {
			return false;
		}
		ui_init(&vix->ui, vix);
	}
	vix->change_colors = true;
	for (size_t i = 0; i < LENGTH(vix->registers); i++) {
		da_push(vix, vix->registers + i);
	}
	vix->registers[VIX_REG_BLACKHOLE].type = REGISTER_BLACKHOLE;
	vix->registers[VIX_REG_CLIPBOARD].type = REGISTER_CLIPBOARD;
	vix->registers[VIX_REG_PRIMARY].type = REGISTER_CLIPBOARD;
	vix->registers[VIX_REG_NUMBER].type = REGISTER_NUMBER;
	action_reset(&vix->action);
	vix->input_queue = (Buffer){0};
	vix->running = true;
	if (!(vix->command_file = file_new_internal(vix, NULL))) {
		goto err;
	}
	if (!(vix->search_file = file_new_internal(vix, NULL))) {
		goto err;
	}
	if (!(vix->error_file = file_new_internal(vix, NULL))) {
		goto err;
	}
	if (!(vix->actions = map_new())) {
		goto err;
	}
	if (!(vix->keymap = map_new())) {
		goto err;
	}
	if (!sam_init(vix)) {
		goto err;
	}
	struct passwd *pw;
	char *shell = getenv("SHELL");
	if ((!shell || !*shell) && (pw = getpwuid(getuid()))) {
		shell = pw->pw_shell;
	}
	if (!shell || !*shell) {
		shell = "/bin/sh";
	}
	if (!(vix->shell = strdup(shell))) {
		goto err;
	}
	vix->mode_prev = vix->mode = &vix_modes[VIX_MODE_NORMAL];
	vix_modes[VIX_MODE_INSERT].input  = vix_event_mode_insert_input;
	vix_modes[VIX_MODE_REPLACE].input = vix_event_mode_replace_input;
	return true;
err:
	vix_cleanup(vix);
	return false;
}

void vix_cleanup(Vix *vix)
{
	if (!vix) {
		return;
	}
	while (vix->windows) {
		vix_window_close(vix->windows);
	}
	vix_event_emit(vix, VIX_EVENT_QUIT);
	vix_process_waitall(vix);
	file_free(vix, vix->command_file);
	file_free(vix, vix->search_file);
	file_free(vix, vix->error_file);
	for (int i = 0; i < LENGTH(vix->registers); i++) {
		for (VixDACount j = 0; j < vix->registers[i].count; j++) {
			buffer_release(vix->registers[i].data + j);
		}
		da_release(vix->registers + i);
	}
	ui_terminal_free(&vix->ui);
	if (vix->usercmds) {
		const char *name = 0;
		while (map_first(vix->usercmds, &name) && vix_cmd_unregister(vix, name));
	}
	map_free(vix->usercmds);
	map_free(vix->cmds);
	if (vix->options) {
		const char *name = 0;
		while (map_first(vix->options, &name) && vix_option_unregister(vix, name));
	}
	map_free(vix->options);
	map_free(vix->actions);
	map_free(vix->keymap);
	buffer_release(&vix->input_queue);
	for (int i = 0; i < VIX_MODE_INVALID; i++) {
		map_free(vix_modes[i].bindings);
	}
	da_release(&vix->operators);
	da_release(&vix->motions);
	da_release(&vix->textobjects);
	while (vix->bindings.count) {
		vix_binding_free(vix, vix->bindings.data[0]);
	}
	da_release(&vix->bindings);
	for (VixDACount i = 0; i < vix->actions_user.count; i++) {
		keyaction_free(vix->actions_user.data[i]);
	}
	da_release(&vix->actions_user);
	free(vix->shell);
}

void vix_insert(Vix *vix, size_t pos, const char *data, size_t len) {
	Win *win = vix->win;
	if (!win) {
		return;
	}
	text_insert(vix, win->file->text, pos, data, len);
	vix_window_invalidate(win);
}

void vix_insert_key(Vix *vix, const char *data, size_t len) {
	Win *win = vix->win;
	if (!win) {
		return;
	}
	for (Selection *s = view_selections(&win->view); s; s = view_selections_next(s)) {
		size_t pos = view_cursors_pos(s);
		if (pos != EPOS) {
			vix_insert(vix, pos, data, len);
			view_cursors_scroll_to(s, pos + len);
		}
	}
}

void vix_replace(Vix *vix, size_t pos, const char *data, size_t len) {
	Win *win = vix->win;
	if (!win) {
		return;
	}
	Text *txt = win->file->text;
	Iterator it = text_iterator_get(txt, pos);
	int chars = text_char_count(data, len);
	for (char c; chars-- > 0 && text_iterator_byte_get(&it, &c) && c != '\n'; ) {
		text_iterator_char_next(&it, NULL);
	}

	text_delete(txt, pos, it.pos - pos);
	vix_insert(vix, pos, data, len);
}

void vix_replace_key(Vix *vix, const char *data, size_t len) {
	Win *win = vix->win;
	if (!win) {
		return;
	}
	for (Selection *s = view_selections(&win->view); s; s = view_selections_next(s)) {
		size_t pos = view_cursors_pos(s);
		vix_replace(vix, pos, data, len);
		view_cursors_scroll_to(s, pos + len);
	}
}

bool vix_action_register(Vix *vix, const KeyAction *action) {
	return map_put(vix->actions, action->name, action);
}

bool vix_keymap_add(Vix *vix, const char *key, const char *mapping) {
	return map_put(vix->keymap, key, mapping);
}

void vix_keymap_disable(Vix *vix) {
	vix->keymap_disabled = true;
}

void vix_do(Vix *vix) {
	Win *win = vix->win;
	if (!win) {
		return;
	}
	File *file = win->file;
	Text *txt = file->text;
	View *view = &win->view;
	Action *a = &vix->action;

	int count = MAX(a->count, 1);
	if (a->op == &vix_operators[VIX_OP_MODESWITCH]) {
		count = 1; /* count should apply to inserted text not motion */
	}
	bool repeatable = a->op && !vix->macro_operator && !vix->win->parent;
	bool multiple_cursors = view->selection_count > 1;

	bool linewise = !(a->type & CHARWISE) && (
		a->type & LINEWISE || (a->movement && a->movement->type & LINEWISE) ||
		vix->mode == &vix_modes[VIX_MODE_VISUAL_LINE]);

	Register *reg = a->reg;
	size_t reg_slot = multiple_cursors ? EPOS : 0;
	size_t last_reg_slot = reg_slot;
	if (!reg) {
		reg = &vix->registers[file->internal ? VIX_REG_PROMPT : VIX_REG_DEFAULT];
	}
	if (a->op == &vix_operators[VIX_OP_PUT_AFTER] && multiple_cursors && vix_register_count(vix, reg) == 1) {
		reg_slot = 0;
	}

	if (vix->mode->visual && a->op) {
		window_selection_save(win);
	}

	for (Selection *sel = view_selections(view), *next; sel; sel = next) {
		if (vix->interrupted) {
			break;
		}

		next = view_selections_next(sel);

		size_t pos = view_cursors_pos(sel);
		if (pos == EPOS) {
			if (!view_selections_dispose(sel)) {
				view_cursors_to(sel, 0);
			}
			continue;
		}

		OperatorContext c = {
			.count = count,
			.pos = pos,
			.newpos = EPOS,
			.range = text_range_empty(),
			.reg = reg,
			.reg_slot = reg_slot == EPOS ? (size_t)view_selections_number(sel) : reg_slot,
			.linewise = linewise,
			.arg = &a->arg,
			.context = a->op ? a->op->context : NULL,
		};

		last_reg_slot = c.reg_slot;

		bool err = false;
		if (a->movement) {
			size_t start = pos;
			for (int i = 0; i < count; i++) {
				size_t pos_prev = pos;
				if (a->movement->txt) {
					pos = a->movement->txt(txt, pos);
				} else if (a->movement->cur) {
					pos = a->movement->cur(sel);
				} else if (a->movement->file) {
					pos = a->movement->file(vix, file, sel);
				} else if (a->movement->vix) {
					pos = a->movement->vix(vix, txt, pos);
				} else if (a->movement->view) {
					pos = a->movement->view(vix, view);
				} else if (a->movement->win) {
					pos = a->movement->win(vix, win, pos);
				} else if (a->movement->user) {
					pos = a->movement->user(vix, win, a->movement->data, pos);
				}
				if (pos == EPOS || a->movement->type & IDEMPOTENT || pos == pos_prev) {
					err = a->movement->type & COUNT_EXACT;
					break;
				}
			}

			if (err) {
				repeatable = false;
				continue; // break?
			}

			if (pos == EPOS) {
				c.range.start = start;
				c.range.end = start;
				pos = start;
			} else {
				c.range = text_range_new(start, pos);
				c.newpos = pos;
			}

			if (!a->op) {
				if (a->movement->type & CHARWISE) {
					view_cursors_scroll_to(sel, pos);
				} else {
					view_cursors_to(sel, pos);
				}
				if (vix->mode->visual) {
					c.range = view_selections_get(sel);
				}
			} else if (a->movement->type & INCLUSIVE && c.range.end > start) {
				c.range.end = text_char_next(txt, c.range.end);
			} else if (linewise && (a->movement->type & LINEWISE_INCLUSIVE)) {
				c.range.end = text_char_next(txt, c.range.end);
			}
		} else if (a->textobj) {
			if (vix->mode->visual) {
				c.range = view_selections_get(sel);
			} else {
				c.range.start = c.range.end = pos;
			}
			for (int i = 0; i < count; i++) {
				Filerange r = text_range_empty();
				if (a->textobj->txt) {
					r = a->textobj->txt(txt, pos);
				} else if (a->textobj->vix) {
					r = a->textobj->vix(vix, txt, pos);
				} else if (a->textobj->user) {
					r = a->textobj->user(vix, win, a->textobj->data, pos);
				}
				if (!text_range_valid(&r)) {
					break;
				}
				if (a->textobj->type & TEXTOBJECT_DELIMITED_OUTER) {
					r.start--;
					r.end++;
				} else if (linewise && (a->textobj->type & TEXTOBJECT_DELIMITED_INNER)) {
					r.start = text_line_next(txt, r.start);
					r.end = text_line_prev(txt, r.end);
				}

				if (vix->mode->visual || (i > 0 && !(a->textobj->type & TEXTOBJECT_NON_CONTIGUOUS))) {
					c.range = text_range_union(&c.range, &r);
				} else {
					c.range = r;
				}

				if (i < count - 1) {
					if (a->textobj->type & TEXTOBJECT_EXTEND_BACKWARD) {
						pos = c.range.start;
						if ((a->textobj->type & TEXTOBJECT_DELIMITED_INNER) && pos > 0) {
							pos--;
						}
					} else {
						pos = c.range.end;
						if (a->textobj->type & TEXTOBJECT_DELIMITED_INNER) {
							pos++;
						}
					}
				}
			}
		} else if (vix->mode->visual) {
			c.range = view_selections_get(sel);
			if (!text_range_valid(&c.range)) {
				c.range.start = c.range.end = pos;
			}
		}

		if (linewise && vix->mode != &vix_modes[VIX_MODE_VISUAL]) {
			c.range = text_range_linewise(txt, &c.range);
		}
		if (vix->mode->visual) {
			view_selections_set(sel, &c.range);
			sel->anchored = true;
		}

		if (a->op) {
			size_t pos = a->op->func(vix, txt, &c);
			if (pos == EPOS) {
				view_selections_dispose(sel);
			} else if (pos <= text_size(txt)) {
				view_selection_clear(sel);
				view_cursors_to(sel, pos);
			}
		}
	}

	view_selections_normalize(view);
	if (a->movement && (a->movement->type & JUMP)) {
		vix_jumplist_save(vix);
	}

	if (a->op) {

		if (a->op == &vix_operators[VIX_OP_YANK] ||
		    a->op == &vix_operators[VIX_OP_DELETE] ||
		    a->op == &vix_operators[VIX_OP_CHANGE] ||
		    a->op == &vix_operators[VIX_OP_REPLACE]) {
			register_resize(reg, last_reg_slot+1);
		}

		/* we do not support visual repeat, still do something reasonable */
		if (vix->mode->visual && !a->movement && !a->textobj) {
			a->movement = &vix_motions[VIX_MOVE_NOP];
		}

		/* operator implementations must not change the mode,
		 * they might get called multiple times (once for every cursor)
		 */
		if (a->op == &vix_operators[VIX_OP_CHANGE]) {
			vix_mode_switch(vix, VIX_MODE_INSERT);
		} else if (a->op == &vix_operators[VIX_OP_MODESWITCH]) {
			vix_mode_switch(vix, a->mode);
		} else if (vix->mode == &vix_modes[VIX_MODE_OPERATOR_PENDING]) {
			mode_set(vix, vix->mode_prev);
		} else if (vix->mode->visual) {
			vix_mode_switch(vix, VIX_MODE_NORMAL);
		}

		if (vix->mode == &vix_modes[VIX_MODE_NORMAL]) {
			vix_file_snapshot(vix, file);
		}
		vix_draw(vix);
	}

	if (a != &vix->action_prev) {
		if (repeatable) {
			if (!a->macro) {
				a->macro = vix->macro_operator;
			}
			vix->action_prev = *a;
		}
		action_reset(a);
	}
}

void action_reset(Action *a) {
	zero_struct(a);
	a->count = VIX_COUNT_UNKNOWN;
}

void vix_cancel(Vix *vix) {
	action_reset(&vix->action);
}

void vix_die(Vix *vix, const char *msg, ...) {
	va_list ap;
	va_start(ap, msg);
	ui_die(&vix->ui, msg, ap);
	va_end(ap);
}

const char *vix_keys_next(Vix *vix, const char *keys) {
	if (!keys || !*keys) {
		return NULL;
	}
	TermKeyKey key;
	TermKey *termkey = vix->ui.termkey;
	const char *next = NULL;
	/* first try to parse a special key of the form <Key> */
	if (*keys == '<' && keys[1] && (next = termkey_strpkey(termkey, keys+1, &key, TERMKEY_FORMAT_VIM)) && *next == '>') {
		return next+1;
	}
	if (strncmp(keys, "<vix-", 5) == 0) {
		const char *start = keys + 1, *end = start;
		while (*end && *end != '>') {
			end++;
		}
		if (end > start && end - start - 1 < VIX_KEY_LENGTH_MAX && *end == '>') {
			char key[VIX_KEY_LENGTH_MAX];
			memcpy(key, start, end - start);
			key[end - start] = '\0';
			if (map_get(vix->actions, key)) {
				return end + 1;
			}
		}
	}
	if (ISUTF8(*keys)) {
		keys++;
	}
	while (!ISUTF8(*keys)) {
		keys++;
	}
	return keys;
}

long vix_keys_codepoint(Vix *vix, const char *keys) {
	long codepoint = -1;
	const char *next;
	TermKeyKey key;
	TermKey *termkey = vix->ui.termkey;

	if (!keys[0]) {
		return -1;
	}
	if (keys[0] == '<' && !keys[1]) {
		return '<';
	}

	if (keys[0] == '<' && (next = termkey_strpkey(termkey, keys+1, &key, TERMKEY_FORMAT_VIM)) && *next == '>') {
		codepoint = (key.type == TERMKEY_TYPE_UNICODE) ? key.code.codepoint : -1;
	} else if ((next = termkey_strpkey(termkey, keys, &key, TERMKEY_FORMAT_VIM))) {
		codepoint = (key.type == TERMKEY_TYPE_UNICODE) ? key.code.codepoint : -1;
	}

	if (codepoint != -1) {
		if (key.modifiers == TERMKEY_KEYMOD_CTRL) {
			codepoint &= 0x1f;
		}
		return codepoint;
	}

	if (!next || key.type != TERMKEY_TYPE_KEYSYM) {
		return -1;
	}

	const int keysym[] = {
		TERMKEY_SYM_ENTER, '\n',
		TERMKEY_SYM_TAB, '\t',
		TERMKEY_SYM_BACKSPACE, '\b',
		TERMKEY_SYM_ESCAPE, 0x1b,
		TERMKEY_SYM_DELETE, 0x7f,
		0,
	};

	for (const int *k = keysym; k[0]; k += 2) {
		if (key.code.sym == k[0]) {
			return k[1];
		}
	}

	return -1;
}

bool vix_keys_utf8(Vix *vix, const char *keys, char utf8[4+1])
{
	uint32_t cp = vix_keys_codepoint(vix, keys);
	bool result = cp != -1;
	if (result) {
		size_t len = utf8_encode((unsigned char *)utf8, cp);
		utf8[len] = 0;
	}
	return result;
}

typedef struct {
	Vix *vix;
	size_t len;         // length of the prefix
	int count;          // how many bindings can complete this prefix
	bool angle_bracket; // does the prefix end with '<'
} PrefixCompletion;

static bool isprefix(const char *key, void *value, void *data) {
	PrefixCompletion *completion = data;
	if (!completion->angle_bracket) {
		completion->count++;
	} else {
		const char *start = key + completion->len;
		const char *end = vix_keys_next(completion->vix, start);
		if (end && start + 1 == end) {
			completion->count++;
		}
	}
	return completion->count == 1;
}

static void vix_keys_process(Vix *vix, size_t pos) {
	Buffer *buf = &vix->input_queue;
	char *keys = buf->data + pos, *start = keys, *cur = keys, *end = keys, *binding_end = keys;;
	bool prefix = false;
	KeyBinding *binding = NULL;

	while (cur && *cur) {

		if (!(end = (char*)vix_keys_next(vix, cur))) {
			buffer_remove(buf, keys - buf->data, strlen(keys));
			return;
		}

		char tmp = *end;
		*end = '\0';
		prefix = false;

		for (Mode *global_mode = vix->mode; global_mode && !prefix; global_mode = global_mode->parent) {
			for (int global = 0; global < 2 && !prefix; global++) {
				Mode *mode = (global || !vix->win) ?
					     global_mode :
				             &vix->win->modes[global_mode->id];
				if (!mode->bindings) {
					continue;
				}
				/* keep track of longest matching binding */
				KeyBinding *match = map_get(mode->bindings, start);
				if (match && end > binding_end) {
					binding = match;
					binding_end = end;
				}

				const Map *pmap = map_prefix(mode->bindings, start);
				PrefixCompletion completions = {
					.vix = vix,
					.len = cur - start,
					.count = 0,
					.angle_bracket = !strcmp(cur, "<"),
				};
				map_iterate(pmap, isprefix, &completions);

				prefix = (!match && completions.count > 0) ||
				         ( match && completions.count > 1);
			}
		}

		*end = tmp;

		if (prefix) {
			/* input so far is ambiguous, wait for more */
			cur = end;
			end = start;
		} else if (binding) { /* exact match */
			if (binding->action) {
				size_t len = binding_end - start;
				strcpy(vix->key_prev, vix->key_current);
				strncpy(vix->key_current, start, len);
				vix->key_current[len] = '\0';
				end = (char*)binding->action->func(vix, binding_end, &binding->action->arg);
				if (!end) {
					end = start;
					break;
				}
				start = cur = end;
			} else if (binding->alias) {
				buffer_remove(buf, start - buf->data, binding_end - start);
				buffer_insert0(buf, start - buf->data, binding->alias);
				cur = end = start;
			}
			binding = NULL;
			binding_end = start;
		} else { /* no keybinding */
			KeyAction *action = NULL;
			if (start[0] == '<' && end[-1] == '>') {
				/* test for special editor key command */
				char tmp = end[-1];
				end[-1] = '\0';
				action = map_get(vix->actions, start+1);
				end[-1] = tmp;
				if (action) {
					size_t len = end - start;
					strcpy(vix->key_prev, vix->key_current);
					strncpy(vix->key_current, start, len);
					vix->key_current[len] = '\0';
					end = (char*)action->func(vix, end, &action->arg);
					if (!end) {
						end = start;
						break;
					}
				}
			}
			if (!action && vix->mode->input) {
				end = (char*)vix_keys_next(vix, start);
				vix->mode->input(vix, start, end - start);
			}
			start = cur = end;
		}
	}

	buffer_remove(buf, keys - buf->data, end - keys);
}

static void vix_keys_push(Vix *vix, const char *input, size_t pos, bool record)
{
	if (!input) {
		return;
	}
	if (record && vix->recording) {
		macro_append(vix->recording, input);
	}
	if (vix->macro_operator) {
		macro_append(vix->macro_operator, input);
	}
	if (buffer_append0(&vix->input_queue, input)) {
		vix_keys_process(vix, pos);
	}
}

static void macro_replay_internal(Vix *vix, const Macro *macro)
{
	size_t pos = buffer_length0(&vix->input_queue);
	for (char *key = macro->data, *next; key; key = next) {
		char tmp;
		next = (char*)vix_keys_next(vix, key);
		if (next) {
			tmp = *next;
			*next = '\0';
		}

		vix_keys_push(vix, key, pos, false);

		if (next) {
			*next = tmp;
		}
	}
}

static void macro_replay(Vix *vix, const Macro *macro)
{
	const Macro *replaying = vix->replaying;
	vix->replaying = macro;
	macro_replay_internal(vix, macro);
	vix->replaying = replaying;
}

void vix_keys_feed(Vix *vix, const char *input) {
	if (!input) {
		return;
	}
	Macro macro = {0};
	if (!macro_append(&macro, input)) {
		return;
	}
	/* use internal function, to keep Lua based tests which use undo points working */
	macro_replay_internal(vix, &macro);
	macro_release(&macro);
}

static const char *getkey(Vix *vix) {
	TermKeyKey key = { 0 };
	if (!ui_getkey(&vix->ui, &key)) {
		return NULL;
	}
	ui_info_hide(&vix->ui);
	bool use_keymap = vix->mode->id != VIX_MODE_INSERT &&
	                  vix->mode->id != VIX_MODE_REPLACE &&
	                  !vix->keymap_disabled;
	vix->keymap_disabled = false;
	if (key.type == TERMKEY_TYPE_UNICODE && use_keymap) {
		const char *mapped = map_get(vix->keymap, key.utf8);
		if (mapped) {
			size_t len = strlen(mapped)+1;
			if (len <= sizeof(key.utf8)) {
				memcpy(key.utf8, mapped, len);
			}
		}
	}

	TermKey *termkey = vix->ui.termkey;
	if (key.type == TERMKEY_TYPE_UNKNOWN_CSI) {
		long args[18];
		size_t nargs;
		unsigned long cmd;
		if (termkey_interpret_csi(termkey, &key, &args[2], &nargs, &cmd) == TERMKEY_RES_KEY) {
			args[0] = (long)cmd;
			args[1] = nargs;
			vix_event_emit(vix, VIX_EVENT_TERM_CSI, args);
		}
		return getkey(vix);
	}
	termkey_strfkey(termkey, vix->key, sizeof(vix->key), &key, TERMKEY_FORMAT_VIM);
	return vix->key;
}

bool vix_signal_handler(Vix *vix, int signum, const siginfo_t *siginfo, const void *context) {
	switch (signum) {
	case SIGBUS:
		for (File *file = vix->files; file; file = file->next) {
			if (text_mmaped(file->text, siginfo->si_addr)) {
				file->truncated = true;
			}
		}
		vix->sigbus = true;
		if (vix->running) {
			siglongjmp(vix->sigbus_jmpbuf, 1);
		}
		return true;
	case SIGINT:
		vix->interrupted = true;
		return true;
	case SIGCONT:
		vix->resume = true;
		/* fall through */
	case SIGWINCH:
		vix->need_resize = true;
		return true;
	case SIGTERM:
	case SIGHUP:
		vix->terminate = true;
		return true;
	}
	return false;
}

int vix_run(Vix *vix) {
	if (!vix->running || !vix->windows) {
		return EXIT_SUCCESS;
	}
	if (vix->exit_status != -1) {
		return vix->exit_status;
	}

	if (setjmp(vix->oom_jmp_buf)) {
		/* TODO: if we run out of memory here we may have files with unsaved changes.
		 * ideally we need to try and save temporary versions somewhere for later recovery */
		vix_cleanup(vix);
		vix_die(vix, "vix: out of memory\n");
	}

	vix_event_emit(vix, VIX_EVENT_START);

	struct timespec idle = { .tv_nsec = 0 }, *timeout = NULL;

	sigset_t emptyset;
	sigemptyset(&emptyset);
	vix_draw(vix);
	vix->exit_status = EXIT_SUCCESS;

	sigsetjmp(vix->sigbus_jmpbuf, 1);

	while (vix->running) {
		if (vix->headless && !vix->input_queue.len) {
			break;
		}
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);

		if (vix->sigbus) {
			char *name = NULL;
			for (Win *next, *win = vix->windows; win; win = next) {
				next = win->next;
				if (win->file->truncated) {
					free(name);
					name = strdup(win->file->name);
					vix_window_close(win);
				}
			}
			if (!vix->windows) {
				vix_die(vix, "WARNING: file `%s' truncated!\n", name ? name : "-");
			} else {
				vix_info_show(vix, "WARNING: file `%s' truncated!", name ? name : "-");
			}
			vix->sigbus = false;
			free(name);
		}

		if (vix->terminate) {
			vix_die(vix, "Killed by SIGTERM\n");
		}
		if (vix->interrupted) {
			vix->interrupted = false;
			vix_keys_push(vix, "<C-c>", 0, true);
			continue;
		}

		if (vix->resume) {
			ui_terminal_resume(&vix->ui);
			vix->resume = false;
		}

		if (vix->need_resize) {
			ui_resize(&vix->ui);
			vix->need_resize = false;
		}

		ui_draw(&vix->ui);
		idle.tv_sec = vix->mode->idle_timeout;
		
		if (vix->headless && !vix->input_queue.len) {
			vix->running = false;
			break;
		}

		int r = pselect(vix_process_before_tick(&fds) + 1, &fds, NULL, NULL,
		                timeout, &emptyset);
		if (r == -1 && errno == EINTR) {
			continue;
		}

		if (r < 0) {
			/* TODO save all pending changes to a ~suffixed file */
			vix_die(vix, "Error in mainloop: %s\n", strerror(errno));
		}
		vix_process_tick(vix, &fds);

		if (!FD_ISSET(STDIN_FILENO, &fds)) {
			if (vix->mode->idle) {
				vix->mode->idle(vix);
			}
			timeout = NULL;
			continue;
		}

		termkey_advisereadable(vix->ui.termkey);
		const char *key;

		while ((key = getkey(vix))) {
			vix_keys_push(vix, key, 0, true);
		}

		if (vix->mode->idle) {
			timeout = &idle;
		}
	}
	return vix->exit_status;
}

Macro *macro_get(Vix *vix, enum VixRegister id)
{
	if (id == VIX_MACRO_LAST_RECORDED) {
		return vix->last_recording;
	}
	if (VIX_REG_A <= id && id <= VIX_REG_Z) {
		id -= VIX_REG_A;
	}
	if (id < LENGTH(vix->registers)) {
		return vix->registers[id].data;
	}
	return NULL;
}

void macro_operator_record(Vix *vix) {
	if (vix->macro_operator) {
		return;
	}
	vix->macro_operator = macro_get(vix, VIX_MACRO_OPERATOR);
	vix->macro_operator->len = 0;
}

void macro_operator_stop(Vix *vix) {
	if (!vix->macro_operator) {
		return;
	}
	Macro *dot = macro_get(vix, VIX_REG_DOT);
	buffer_put(dot, vix->macro_operator->data, vix->macro_operator->len);
	vix->action_prev.macro = dot;
	vix->macro_operator = NULL;
}

bool vix_macro_record(Vix *vix, enum VixRegister id) {
	Macro *macro = macro_get(vix, id);
	if (vix->recording || !macro) {
		return false;
	}
	if (!(VIX_REG_A <= id && id <= VIX_REG_Z)) {
		macro->len = 0;
	}
	vix->recording = macro;
	vix_event_emit(vix, VIX_EVENT_WIN_STATUS, vix->win);
	return true;
}

bool vix_macro_record_stop(Vix *vix) {
	if (!vix->recording) {
		return false;
	}
	/* XXX: hack to remove last recorded key, otherwise upon replay
	 * we would start another recording */
	if (vix->recording->len > 1) {
		vix->recording->len--;
		vix->recording->data[vix->recording->len-1] = '\0';
	}
	vix->last_recording = vix->recording;
	vix->recording = NULL;
	vix_event_emit(vix, VIX_EVENT_WIN_STATUS, vix->win);
	return true;
}

bool vix_macro_recording(Vix *vix) {
	return vix->recording;
}

bool vix_macro_replay(Vix *vix, enum VixRegister id) {
	if (id == VIX_REG_SEARCH) {
		return vix_motion(vix, VIX_MOVE_SEARCH_REPEAT_FORWARD);
	}
	if (id == VIX_REG_COMMAND) {
		const char *cmd = register_get(vix, &vix->registers[id], NULL);
		return vix_cmd(vix, cmd);
	}

	Macro *macro = macro_get(vix, id);
	if (!macro || macro == vix->recording) {
		return false;
	}
	int count = VIX_COUNT_DEFAULT(vix->action.count, 1);
	vix_cancel(vix);
	for (int i = 0; i < count; i++) {
		macro_replay(vix, macro);
	}
	Win *win = vix->win;
	if (win) {
		vix_file_snapshot(vix, win->file);
	}
	return true;
}

void vix_repeat(Vix *vix) {
	const Macro *macro = vix->action_prev.macro;
	int count = vix->action.count;
	if (count != VIX_COUNT_UNKNOWN) {
		vix->action_prev.count = count;
	} else {
		count = vix->action_prev.count;
	}
	vix->action = vix->action_prev;
	vix_mode_switch(vix, VIX_MODE_OPERATOR_PENDING);
	vix_do(vix);
	if (macro) {
		Mode *mode = vix->mode;
		Action action_prev = vix->action_prev;
		if (count < 1 || action_prev.op == &vix_operators[VIX_OP_CHANGE]) {
			count = 1;
		}
		if (vix->action_prev.op == &vix_operators[VIX_OP_MODESWITCH]) {
			vix->action_prev.count = 1;
		}
		for (int i = 0; i < count; i++) {
			if (vix->interrupted) {
				break;
			}
			mode_set(vix, mode);
			macro_replay(vix, macro);
		}
		vix->action_prev = action_prev;
	}
	vix_cancel(vix);
	Win *win = vix->win;
	if (win) {
		vix_file_snapshot(vix, win->file);
	}
}

VixCountIterator vix_count_iterator_get(Vix *vix, int def) {
	return (VixCountIterator) {
		.vix = vix,
		.iteration = 0,
		.count = VIX_COUNT_DEFAULT(vix->action.count, def),
	};
}

VixCountIterator vix_count_iterator_init(Vix *vix, int count) {
	return (VixCountIterator) {
		.vix = vix,
		.iteration = 0,
		.count = count,
	};
}

bool vix_count_iterator_next(VixCountIterator *it) {
	if (it->vix->interrupted) {
		return false;
	}
	return it->iteration++ < it->count;
}

void vix_exit(Vix *vix, int status) {
	vix->running = false;
	vix->exit_status = status;
}

void vix_insert_tab(Vix *vix) {
	Win *win = vix->win;
	if (!win) {
		return;
	}
	if (!win->expandtab) {
		vix_insert_key(vix, "\t", 1);
		return;
	}
	char spaces[9];
	int tabwidth = MIN(vix->win->view.tabwidth, LENGTH(spaces) - 1);
	for (Selection *s = view_selections(&win->view); s; s = view_selections_next(s)) {
		size_t pos = view_cursors_pos(s);
		int width = text_line_width_get(win->file->text, pos);
		int count = tabwidth - (width % tabwidth);
		for (int i = 0; i < count; i++) {
			spaces[i] = ' ';
		}
		spaces[count] = '\0';
		vix_insert(vix, pos, spaces, count);
		view_cursors_scroll_to(s, pos + count);
	}
}

size_t vix_text_insert_nl(Vix *vix, Text *txt, size_t pos) {
	size_t indent_len = 0;
	char byte, *indent = NULL;
	/* insert second newline at end of file, except if there is already one */
	bool eof = pos == text_size(txt);
	bool nl2 = eof && !(pos > 0 && text_byte_get(txt, pos-1, &byte) && byte == '\n');

	if (vix->autoindent) {
		/* copy leading white space of current line */
		size_t begin = text_line_begin(txt, pos);
		size_t start = text_line_start(txt, begin);
		size_t end = text_line_end(txt, start);
		if (start > pos) {
			start = pos;
		}
		indent_len = start >= begin ? start-begin : 0;
		if (start == end) {
			pos = begin;
		} else {
			indent = malloc(indent_len+1);
			if (indent) {
				indent_len = text_bytes_get(txt, begin, indent_len, indent);
			}
		}
	}

	text_insert(vix, txt, pos, "\n", 1);
	if (eof) {
		if (nl2) {
			text_insert(vix, txt, text_size(txt), "\n", 1);
		} else {
			pos--; /* place cursor before, not after nl */
		}
	}
	pos++;

	if (indent) {
		text_insert(vix, txt, pos, indent, indent_len);
	}
	free(indent);
	return pos + indent_len;
}

void vix_insert_nl(Vix *vix) {
	Win *win = vix->win;
	if (!win) {
		return;
	}
	Text *txt = win->file->text;
	for (Selection *s = view_selections(&win->view); s; s = view_selections_next(s)) {
		size_t pos = view_cursors_pos(s);
		size_t newpos = vix_text_insert_nl(vix, txt, pos);
		/* This is a bit of a hack to fix cursor positioning when
		 * inserting a new line at the start of the view port.
		 * It has the effect of resetting the mark used by the view
		 * code to keep track of the start of the visible region.
		 */
		view_cursors_to(s, pos);
		view_cursors_to(s, newpos);
	}
	vix_window_invalidate(win);
}

Regex *vix_regex(Vix *vix, const char *pattern) {
	if (!pattern && !(pattern = register_get(vix, &vix->registers[VIX_REG_SEARCH], NULL))) {
		return NULL;
	}
	Regex *regex = text_regex_new();
	if (!regex) {
		return NULL;
	}
	int cflags = REG_EXTENDED|REG_NEWLINE|(REG_ICASE*vix->ignorecase);
	if (text_regex_compile(regex, pattern, cflags) != 0) {
		text_regex_free(regex);
		return NULL;
	}
	register_put0(vix, &vix->registers[VIX_REG_SEARCH], pattern);
	return regex;
}

static int _vix_pipe(Vix *vix, File *file, Filerange *range, const char* buf, const char *argv[],
	void *stdout_context, ssize_t (*read_stdout)(void *stdout_context, char *data, size_t len),
	void *stderr_context, ssize_t (*read_stderr)(void *stderr_context, char *data, size_t len),
	bool fullscreen) {

	/* if an invalid range was given, stdin (i.e. key board input) is passed
	 * through the external command. */
	Text *text = file != NULL ? file->text : NULL;
	int pin[2], pout[2], perr[2], status = -1;
	bool interactive = buf == NULL && (range == NULL || !text_range_valid(range));
	Filerange rout = (interactive  || buf != NULL) ? text_range_new(0, 0) : *range;

	if (pipe(pin) == -1) {
		return -1;
	}
	if (pipe(pout) == -1) {
		close(pin[0]);
		close(pin[1]);
		return -1;
	}

	if (pipe(perr) == -1) {
		close(pin[0]);
		close(pin[1]);
		close(pout[0]);
		close(pout[1]);
		return -1;
	}

	ui_terminal_save(&vix->ui, fullscreen);
	pid_t pid = fork();

	if (pid == -1) {
		close(pin[0]);
		close(pin[1]);
		close(pout[0]);
		close(pout[1]);
		close(perr[0]);
		close(perr[1]);
		vix_info_show(vix, "fork failure: %s", strerror(errno));
		return -1;
	} else if (pid == 0) { /* child i.e filter */
		sigset_t sigterm_mask;
		sigemptyset(&sigterm_mask);
		sigaddset(&sigterm_mask, SIGTERM);
		if (sigprocmask(SIG_UNBLOCK, &sigterm_mask, NULL) == -1) {
			fprintf(stderr, "failed to reset signal mask");
			exit(EXIT_FAILURE);
		}

		int null = open("/dev/null", O_RDWR);
		if (null == -1) {
			fprintf(stderr, "failed to open /dev/null");
			exit(EXIT_FAILURE);
		}

		if (!interactive) {
			/* If we have nothing to write, let stdin point to
			 * /dev/null instead of a pipe which is immediately
			 * closed. Some programs behave differently when used
			 * in a pipeline.
			 */
			if (range && text_range_size(range) == 0) {
				dup2(null, STDIN_FILENO);
			} else {
				dup2(pin[0], STDIN_FILENO);
			}
		}

		close(pin[0]);
		close(pin[1]);
		if (interactive) {
			dup2(STDERR_FILENO, STDOUT_FILENO);
			/* For some reason the first byte written by the
			 * interactive application is not being displayed.
			 * It probably has something to do with the terminal
			 * state change. By writing a dummy byte ourself we
			 * ensure that the complete output is visible.
			 */
			while(write(STDOUT_FILENO, " ", 1) == -1 && errno == EINTR);
		} else if (read_stdout) {
			dup2(pout[1], STDOUT_FILENO);
		} else {
			dup2(null, STDOUT_FILENO);
		}
		close(pout[1]);
		close(pout[0]);
		if (!interactive) {
			if (read_stderr) {
				dup2(perr[1], STDERR_FILENO);
			} else {
				dup2(null, STDERR_FILENO);
			}
		}
		close(perr[0]);
		close(perr[1]);
		close(null);

		if (file != NULL && file->name) {
			const char *name = (strrchr)(file->name, '/');
			setenv("vix_filepath", file->name, 1);
			setenv("vix_filename", name ? name+1 : file->name, 1);
		}

		if (!argv[1]) {
			execlp(vix->shell, vix->shell, "-c", argv[0], (char*)NULL);
		} else {
			execvp(argv[0], (char* const*)argv);
		}
		fprintf(stderr, "exec failure: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	vix->interrupted = false;

	close(pin[0]);
	close(pout[1]);
	close(perr[1]);

	if (fcntl(pout[0], F_SETFL, O_NONBLOCK) == -1 ||
	    fcntl(perr[0], F_SETFL, O_NONBLOCK) == -1) {
		goto err;
	}

	fd_set rfds, wfds;

	do {
		if (vix->interrupted) {
			kill(0, SIGTERM);
			break;
		}

		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		if (pin[1] != -1) {
			FD_SET(pin[1], &wfds);
		}
		if (pout[0] != -1) {
			FD_SET(pout[0], &rfds);
		}
		if (perr[0] != -1) {
			FD_SET(perr[0], &rfds);
		}

		if (select(FD_SETSIZE, &rfds, &wfds, NULL, NULL) == -1) {
			if (errno == EINTR) {
				continue;
			}
			vix_info_show(vix, "Select failure");
			break;
		}

		if (pin[1] != -1 && FD_ISSET(pin[1], &wfds)) {
			ssize_t written = 0;
			Filerange junk = rout;
			if (text_range_size(&rout)) {
				if (junk.end > junk.start + PIPE_BUF) {
					junk.end = junk.start + PIPE_BUF;
				}
				written = text_write_range(text, &junk, pin[1]);
				if (written > 0) {
					rout.start += written;
					if (text_range_size(&rout) == 0) {
						close(pin[1]);
						pin[1] = -1;
					}
				}
			} else if (buf != NULL) {
				size_t len = strlen(buf);
				if (len > 0) {
					if (len > PIPE_BUF) {
						len = PIPE_BUF;
					}

					written = write_all(pin[1], buf, len);
					if (written > 0) {
						buf += written;
					}
				}
			}

			if (written <= 0) {
				close(pin[1]);
				pin[1] = -1;
				if (written == -1) {
					vix_info_show(vix, "Error writing to external command");
				}
			}
		}

		if (pout[0] != -1 && FD_ISSET(pout[0], &rfds)) {
			char buf[BUFSIZ];
			ssize_t len = read(pout[0], buf, sizeof buf);
			if (len > 0) {
				if (read_stdout) {
					(*read_stdout)(stdout_context, buf, len);
				}
			} else if (len == 0) {
				close(pout[0]);
				pout[0] = -1;
			} else if (errno != EINTR && errno != EWOULDBLOCK) {
				vix_info_show(vix, "Error reading from filter stdout");
				close(pout[0]);
				pout[0] = -1;
			}
		}

		if (perr[0] != -1 && FD_ISSET(perr[0], &rfds)) {
			char buf[BUFSIZ];
			ssize_t len = read(perr[0], buf, sizeof buf);
			if (len > 0) {
				if (read_stderr) {
					(*read_stderr)(stderr_context, buf, len);
				}
			} else if (len == 0) {
				close(perr[0]);
				perr[0] = -1;
			} else if (errno != EINTR && errno != EWOULDBLOCK) {
				vix_info_show(vix, "Error reading from filter stderr");
				close(perr[0]);
				perr[0] = -1;
			}
		}

	} while (pin[1] != -1 || pout[0] != -1 || perr[0] != -1);

err:
	if (pin[1] != -1) {
		close(pin[1]);
	}
	if (pout[0] != -1) {
		close(pout[0]);
	}
	if (perr[0] != -1) {
		close(perr[0]);
	}

	for (;;) {
		if (vix->interrupted) {
			kill(0, SIGTERM);
		}
		pid_t died = waitpid(pid, &status, 0);
		if ((died == -1 && errno == ECHILD) || pid == died) {
			break;
		}
	}

	/* clear any pending SIGTERM */
	struct sigaction sigterm_ignore, sigterm_old;
	sigterm_ignore.sa_handler = SIG_IGN;
	sigterm_ignore.sa_flags = 0;
	sigemptyset(&sigterm_ignore.sa_mask);

	sigaction(SIGTERM, &sigterm_ignore, &sigterm_old);
	sigaction(SIGTERM, &sigterm_old, NULL);

	vix->interrupted = false;
	ui_terminal_restore(&vix->ui);

	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}

	return -1;
}

int vix_pipe(Vix *vix, File *file, Filerange *range, const char *argv[],
	void *stdout_context, ssize_t (*read_stdout)(void *stdout_context, char *data, size_t len),
	void *stderr_context, ssize_t (*read_stderr)(void *stderr_context, char *data, size_t len),
	bool fullscreen) {
	return _vix_pipe(vix, file, range, NULL, argv, stdout_context, read_stdout, stderr_context, read_stderr, fullscreen);
}

int vix_pipe_buf(Vix *vix, const char* buf, const char *argv[],
	void *stdout_context, ssize_t (*read_stdout)(void *stdout_context, char *data, size_t len),
	void *stderr_context, ssize_t (*read_stderr)(void *stderr_context, char *data, size_t len),
	bool fullscreen) {
	return _vix_pipe(vix, NULL, NULL, buf, argv, stdout_context, read_stdout, stderr_context, read_stderr, fullscreen);
}

static int _vix_pipe_collect(Vix *vix, File *file, Filerange *range, const char* buf, const char *argv[], char **out, char **err, bool fullscreen) {
	Buffer bufout = {0}, buferr = {0};
	int status = _vix_pipe(vix, file, range, buf, argv,
	                      &bufout, out ? read_into_buffer : NULL,
	                      &buferr, err ? read_into_buffer : NULL,
	                      fullscreen);
	buffer_terminate(&bufout);
	buffer_terminate(&buferr);
	if (out) { *out = bufout.data; }
	if (err) { *err = buferr.data; }
	return status;
}

int vix_pipe_collect(Vix *vix, File *file, Filerange *range, const char *argv[], char **out, char **err, bool fullscreen) {
	return _vix_pipe_collect(vix, file, range, NULL, argv, out, err, fullscreen);
}

int vix_pipe_buf_collect(Vix *vix, const char* buf, const char *argv[], char **out, char **err, bool fullscreen) {
	return _vix_pipe_collect(vix, NULL, NULL, buf, argv, out, err, fullscreen);
}

bool vix_cmd(Vix *vix, const char *cmdline) {
	if (!cmdline) {
		return true;
	}
	while (*cmdline == ':') {
		cmdline++;
	}
	char *line = strdup(cmdline);
	if (!line) {
		return false;
	}

	size_t len = strlen(line);
	while (len > 0 && isspace((unsigned char)line[len-1])) {
		len--;
	}
	line[len] = '\0';

	enum SamError err = sam_cmd(vix, line);
	if (err != SAM_ERR_OK) {
		vix_info_show(vix, "%s", sam_error(err));
	}
	free(line);
	return err == SAM_ERR_OK;
}

void vix_file_snapshot(Vix *vix, File *file) {
	if (!vix->replaying) {
		text_snapshot(file->text);
	}
}

Text *vix_text(Vix *vix) {
	Win *win = vix->win;
	return win ? win->file->text : NULL;
}

View *vix_view(Vix *vix) {
	Win *win = vix->win;
	return win ? &win->view : NULL;
}
