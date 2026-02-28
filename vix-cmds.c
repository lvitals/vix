/* this file is included from sam.c */

#include "vix-lua.h"

// FIXME: avoid this redirection?
typedef struct {
	CommandDef def;
	VixCommandFunction *func;
	void *data;
} CmdUser;

static void cmdfree(CmdUser *cmd) {
	if (!cmd) {
		return;
	}
	free((char*)cmd->def.name);
	free(VIX_HELP_USE((char*)cmd->def.help));
	free(cmd);
}

bool vix_cmd_register(Vix *vix, const char *name, const char *help, void *data, VixCommandFunction *func) {
	if (!name) {
		return false;
	}
	if (!vix->usercmds && !(vix->usercmds = map_new())) {
		return false;
	}
	CmdUser *cmd = calloc(1, sizeof *cmd);
	if (!cmd) {
		return false;
	}
	if (!(cmd->def.name = strdup(name))) {
		goto err;
	}
#if CONFIG_HELP
	if (help && !(cmd->def.help = strdup(help))) {
		goto err;
	}
#endif
	cmd->def.flags = CMD_ARGV|CMD_FORCE|CMD_ONCE|CMD_ADDRESS_ALL;
	cmd->def.func = cmd_user;
	cmd->func = func;
	cmd->data = data;
	if (!map_put(vix->cmds, name, &cmd->def)) {
		goto err;
	}
	if (!map_put(vix->usercmds, name, cmd)) {
		map_delete(vix->cmds, name);
		goto err;
	}
	return true;
err:
	cmdfree(cmd);
	return false;
}

bool vix_cmd_unregister(Vix *vix, const char *name) {
	if (!name) {
		return true;
	}
	CmdUser *cmd = map_get(vix->usercmds, name);
	if (!cmd) {
		return false;
	}
	if (!map_delete(vix->cmds, name)) {
		return false;
	}
	if (!map_delete(vix->usercmds, name)) {
		return false;
	}
	cmdfree(cmd);
	return true;
}

static void option_free(OptionDef *opt) {
	if (!opt) {
		return;
	}
	for (size_t i = 0; i < LENGTH(options); i++) {
		if (opt == &options[i]) {
			return;
		}
	}

	for (const char **name = opt->names; *name; name++) {
		free((char*)*name);
	}
	free(VIX_HELP_USE((char*)opt->help));
	free(opt);
}

bool vix_option_register(Vix *vix, const char *names[], enum VixOption flags,
                         VixOptionFunction *func, void *context, const char *help) {

	if (!names || !names[0]) {
		return false;
	}

	for (const char **name = names; *name; name++) {
		if (map_get(vix->options, *name)) {
			return false;
		}
	}
	OptionDef *opt = calloc(1, sizeof *opt);
	if (!opt) {
		return false;
	}
	for (size_t i = 0; i < LENGTH(opt->names)-1 && names[i]; i++) {
		if (!(opt->names[i] = strdup(names[i]))) {
			goto err;
		}
	}
	opt->flags = flags;
	opt->func = func;
	opt->context = context;
#if CONFIG_HELP
	if (help && !(opt->help = strdup(help))) {
		goto err;
	}
#endif
	for (const char **name = names; *name; name++) {
		map_put(vix->options, *name, opt);
	}
	return true;
err:
	option_free(opt);
	return false;
}

bool vix_option_unregister(Vix *vix, const char *name) {
	OptionDef *opt = map_get(vix->options, name);
	if (!opt) {
		return false;
	}
	for (const char **alias = opt->names; *alias; alias++) {
		if (!map_delete(vix->options, *alias)) {
			return false;
		}
	}
	option_free(opt);
	return true;
}

static bool cmd_user(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	CmdUser *user = map_get(vix->usercmds, argv[0]);
	return user && user->func(vix, win, user->data, cmd->flags == '!', argv, sel, range);
}

void vix_shell_set(Vix *vix, const char *new_shell) {
	char *shell =  strdup(new_shell);
	if (!shell) {
		vix_info_show(vix, "Failed to change shell");
	} else {
		free(vix->shell);
		vix->shell = shell;
	}
}

/* parse human-readable boolean value in s. If successful, store the result in
 * outval and return true. Else return false and leave outval alone. */
static bool parse_bool(const char *s, bool *outval) {
	for (const char **t = (const char*[]){"1", "true", "yes", "on", NULL}; *t; t++) {
		if (!strcasecmp(s, *t)) {
			*outval = true;
			return true;
		}
	}
	for (const char **f = (const char*[]){"0", "false", "no", "off", NULL}; *f; f++) {
		if (!strcasecmp(s, *f)) {
			*outval = false;
			return true;
		}
	}
	return false;
}

