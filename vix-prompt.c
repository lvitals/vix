#include "vix-core.h"
#include "text-motions.h"
#include "text-objects.h"
#include "text-util.h"

bool vix_prompt_cmd(Vix *vix, const char *cmd) {
	if (!cmd || !cmd[0]) {
		return true;
	}
	switch (cmd[0]) {
	case '/':
		return vix_motion(vix, VIX_MOVE_SEARCH_FORWARD, cmd+1);
	case '?':
		return vix_motion(vix, VIX_MOVE_SEARCH_BACKWARD, cmd+1);
	case '+':
	case ':':
		register_put0(vix, &vix->registers[VIX_REG_COMMAND], cmd+1);
		return vix_cmd(vix, cmd+1);
	default:
		register_put0(vix, &vix->registers[VIX_REG_COMMAND], cmd);
		return vix_cmd(vix, cmd);
	}
}

static void prompt_hide(Win *win) {
	Text *txt = win->file->text;
	size_t size = text_size(txt);
	/* make sure that file is new line terminated */
	char lastchar = '\0';
	if (size >= 1 && text_byte_get(txt, size-1, &lastchar) && lastchar != '\n') {
		text_insert(win->vix, txt, size, "\n", 1);
	}
	/* remove empty entries */
	Filerange line_range = text_object_line(txt, text_size(txt)-1);
	char *line = text_bytes_alloc0(txt, line_range.start, text_range_size(&line_range));
	if (line && (line[0] == '\n' || ((strchr)(":/?", line[0]) && (line[1] == '\n' || line[1] == '\0')))) {
		text_delete_range(txt, &line_range);
	}
	free(line);
	vix_window_close(win);
}

static void prompt_restore(Win *win) {
	Vix *vix = win->vix;
	/* restore window and mode which was active before the prompt window
	 * we deliberately don't use vix_mode_switch because we do not want
	 * to invoke the modes enter/leave functions */
	if (win->parent) {
		vix->win = win->parent;
	}
	vix->mode = win->parent_mode;
}

static const char *prompt_enter(Vix *vix, const char *keys, const Arg *arg) {
	Win *prompt = vix->win;
	View *view = &prompt->view;
	Text *txt = prompt->file->text;
	Win *win = prompt->parent;
	char *cmd = NULL;

	Filerange range = view_selections_get(view->selection);
	if (!vix->mode->visual) {
		const char *pattern = NULL;
		Regex *regex = text_regex_new();
		size_t pos = view_cursor_get(view);
		if (prompt->file == vix->command_file) {
			pattern = "^:";
		} else if (prompt->file == vix->search_file) {
			pattern = "^(/|\\?)";
		}
		int cflags = REG_EXTENDED|REG_NEWLINE|(REG_ICASE*vix->ignorecase);
		if (pattern && regex && text_regex_compile(regex, pattern, cflags) == 0) {
			size_t end = text_line_end(txt, pos);
			size_t prev = text_search_backward(txt, end, regex);
			if (prev > pos) {
				prev = EPOS;
			}
			size_t next = text_search_forward(txt, pos, regex);
			if (next < pos) {
				next = text_size(txt);
			}
			range = text_range_new(prev, next);
		}
		text_regex_free(regex);
	}
	if (text_range_valid(&range)) {
		cmd = text_bytes_alloc0(txt, range.start, text_range_size(&range));
	}

	if (!win || !cmd) {
		if (!win) {
			vix_info_show(vix, "Prompt window invalid");
		} else if (!cmd) {
			vix_info_show(vix, "Failed to detect command");
		}
		prompt_restore(prompt);
		prompt_hide(prompt);
		free(cmd);
		return keys;
	}

	size_t len = strlen(cmd);
	if (len > 0 && cmd[len-1] == '\n') {
		cmd[len-1] = '\0';
	}

	bool lastline = (range.end == text_size(txt));

	prompt_restore(prompt);
	if (vix_prompt_cmd(vix, cmd)) {
		prompt_hide(prompt);
		if (!lastline) {
			text_delete(txt, range.start, text_range_size(&range));
			text_appendf(vix, txt, "%s\n", cmd);
		}
	} else {
		vix->win = prompt;
		vix->mode = &vix_modes[VIX_MODE_INSERT];
	}
	free(cmd);
	vix_draw(vix);
	return keys;
}

static const char *prompt_esc(Vix *vix, const char *keys, const Arg *arg) {
	Win *prompt = vix->win;
	if (prompt->view.selection_count > 1) {
		view_selections_dispose_all(&prompt->view);
	} else {
		prompt_restore(prompt);
		prompt_hide(prompt);
	}
	return keys;
}

static const char *prompt_up(Vix *vix, const char *keys, const Arg *arg) {
	vix_motion(vix, VIX_MOVE_LINE_UP);
	vix_motion(vix, VIX_MOVE_LINE_END);
	return keys;
}

static const char *prompt_down(Vix *vix, const char *keys, const Arg *arg) {
	vix_motion(vix, VIX_MOVE_LINE_DOWN);
	vix_motion(vix, VIX_MOVE_LINE_END);
	return keys;
}