static bool cmd_set(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {

	if (!argv[1] || !argv[1][0] || argv[3]) {
		vix_info_show(vix, "Expecting: set option [value]");
		return false;
	}

	char name[256];
	strncpy(name, argv[1], sizeof(name)-1);
	char *lastchar = &name[strlen(name)-1];
	bool toggle = (*lastchar == '!');
	if (toggle) {
		*lastchar = '\0';
	}

	OptionDef *opt = map_closest(vix->options, name);
	if (!opt) {
		vix_info_show(vix, "Unknown option: `%s'", name);
		return false;
	}

	if (opt->flags & VIX_OPTION_DEPRECATED && strcmp(opt->context, name) == 0) {
		vix_info_show(vix, "%s is deprecated and will be removed in the next release", name);
	}

	if (!win && (opt->flags & VIX_OPTION_NEED_WINDOW)) {
		vix_info_show(vix, "Need active window for `:set %s'", name);
		return false;
	}

	if (toggle) {
		if (!(opt->flags & VIX_OPTION_TYPE_BOOL)) {
			vix_info_show(vix, "Only boolean options can be toggled");
			return false;
		}
		if (argv[2]) {
			vix_info_show(vix, "Can not specify option value when toggling");
			return false;
		}
	}

	Arg arg;
	if (opt->flags & VIX_OPTION_TYPE_STRING) {
		if (!(opt->flags & VIX_OPTION_VALUE_OPTIONAL) && !argv[2]) {
			vix_info_show(vix, "Expecting string option value");
			return false;
		}
		arg.s = argv[2];
	} else if (opt->flags & VIX_OPTION_TYPE_BOOL) {
		if (!argv[2]) {
			arg.b = !toggle;
		} else if (!parse_bool(argv[2], &arg.b)) {
			vix_info_show(vix, "Expecting boolean option value not: `%s'", argv[2]);
			return false;
		}
	} else if (opt->flags & VIX_OPTION_TYPE_NUMBER) {
		if (!argv[2]) {
			vix_info_show(vix, "Expecting number");
			return false;
		}
		char *ep;
		errno = 0;
		long lval = strtol(argv[2], &ep, 10);
		if (argv[2][0] == '\0' || *ep != '\0') {
			vix_info_show(vix, "Invalid number");
			return false;
		}

		if ((errno == ERANGE && (lval == LONG_MAX || lval == LONG_MIN)) ||
		    (lval > INT_MAX || lval < INT_MIN)) {
			vix_info_show(vix, "Number overflow");
			return false;
		}

		if (lval < 0) {
			vix_info_show(vix, "Expecting positive number");
			return false;
		}
		arg.i = lval;
	} else {
		return false;
	}

	size_t opt_index = 0;
	for (; opt_index < LENGTH(options); opt_index++) {
		if (opt == &options[opt_index]) {
			break;
		}
	}

	switch (opt_index) {
	case OPTION_SHELL:
		vix_shell_set(vix, arg.s);
		break;
	case OPTION_ESCDELAY:
	{
		termkey_set_waittime(vix->ui.termkey, arg.i);
		break;
	}
	case OPTION_EXPANDTAB:
		vix->win->expandtab = toggle ? !vix->win->expandtab : arg.b;
		break;
	case OPTION_AUTOINDENT:
		vix->autoindent = toggle ? !vix->autoindent : arg.b;
		break;
	case OPTION_TABWIDTH:
		view_tabwidth_set(&vix->win->view, arg.i);
		break;
	case OPTION_SHOW_SPACES:
	case OPTION_SHOW_TABS:
	case OPTION_SHOW_NEWLINES:
	case OPTION_SHOW_EOF:
	case OPTION_STATUSBAR:
	{
		const int values[] = {
			[OPTION_SHOW_SPACES] = UI_OPTION_SYMBOL_SPACE,
			[OPTION_SHOW_TABS] = UI_OPTION_SYMBOL_TAB|UI_OPTION_SYMBOL_TAB_FILL,
			[OPTION_SHOW_NEWLINES] = UI_OPTION_SYMBOL_EOL,
			[OPTION_SHOW_EOF] = UI_OPTION_SYMBOL_EOF,
			[OPTION_STATUSBAR] = UI_OPTION_STATUSBAR,
		};
		int flags = win->options;
		if (arg.b || (toggle && !(flags & values[opt_index]))) {
			flags |= values[opt_index];
		} else {
			flags &= ~values[opt_index];
		}
		win_options_set(win, flags);
		break;
	}
	case OPTION_NUMBER: {
		enum UiOption opt = win->options;
		if (arg.b || (toggle && !(opt & UI_OPTION_LINE_NUMBERS_ABSOLUTE))) {
			opt &= ~UI_OPTION_LINE_NUMBERS_RELATIVE;
			opt |=  UI_OPTION_LINE_NUMBERS_ABSOLUTE;
		} else {
			opt &= ~UI_OPTION_LINE_NUMBERS_ABSOLUTE;
		}
		win_options_set(win, opt);
		break;
	}
	case OPTION_NUMBER_RELATIVE: {
		enum UiOption opt = win->options;
		if (arg.b || (toggle && !(opt & UI_OPTION_LINE_NUMBERS_RELATIVE))) {
			opt &= ~UI_OPTION_LINE_NUMBERS_ABSOLUTE;
			opt |=  UI_OPTION_LINE_NUMBERS_RELATIVE;
		} else {
			opt &= ~UI_OPTION_LINE_NUMBERS_RELATIVE;
		}
		win_options_set(win, opt);
		break;
	}
	case OPTION_CURSOR_LINE: {
		enum UiOption opt = win->options;
		if (arg.b || (toggle && !(opt & UI_OPTION_CURSOR_LINE))) {
			opt |= UI_OPTION_CURSOR_LINE;
		} else {
			opt &= ~UI_OPTION_CURSOR_LINE;
		}
		win_options_set(win, opt);
		break;
	}
	case OPTION_COLOR_COLUMN:
		if (arg.i >= 0) {
			win->view.colorcolumn = arg.i;
		}
		break;
	case OPTION_SAVE_METHOD:
		if (strcmp("auto", arg.s) == 0) {
			win->file->save_method = TEXT_SAVE_AUTO;
		} else if (strcmp("atomic", arg.s) == 0) {
			win->file->save_method = TEXT_SAVE_ATOMIC;
		} else if (strcmp("inplace", arg.s) == 0) {
			win->file->save_method = TEXT_SAVE_INPLACE;
		} else {
			vix_info_show(vix, "Invalid save method `%s', expected "
			              "'auto', 'atomic' or 'inplace'", arg.s);
			return false;
		}
		break;
	case OPTION_LOAD_METHOD:
		if (strcmp("auto", arg.s) == 0) {
			vix->load_method = TEXT_LOAD_AUTO;
		} else if (strcmp("read", arg.s) == 0) {
			vix->load_method = TEXT_LOAD_READ;
		} else if (strcmp("mmap", arg.s) == 0) {
			vix->load_method = TEXT_LOAD_MMAP;
		} else {
			vix_info_show(vix, "Invalid load method `%s', expected "
			              "'auto', 'read' or 'mmap'", arg.s);
			return false;
		}
		break;
	case OPTION_CHANGE_256COLORS:
		vix->change_colors = toggle ? !vix->change_colors : arg.b;
		break;
	case OPTION_LAYOUT: {
		enum UiLayout layout;
		if (strcmp("h", arg.s) == 0) {
			layout = UI_LAYOUT_HORIZONTAL;
		} else if (strcmp("v", arg.s) == 0) {
			layout = UI_LAYOUT_VERTICAL;
		} else {
			vix_info_show(vix, "Invalid layout `%s', expected 'h' or 'v'", arg.s);
			return false;
		}
		ui_arrange(&vix->ui, layout);
		break;
	}
	case OPTION_IGNORECASE:
		vix->ignorecase = toggle ? !vix->ignorecase : arg.b;
		break;
	case OPTION_BREAKAT:
		if (!view_breakat_set(&win->view, arg.s)) {
			vix_info_show(vix, "Failed to set breakat");
			return false;
		}
		break;
	case OPTION_WRAP_COLUMN:
		if (arg.i >= 0) {
			win->view.wrapcolumn = arg.i;
		}
		break;
	default:
		if (!opt->func) {
			return false;
		}
		bool ret = opt->func(vix, win, opt->context, toggle, opt->flags, name, &arg);
		if (ret) {
			goto record;
		}
		return false;
	}

	goto record;

record:
	{
		lua_State *L = vix->lua;
		if (L) {
			lua_getfield(L, LUA_REGISTRYINDEX, "vix_session_changes");
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				lua_newtable(L);
				lua_pushvalue(L, -1);
				lua_setfield(L, LUA_REGISTRYINDEX, "vix_session_changes");
			}
			lua_pushstring(L, opt->names[0]);
			lua_pushboolean(L, true);
			lua_settable(L, -3);
			lua_pop(L, 1);
		}
	}

	return true;
}

static bool is_file_pattern(const char *pattern) {
	if (!pattern) {
		return false;
	}
	struct stat meta;
	if (stat(pattern, &meta) == 0 && S_ISDIR(meta.st_mode)) {
		return true;
	}
	/* tilde expansion is defined only for the tilde at the
	   beginning of the pattern. */
	if (pattern[0] == '~') {
		return true;
	}
	for (char special[] = "*?[{$", *s = special; *s; s++) {
		if ((strchr)(pattern, *s)) {
			return true;
		}
	}
	return false;
}

static const char *file_open_dialog(Vix *vix, const char *pattern) {
	static char name[PATH_MAX];
	name[0] = '\0';

	if (!is_file_pattern(pattern)) {
		return pattern;
	}

	Buffer bufcmd = {0}, bufout = {0}, buferr = {0};

	if (!buffer_put0(&bufcmd, VIX_OPEN " ") || !buffer_append0(&bufcmd, pattern ? pattern : "")) {
		return NULL;
	}

	Filerange empty = text_range_new(0,0);
	int status = vix_pipe(vix, vix->win->file, &empty,
		(const char*[]){ buffer_content0(&bufcmd), NULL },
		&bufout, read_into_buffer, &buferr, read_into_buffer, false);

	if (status == 0) {
		strncpy(name, buffer_content0(&bufout), sizeof(name)-1);
	} else if (status != 1) {
		vix_info_show(vix, "Command failed %s", buffer_content0(&buferr));
	}

	buffer_release(&bufcmd);
	buffer_release(&bufout);
	buffer_release(&buferr);

	for (char *end = name+strlen(name)-1; end >= name && isspace((unsigned char)*end); end--) {
		*end = '\0';
	}

	return name[0] ? name : NULL;
}