static const char *prompt_left(Vix *vix, const char *keys, const Arg *arg) {
	Win *prompt = vix->win;
	size_t pos = view_cursor_get(&prompt->view);
	size_t start = text_line_begin(prompt->file->text, pos);
	if (pos > start + 1) {
		vix_motion(vix, VIX_MOVE_CHAR_PREV);
	}
	return keys;
}

static const char *prompt_home(Vix *vix, const char *keys, const Arg *arg) {
	Win *prompt = vix->win;
	size_t pos = view_cursor_get(&prompt->view);
	size_t start = text_line_begin(prompt->file->text, pos);
	view_cursors_to(prompt->view.selection, start + 1);
	return keys;
}

static const char *prompt_backspace(Vix *vix, const char *keys, const Arg *arg) {
	Win *prompt = vix->win;
	Text *txt = prompt->file->text;
	size_t pos = view_cursor_get(&prompt->view);
	size_t start = text_line_begin(txt, pos);
	if (pos > start + 1) {
		text_delete(txt, pos - 1, 1);
		view_cursors_to(prompt->view.selection, pos - 1);
	}
	return keys;
}

static const KeyBinding prompt_enter_binding = {
	.key = "<Enter>",
	.action = &(KeyAction){
		.func = prompt_enter,
	},
};

static const KeyBinding prompt_esc_binding = {
	.key = "<Escape>",
	.action = &(KeyAction){
		.func = prompt_esc,
	},
};

static const KeyBinding prompt_up_binding = {
	.key = "<Up>",
	.action = &(KeyAction){
		.func = prompt_up,
	},
};

static const KeyBinding prompt_down_binding = {
	.key = "<Down>",
	.action = &(KeyAction){
		.func = prompt_down,
	},
};

static const KeyBinding prompt_left_binding = {
	.key = "<Left>",
	.action = &(KeyAction){
		.func = prompt_left,
	},
};

static const KeyBinding prompt_backspace_binding = {
	.key = "<Backspace>",
	.action = &(KeyAction){
		.func = prompt_backspace,
	},
};

static const KeyBinding prompt_home_binding = {
	.key = "<C-a>",
	.action = &(KeyAction){
		.func = prompt_home,
	},
};

static const KeyBinding prompt_tab_binding = {
	.key = "<Tab>",
	.alias = "<C-x><C-o>",
};

void vix_prompt_show(Vix *vix, const char *title) {
	Win *active = vix->win;
	Win *prompt = window_new_file(vix, title[0] == ':' ? vix->command_file : vix->search_file,
		UI_OPTION_ONELINE);
	if (!prompt) {
		return;
	}
	Text *txt = prompt->file->text;
	text_appendf(vix, txt, "%s\n", title);
	Selection *sel = view_selections_primary_get(&prompt->view);
	view_cursors_scroll_to(sel, text_size(txt)-1);
	prompt->parent = active;
	prompt->parent_mode = vix->mode;
	vix_window_focus(prompt);
	vix_window_mode_map(prompt, VIX_MODE_NORMAL, true, "<Enter>", &prompt_enter_binding);
	vix_window_mode_map(prompt, VIX_MODE_INSERT, true, "<Enter>", &prompt_enter_binding);
	vix_window_mode_map(prompt, VIX_MODE_INSERT, true, "<C-j>", &prompt_enter_binding);
	vix_window_mode_map(prompt, VIX_MODE_VISUAL, true, "<Enter>", &prompt_enter_binding);
	vix_window_mode_map(prompt, VIX_MODE_NORMAL, true, "<Escape>", &prompt_esc_binding);
	vix_window_mode_map(prompt, VIX_MODE_INSERT, true, "<Up>", &prompt_up_binding);
	vix_window_mode_map(prompt, VIX_MODE_INSERT, true, "<Down>", &prompt_down_binding);
	vix_window_mode_map(prompt, VIX_MODE_INSERT, true, "<Left>", &prompt_left_binding);
	vix_window_mode_map(prompt, VIX_MODE_INSERT, true, "<Backspace>", &prompt_backspace_binding);
	vix_window_mode_map(prompt, VIX_MODE_INSERT, true, "<C-h>", &prompt_backspace_binding);
	vix_window_mode_map(prompt, VIX_MODE_INSERT, true, "<C-a>", &prompt_home_binding);
	vix_window_mode_map(prompt, VIX_MODE_INSERT, true, "<Home>", &prompt_home_binding);
	if (CONFIG_LUA) {
		vix_window_mode_map(prompt, VIX_MODE_INSERT, true, "<Tab>", &prompt_tab_binding);
	}
	vix_mode_switch(vix, VIX_MODE_INSERT);
}

void vix_info_show(Vix *vix, const char *msg, ...) {
	va_list ap;
	va_start(ap, msg);
	ui_info_show(&vix->ui, msg, ap);
	va_end(ap);
}

void vix_message_show(Vix *vix, const char *msg) {
	if (!msg) {
		return;
	}
	if (!vix->message_window) {
		vix->message_window = window_new_file(vix, vix->error_file, UI_OPTION_STATUSBAR);
	}
	Win *win = vix->message_window;
	if (!win) {
		return;
	}
	Text *txt = win->file->text;
	size_t pos = text_size(txt);
	text_appendf(vix, txt, "%s\n", msg);
	text_mark_current_revision(txt);
	view_cursors_to(win->view.selection, pos);
	vix_window_focus(win);
}