static bool openfiles(Vix *vix, const char **files) {
	for (; *files; files++) {
		const char *file = file_open_dialog(vix, *files);
		if (!file) {
			return false;
		}
		errno = 0;
		if (!vix_window_new(vix, file)) {
			vix_info_show(vix, "Could not open `%s' %s", file,
			                 errno ? strerror(errno) : "");
			return false;
		}
	}
	return true;
}

static bool cmd_open(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	if (!argv[1]) {
		return vix_window_new(vix, NULL);
	}
	return openfiles(vix, &argv[1]);
}

static void info_unsaved_changes(Vix *vix) {
	vix_info_show(vix, "No write since last change (add ! to override)");
}

static bool cmd_edit(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	if (argv[2]) {
		vix_info_show(vix, "Only 1 filename allowed");
		return false;
	}
	Win *oldwin = win;
	if (!oldwin) {
		return false;
	}
	if (cmd->flags != '!' && !vix_window_closable(oldwin)) {
		info_unsaved_changes(vix);
		return false;
	}
	if (!argv[1]) {
		if (oldwin->file->refcount > 1) {
			vix_info_show(vix, "Can not reload file being opened multiple times");
			return false;
		}
		return vix_window_reload(oldwin);
	}
	if (!openfiles(vix, &argv[1])) {
		return false;
	}
	if (vix->win != oldwin) {
		Win *newwin = vix->win;
		vix_window_swap(oldwin, newwin);
		vix_window_close(oldwin);
		vix_window_focus(newwin);
	}
	return vix->win != oldwin;
}

static bool cmd_read(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	bool ret = false;
	const size_t first_file = 3;
	const char *args[MAX_ARGV] = { argv[0], "cat", "--" };
	const char **name = argv[1] ? &argv[1] : (const char*[]){ ".", NULL };
	for (size_t i = first_file; *name && i < LENGTH(args)-1; name++, i++) {
		const char *file = file_open_dialog(vix, *name);
		if (!file || !(args[i] = strdup(file))) {
			goto err;
		}
	}
	args[LENGTH(args)-1] = NULL;
	ret = cmd_pipein(vix, win, cmd, args, sel, range);
err:
	for (size_t i = first_file; i < LENGTH(args); i++) {
		free((char*)args[i]);
	}
	return ret;
}

static bool has_windows(Vix *vix) {
	for (Win *win = vix->windows; win; win = win->next) {
		if (!win->file->internal) {
			return true;
		}
	}
	return false;
}

static bool cmd_quit(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	if (cmd->flags != '!' && !vix_window_closable(win)) {
		info_unsaved_changes(vix);
		return false;
	}
	vix_window_close(win);
	if (!has_windows(vix)) {
		vix_exit(vix, argv[1] ? atoi(argv[1]) : EXIT_SUCCESS);
	}
	return true;
}

static bool cmd_qall(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	for (Win *next, *win = vix->windows; win; win = next) {
		next = win->next;
		if (!win->file->internal && (!text_modified(win->file->text) || cmd->flags == '!')) {
			vix_window_close(win);
		}
	}
	if (!has_windows(vix)) {
		vix_exit(vix, argv[1] ? atoi(argv[1]) : EXIT_SUCCESS);
		return true;
	} else {
		info_unsaved_changes(vix);
		return false;
	}
}

static bool cmd_split(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	if (!win) {
		return false;
	}
	enum UiOption options = win->options;
	vix->ui.layout = UI_LAYOUT_HORIZONTAL;
	if (!argv[1]) {
		return vix_window_split(win);
	}
	bool ret = openfiles(vix, &argv[1]);
	if (ret) {
		win_options_set(vix->win, options);
	}
	return ret;
}

static bool cmd_vsplit(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	if (!win) {
		return false;
	}
	enum UiOption options = win->options;
	vix->ui.layout = UI_LAYOUT_VERTICAL;
	if (!argv[1]) {
		return vix_window_split(win);
	}
	bool ret = openfiles(vix, &argv[1]);
	if (ret) {
		win_options_set(vix->win, options);
	}
	return ret;
}

static bool cmd_new(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	ui_arrange(&vix->ui, UI_LAYOUT_HORIZONTAL);
	return vix_window_new(vix, NULL);
}

static bool cmd_vnew(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	ui_arrange(&vix->ui, UI_LAYOUT_VERTICAL);
	return vix_window_new(vix, NULL);
}

static bool cmd_wq(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	if (!win) {
		return false;
	}
	File *file = win->file;
	bool unmodified = file->fd == -1 && !file->name && !text_modified(file->text);
	if (unmodified || cmd_write(vix, win, cmd, argv, sel, range)) {
		return cmd_quit(vix, win, cmd, (const char*[]){argv[0], NULL}, sel, range);
	}
	return false;
}

static bool cmd_earlier_later(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	if (!win) {
		return false;
	}
	Text *txt = win->file->text;
	char *unit = "";
	long count = 1;
	size_t pos = EPOS;
	if (argv[1]) {
		errno = 0;
		count = strtol(argv[1], &unit, 10);
		if (errno || unit == argv[1] || count < 0) {
			vix_info_show(vix, "Invalid number");
			return false;
		}

		if (*unit) {
			while (*unit && isspace((unsigned char)*unit)) {
				unit++;
			}
			switch (*unit) {
			case 'd': count *= 24; /* fall through */
			case 'h': count *= 60; /* fall through */
			case 'm': count *= 60; /* fall through */
			case 's': break;
			default:
				vix_info_show(vix, "Unknown time specifier (use: s,m,h or d)");
				return false;
			}

			if (argv[0][0] == 'e') {
				count = -count; /* earlier, move back in time */
			}

			pos = text_restore(txt, text_state(txt) + count);
		}
	}

	if (!*unit) {
		VixCountIterator it = vix_count_iterator_init(vix, count);
		while (vix_count_iterator_next(&it)) {
			if (argv[0][0] == 'e') {
				pos = text_earlier(txt);
			} else {
				pos = text_later(txt);
			}
		}
	}

	struct tm tm;
	time_t state = text_state(txt);
	char buf[32];
	strftime(buf, sizeof buf, "State from %H:%M", localtime_r(&state, &tm));
	vix_info_show(vix, "%s", buf);

	return pos != EPOS;
}

static int space_replace(char *dest, const char *src, size_t dlen) {
	int invisiblebytes = 0;
	size_t i, size = LENGTH("␣") - 1;
	for (i = 0; *src && i < dlen; src++) {
		if (*src == ' ' && i < dlen - size - 1) {
			memcpy(&dest[i], "␣", size);
			i += size;
			invisiblebytes += size - 1;
		} else {
			dest[i] = *src;
			i++;
		}
	}
	dest[i] = '\0';
	return invisiblebytes;
}

static bool print_keylayout(const char *key, void *value, void *data)
{
	Vix  *vix = ((void **)data)[0];
	Text *txt = ((void **)data)[1];
	char buf[64];
	int invisiblebytes = space_replace(buf, key, sizeof(buf));
	return text_appendf(vix, txt, "  %-*s\t%s\n", 18+invisiblebytes, buf, (char*)value);
}

static bool print_keybinding(const char *key, void *value, void *data)
{
	KeyBinding *binding = value;
	const char *desc = binding->alias;
	if (!desc && binding->action) {
		desc = VIX_HELP_USE(binding->action->help);
	}

	Vix  *vix = ((void **)data)[0];
	Text *txt = ((void **)data)[1];
	char buf[64];
	int invisiblebytes = space_replace(buf, key, sizeof(buf));
	return text_appendf(vix, txt, "  %-*s\t%s\n", 18+invisiblebytes, buf, desc ? desc : "");
}

static void print_mode(Vix *vix, Text *txt, Mode *mode)
{
	if (!map_empty(mode->bindings)) {
		text_appendf(vix, txt, "\n %s\n\n", mode->name);
	}
	void *data[2] = {vix, txt};
	map_iterate(mode->bindings, print_keybinding, data);
}

static bool print_action(const char *key, void *value, void *data)
{
	const char *help = VIX_HELP_USE(((KeyAction*)value)->help);
	Vix  *vix = ((void **)data)[0];
	Text *txt = ((void **)data)[1];
	return text_appendf(vix, txt, "  %-30s\t%s\n", key, help ? help : "");
}

static bool print_cmd(const char *key, void *value, void *data)
{
	CommandDef *cmd = value;
	const char *help = VIX_HELP_USE(cmd->help);
	char usage[256];
	Vix  *vix = ((void **)data)[0];
	Text *txt = ((void **)data)[1];
	snprintf(usage, sizeof usage, "%s%s%s%s%s%s%s",
	         cmd->name,
	         (cmd->flags & CMD_FORCE) ? "[!]" : "",
	         (cmd->flags & CMD_TEXT) ? "/text/" : "",
	         (cmd->flags & CMD_REGEX) ? "/regexp/" : "",
	         (cmd->flags & CMD_CMD) ? " command" : "",
	         (cmd->flags & CMD_SHELL) ? (!strcmp(cmd->name, "s") ? "/regexp/text/" : " shell-command") : "",
	         (cmd->flags & CMD_ARGV) ? " [args...]" : "");
	return text_appendf(vix, txt, "  %-30s %s\n", usage, help ? help : "");
}

static bool print_cmd_name(const char *key, void *value, void *data) {
	CommandDef *cmd = value;
	bool result = buffer_append(data, cmd->name, strlen(cmd->name));
	return result && buffer_append(data, "\n", 1);
}

static bool print_option_name(const char *key, void *value, void *data) {
	bool result = buffer_append(data, key, strlen(key));
	return result && buffer_append(data, "\n", 1);
}

void vix_print_cmds(Vix *vix, Buffer *buf, const char *prefix) {
	map_iterate(map_prefix(vix->cmds, prefix), print_cmd_name, buf);
}

void vix_print_options(Vix *vix, Buffer *buf, const char *prefix) {
	map_iterate(map_prefix(vix->options, prefix), print_option_name, buf);
}

void vix_print_option_value(Vix *vix, const char *name, Buffer *buf) {
	OptionDef *opt = map_get(vix->options, name);
	if (!opt) {
		return;
	}

	Win *win = vix->win;
	size_t opt_index = 0;
	for (; opt_index < LENGTH(options); opt_index++) {
		if (opt == &options[opt_index]) {
			break;
		}
	}

	switch (opt_index) {
	case OPTION_SHELL:
		buffer_append0(buf, vix->shell);
		break;
	case OPTION_ESCDELAY:
		buffer_appendf(buf, "%d", termkey_get_waittime(vix->ui.termkey));
		break;
	case OPTION_EXPANDTAB:
		buffer_append0(buf, win && win->expandtab ? "on" : "off");
		break;
	case OPTION_AUTOINDENT:
		buffer_append0(buf, vix->autoindent ? "on" : "off");
		break;
	case OPTION_TABWIDTH:
		if (win) {
			buffer_appendf(buf, "%d", win->view.tabwidth);
		}
		break;
	case OPTION_SHOW_SPACES:
		buffer_append0(buf, win && (win->options & UI_OPTION_SYMBOL_SPACE) ? "on" : "off");
		break;
	case OPTION_SHOW_TABS:
		buffer_append0(buf, win && (win->options & UI_OPTION_SYMBOL_TAB) ? "on" : "off");
		break;
	case OPTION_SHOW_NEWLINES:
		buffer_append0(buf, win && (win->options & UI_OPTION_SYMBOL_EOL) ? "on" : "off");
		break;
	case OPTION_SHOW_EOF:
		buffer_append0(buf, win && (win->options & UI_OPTION_SYMBOL_EOF) ? "on" : "off");
		break;
	case OPTION_STATUSBAR:
		buffer_append0(buf, win && (win->options & UI_OPTION_STATUSBAR) ? "on" : "off");
		break;
	case OPTION_NUMBER:
		buffer_append0(buf, win && (win->options & UI_OPTION_LINE_NUMBERS_ABSOLUTE) ? "on" : "off");
		break;
	case OPTION_NUMBER_RELATIVE:
		buffer_append0(buf, win && (win->options & UI_OPTION_LINE_NUMBERS_RELATIVE) ? "on" : "off");
		break;
	case OPTION_CURSOR_LINE:
		buffer_append0(buf, win && (win->options & UI_OPTION_CURSOR_LINE) ? "on" : "off");
		break;
	case OPTION_COLOR_COLUMN:
		if (win) {
			buffer_appendf(buf, "%d", win->view.colorcolumn);
		}
		break;
	case OPTION_SAVE_METHOD:
		if (win) {
			switch (win->file->save_method) {
			case TEXT_SAVE_AUTO: buffer_append0(buf, "auto"); break;
			case TEXT_SAVE_ATOMIC: buffer_append0(buf, "atomic"); break;
			case TEXT_SAVE_INPLACE: buffer_append0(buf, "inplace"); break;
			}
		}
		break;
	case OPTION_LOAD_METHOD:
		switch (vix->load_method) {
		case TEXT_LOAD_AUTO: buffer_append0(buf, "auto"); break;
		case TEXT_LOAD_READ: buffer_append0(buf, "read"); break;
		case TEXT_LOAD_MMAP: buffer_append0(buf, "mmap"); break;
		}
		break;
	case OPTION_CHANGE_256COLORS:
		buffer_append0(buf, vix->change_colors ? "on" : "off");
		break;
	case OPTION_LAYOUT:
		buffer_append0(buf, vix->ui.layout == UI_LAYOUT_HORIZONTAL ? "h" : "v");
		break;
	case OPTION_IGNORECASE:
		buffer_append0(buf, vix->ignorecase ? "on" : "off");
		break;
	default:
		{
			lua_State *L = vix->lua;
			if (L) {
				lua_getfield(L, LUA_REGISTRYINDEX, "vix_option_values");
				if (!lua_isnil(L, -1)) {
					lua_pushstring(L, opt->names[0]);
					lua_gettable(L, -2);
					if (lua_isboolean(L, -1)) {
						buffer_append0(buf, lua_toboolean(L, -1) ? "on" : "off");
					} else if (lua_isstring(L, -1)) {
						buffer_append0(buf, lua_tostring(L, -1));
					} else if (lua_isnumber(L, -1)) {
						buffer_appendf(buf, "%ld", (long)lua_tointeger(L, -1));
					}
					lua_pop(L, 1);
				}
				lua_pop(L, 1);
			}
		}
		break;
	}
}

static bool print_option(const char *key, void *value, void *data)
{
	char desc[256];
	const OptionDef *opt = value;
	const char *help = VIX_HELP_USE(opt->help);
	if (strcmp(key, opt->names[0])) {
		return true;
	}
	snprintf(desc, sizeof desc, "%s%s%s%s%s",
	         opt->names[0],
	         opt->names[1] ? "|" : "",
	         opt->names[1] ? opt->names[1] : "",
	         opt->flags & VIX_OPTION_TYPE_BOOL ? " on|off" : "",
	         opt->flags & VIX_OPTION_TYPE_NUMBER ? " nn" : "");
	Vix  *vix = ((void **)data)[0];
	Text *txt = ((void **)data)[1];
	return text_appendf(vix, txt, "  %-30s %s\n", desc, help ? help : "");
}

static void print_symbolic_keys(Vix *vix, Text *txt)
{
	static const int keys[] = {
		TERMKEY_SYM_BACKSPACE,
		TERMKEY_SYM_TAB,
		TERMKEY_SYM_ENTER,
		TERMKEY_SYM_ESCAPE,
		//TERMKEY_SYM_SPACE,
		TERMKEY_SYM_DEL,
		TERMKEY_SYM_UP,
		TERMKEY_SYM_DOWN,
		TERMKEY_SYM_LEFT,
		TERMKEY_SYM_RIGHT,
		TERMKEY_SYM_BEGIN,
		TERMKEY_SYM_FIND,
		TERMKEY_SYM_INSERT,
		TERMKEY_SYM_DELETE,
		TERMKEY_SYM_SELECT,
		TERMKEY_SYM_PAGEUP,
		TERMKEY_SYM_PAGEDOWN,
		TERMKEY_SYM_HOME,
		TERMKEY_SYM_END,
		TERMKEY_SYM_CANCEL,
		TERMKEY_SYM_CLEAR,
		TERMKEY_SYM_CLOSE,
		TERMKEY_SYM_COMMAND,
		TERMKEY_SYM_COPY,
		TERMKEY_SYM_EXIT,
		TERMKEY_SYM_HELP,
		TERMKEY_SYM_MARK,
		TERMKEY_SYM_MESSAGE,
		TERMKEY_SYM_MOVE,
		TERMKEY_SYM_OPEN,
		TERMKEY_SYM_OPTIONS,
		TERMKEY_SYM_PRINT,
		TERMKEY_SYM_REDO,
		TERMKEY_SYM_REFERENCE,
		TERMKEY_SYM_REFRESH,
		TERMKEY_SYM_REPLACE,
		TERMKEY_SYM_RESTART,
		TERMKEY_SYM_RESUME,
		TERMKEY_SYM_SAVE,
		TERMKEY_SYM_SUSPEND,
		TERMKEY_SYM_UNDO,
		TERMKEY_SYM_KP0,
		TERMKEY_SYM_KP1,
		TERMKEY_SYM_KP2,
		TERMKEY_SYM_KP3,
		TERMKEY_SYM_KP4,
		TERMKEY_SYM_KP5,
		TERMKEY_SYM_KP6,
		TERMKEY_SYM_KP7,
		TERMKEY_SYM_KP8,
		TERMKEY_SYM_KP9,
		TERMKEY_SYM_KPENTER,
		TERMKEY_SYM_KPPLUS,
		TERMKEY_SYM_KPMINUS,
		TERMKEY_SYM_KPMULT,
		TERMKEY_SYM_KPDIV,
		TERMKEY_SYM_KPCOMMA,
		TERMKEY_SYM_KPPERIOD,
		TERMKEY_SYM_KPEQUALS,
	};

	TermKey *termkey = vix->ui.termkey;
	text_appendf(vix, txt, "  ␣ (a literal \" \" space symbol must be used to refer to <Space>)\n");
	for (size_t i = 0; i < LENGTH(keys); i++) {
		text_appendf(vix, txt, "  <%s>\n", termkey_get_keyname(termkey, keys[i]));
	}
}

static bool cmd_help(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	if (!vix_window_new(vix, NULL)) {
		return false;
	}

	Text *txt = vix->win->file->text;
	void *map_data[2] = {vix, txt};

	text_appendf(vix, txt, "vix %s (PID: %ld)\n\n", VERSION, (long)getpid());

	text_appendf(vix, txt, " Modes\n\n");
	for (int i = 0; i < LENGTH(vix_modes); i++) {
		Mode *mode = &vix_modes[i];
		if (mode->help) {
			text_appendf(vix, txt, "  %-18s\t%s\n", mode->name, mode->help);
		}
	}

	if (!map_empty(vix->keymap)) {
		text_appendf(vix, txt, "\n Layout specific mappings (affects all modes except INSERT/REPLACE)\n\n");
		map_iterate(vix->keymap, print_keylayout, map_data);
	}

	print_mode(vix, txt, &vix_modes[VIX_MODE_NORMAL]);
	print_mode(vix, txt, &vix_modes[VIX_MODE_OPERATOR_PENDING]);
	print_mode(vix, txt, &vix_modes[VIX_MODE_VISUAL]);
	print_mode(vix, txt, &vix_modes[VIX_MODE_INSERT]);

	text_appendf(vix, txt, "\n :-Commands\n\n");
	map_iterate(vix->cmds, print_cmd, map_data);

	text_appendf(vix, txt, "\n Marks\n\n");
	text_appendf(vix, txt, "  a-z General purpose marks\n");
	for (size_t i = 0; i < LENGTH(vix_marks); i++) {
		const char *help = VIX_HELP_USE(vix_marks[i].help);
		text_appendf(vix, txt, "  %c   %s\n", vix_marks[i].name, help ? help : "");
	}

	text_appendf(vix, txt, "\n Registers\n\n");
	text_appendf(vix, txt, "  a-z General purpose registers\n");
	text_appendf(vix, txt, "  A-Z Append to corresponding general purpose register\n");
	for (size_t i = 0; i < LENGTH(vix_registers); i++) {
		const char *help = VIX_HELP_USE(vix_registers[i].help);
		text_appendf(vix, txt, "  %c   %s\n", vix_registers[i].name, help ? help : "");
	}

	text_appendf(vix, txt, "\n :set command options\n\n");
	map_iterate(vix->options, print_option, map_data);

	text_appendf(vix, txt, "\n Key binding actions\n\n");
	map_iterate(vix->actions, print_action, map_data);

	text_appendf(vix, txt, "\n Symbolic keys usable for key bindings "
		"(prefix with C-, S-, and M- for Ctrl, Shift and Alt respectively)\n\n");
	print_symbolic_keys(vix, txt);

	char *paths[] = { NULL, NULL };
	char *paths_description[] = {
		"Lua paths used to load runtime files (? will be replaced by filename):",
		"Lua paths used to load C libraries (? will be replaced by filename):",
	};

	if (vix_lua_paths_get(vix, &paths[0], &paths[1])) {
		for (size_t i = 0; i < LENGTH(paths); i++) {
			text_appendf(vix, txt, "\n %s\n\n", paths_description[i]);
			for (char *elem = paths[i], *next; elem; elem = next) {
				if ((next = (strstr)(elem, ";"))) {
					*next++ = '\0';
				}
				if (*elem) {
					text_appendf(vix, txt, "  %s\n", elem);
				}
			}
			free(paths[i]);
		}
	}

	text_appendf(vix, txt, "\n Compile time configuration\n\n");

	const struct {
		const char *name;
		bool enabled;
	} configs[] = {
		{ "Curses support: ", CONFIG_CURSES },
		{ "Lua support: ", CONFIG_LUA },
		{ "Lua LPeg statically built-in: ", CONFIG_LPEG },
		{ "TRE based regex support: ", CONFIG_TRE },
		{ "POSIX ACL support: ", CONFIG_ACL },
		{ "SELinux support: ", CONFIG_SELINUX },
	};

	for (size_t i = 0; i < LENGTH(configs); i++) {
		text_appendf(vix, txt, "  %-32s\t%s\n", configs[i].name, configs[i].enabled ? "yes" : "no");
	}

	text_mark_current_revision(txt);
	view_cursors_to(vix->win->view.selection, 0);

	if (argv[1]) {
		vix_motion(vix, VIX_MOVE_SEARCH_FORWARD, argv[1]);
	}
	return true;
}

static bool cmd_langmap(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	const char *nonlatin = argv[1];
	const char *latin = argv[2];
	bool mapped = true;

	if (!latin || !nonlatin) {
		vix_info_show(vix, "usage: langmap <non-latin keys> <latin keys>");
		return false;
	}

	while (*latin && *nonlatin) {
		size_t i = 0, j = 0;
		char latin_key[8], nonlatin_key[8];
		do {
			if (i < sizeof(latin_key)-1) {
				latin_key[i++] = *latin;
			}
			latin++;
		} while (!ISUTF8(*latin));
		do {
			if (j < sizeof(nonlatin_key)-1) {
				nonlatin_key[j++] = *nonlatin;
			}
			nonlatin++;
		} while (!ISUTF8(*nonlatin));
		latin_key[i] = '\0';
		nonlatin_key[j] = '\0';
		mapped &= vix_keymap_add(vix, nonlatin_key, strdup(latin_key));
	}

	return mapped;
}

static bool cmd_map(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	bool mapped = false;
	bool local = (strstr)(argv[0], "-") != NULL;
	enum VixMode mode = vix_mode_from(vix, argv[1]);

	if (local && !win) {
		vix_info_show(vix, "Invalid window for :%s", argv[0]);
		return false;
	}

	if (mode == VIX_MODE_INVALID || !argv[2] || !argv[3]) {
		vix_info_show(vix, "usage: %s mode lhs rhs", argv[0]);
		return false;
	}

	const char *lhs = argv[2];
	KeyBinding *binding = vix_binding_new(vix);
	if (!binding || !(binding->alias = strdup(argv[3]))) {
		goto err;
	}

	if (local) {
		mapped = vix_window_mode_map(win, mode, cmd->flags == '!', lhs, binding);
	} else {
		mapped = vix_mode_map(vix, mode, cmd->flags == '!', lhs, binding);
	}

err:
	if (!mapped) {
		vix_info_show(vix, "Failed to map `%s' in %s mode%s", lhs, argv[1],
		              cmd->flags != '!' ? ", mapping already exists, "
		              "override with `!'" : "");
		vix_binding_free(vix, binding);
	}
	return mapped;
}

static bool cmd_unmap(Vix *vix, Win *win, Command *cmd, const char *argv[], Selection *sel, Filerange *range) {
	bool unmapped = false;
	bool local = (strstr)(argv[0], "-") != NULL;
	enum VixMode mode = vix_mode_from(vix, argv[1]);
	const char *lhs = argv[2];

	if (local && !win) {
		vix_info_show(vix, "Invalid window for :%s", argv[0]);
		return false;
	}

	if (mode == VIX_MODE_INVALID || !lhs) {
		vix_info_show(vix, "usage: %s mode lhs", argv[0]);
		return false;
	}

	if (local) {
		unmapped = vix_window_mode_unmap(win, mode, lhs);
	} else {
		unmapped = vix_mode_unmap(vix, mode, lhs);
	}
	if (!unmapped) {
		vix_info_show(vix, "Failed to unmap `%s' in %s mode", lhs, argv[1]);
	}
	return unmapped;
}
