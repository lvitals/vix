#include "vix-core.h"
#include "text-motions.h"
#include "util.h"

static void keyaction_free(KeyAction *action)
{
	free((char*)action->name);
	free(VIX_HELP_USE((char*)action->help));
	free(action);
}

KeyAction *vix_action_new(Vix *vix, const char *name, const char *help, KeyActionFunction *func, Arg arg)
{
	KeyAction *action = calloc(1, sizeof *action);
	if (!action)
		return 0;
	if (name && !(action->name = strdup(name)))
		goto err;
#if CONFIG_HELP
	if (help && !(action->help = strdup(help)))
		goto err;
#endif
	action->func = func;
	action->arg = arg;
	*da_push(vix, &vix->actions_user) = action;
	return action;
err:
	keyaction_free(action);
	return 0;
}

void vix_action_free(Vix *vix, KeyAction *action)
{
	if (action) {
		for (VixDACount i = 0; i < vix->actions_user.count; i++) {
			if (action == vix->actions_user.data[i]) {
				keyaction_free(action);
				vix->actions_user.data[i] = 0;
				da_unordered_remove(&vix->actions_user, i);
				break;
			}
		}
	}
}

KeyBinding *vix_binding_new(Vix *vix)
{
	KeyBinding *binding = calloc(1, sizeof *binding);
	if (binding)
		*da_push(vix, &vix->bindings) = binding;
	return binding;
}

void vix_binding_free(Vix *vix, KeyBinding *binding)
{
	if (binding) {
		for (VixDACount i = 0; i < vix->bindings.count; i++) {
			if (binding == vix->bindings.data[i]) {
				if (binding->alias)
					free((char*)binding->alias);
				if (binding->action && !binding->action->name)
					vix_action_free(vix, (KeyAction*)binding->action);
				free(binding);
				vix->bindings.data[i] = 0;
				da_unordered_remove(&vix->bindings, i);
				break;
			}
		}
	}
}

Mode *mode_get(Vix *vix, enum VixMode mode) {
	if (mode < LENGTH(vix_modes))
		return &vix_modes[mode];
	return NULL;
}

void mode_set(Vix *vix, Mode *new_mode) {
	if (vix->mode == new_mode)
		return;
	if (vix->mode->leave)
		vix->mode->leave(vix, new_mode);
	if (vix->mode != &vix_modes[VIX_MODE_OPERATOR_PENDING])
		vix->mode_prev = vix->mode;
	vix->mode = new_mode;
	if (new_mode->enter)
		new_mode->enter(vix, vix->mode_prev);
}

void vix_mode_switch(Vix *vix, enum VixMode mode) {
	if (mode < LENGTH(vix_modes))
		mode_set(vix, &vix_modes[mode]);
}

enum VixMode vix_mode_from(Vix *vix, const char *name) {
	for (size_t i = 0; name && i < LENGTH(vix_modes); i++) {
		Mode *mode = &vix_modes[i];
		if (!strcasecmp(mode->name, name))
			return mode->id;
	}
	return VIX_MODE_INVALID;
}

static bool mode_unmap(Mode *mode, const char *key) {
	return mode && mode->bindings && map_delete(mode->bindings, key);
}

bool vix_mode_unmap(Vix *vix, enum VixMode id, const char *key) {
	return id < LENGTH(vix_modes) && mode_unmap(&vix_modes[id], key);
}

bool vix_window_mode_unmap(Win *win, enum VixMode id, const char *key) {
	return id < LENGTH(win->modes) && mode_unmap(&win->modes[id], key);
}

static bool mode_map(Vix *vix, Mode *mode, bool force, const char *key, const KeyBinding *binding) {
	if (!mode)
		return false;
	if (binding->alias && key[0] != '<' && strncmp(key, binding->alias, strlen(key)) == 0)
		return false;
	if (!mode->bindings && !(mode->bindings = map_new()))
		return false;
	if (force)
		map_delete(mode->bindings, key);
	return map_put(mode->bindings, key, binding);
}

bool vix_mode_map(Vix *vix, enum VixMode id, bool force, const char *key, const KeyBinding *binding) {
	return id < LENGTH(vix_modes) && mode_map(vix, &vix_modes[id], force, key, binding);
}

bool vix_window_mode_map(Win *win, enum VixMode id, bool force, const char *key, const KeyBinding *binding) {
	return id < LENGTH(win->modes) && mode_map(win->vix, &win->modes[id], force, key, binding);
}

/** mode switching event handlers */

static void vix_mode_normal_enter(Vix *vix, Mode *old) {
	Win *win = vix->win;
	if (!win)
		return;
	if (old != mode_get(vix, VIX_MODE_INSERT) && old != mode_get(vix, VIX_MODE_REPLACE))
		return;
	if (vix->autoindent && strcmp(vix->key_prev, "<Enter>") == 0) {
		Text *txt = win->file->text;
		for (Selection *s = view_selections(&win->view); s; s = view_selections_next(s)) {
			size_t pos = view_cursors_pos(s);
			size_t start = text_line_start(txt, pos);
			size_t end = text_line_end(txt, pos);
			if (start == pos && start == end) {
				size_t begin = text_line_begin(txt, pos);
				size_t len = start - begin;
				if (len) {
					text_delete(txt, begin, len);
					view_cursors_to(s, pos-len);
				}
			}
		}
	}
	macro_operator_stop(vix);
	if (!win->parent && vix->action_prev.op == &vix_operators[VIX_OP_MODESWITCH] &&
	    vix->action_prev.count > 1) {
		/* temporarily disable motion, in something like `5atext`
		 * we should only move the cursor once then insert the text */
		const Movement *motion = vix->action_prev.movement;
		if (motion)
			vix->action_prev.movement = &vix_motions[VIX_MOVE_NOP];
		/* we already inserted the text once, so temporarily decrease count */
		vix->action_prev.count--;
		vix_repeat(vix);
		vix->action_prev.count++;
		vix->action_prev.movement = motion;
	}
	/* make sure we can recover the current state after an editing operation */
	vix_file_snapshot(vix, win->file);
}

static void vix_mode_operator_input(Vix *vix, const char *str, size_t len) {
	/* invalid operator */
	vix_cancel(vix);
	mode_set(vix, vix->mode_prev);
}

static void vix_mode_visual_enter(Vix *vix, Mode *old) {
	Win *win = vix->win;
	if (!old->visual && win) {
		for (Selection *s = view_selections(&win->view); s; s = view_selections_next(s))
			s->anchored = true;
	}
}

static void vix_mode_visual_line_enter(Vix *vix, Mode *old) {
	Win *win = vix->win;
	if (!old->visual && win) {
		for (Selection *s = view_selections(&win->view); s; s = view_selections_next(s))
			s->anchored = true;
	}
	if (!vix->action.op)
		vix_motion(vix, VIX_MOVE_NOP);
}

static void vix_mode_visual_line_leave(Vix *vix, Mode *new) {
	Win *win = vix->win;
	if (!win)
		return;
	if (!new->visual) {
		if (!vix->action.op)
			window_selection_save(win);
		view_selections_clear_all(&win->view);
	} else {
		view_cursors_to(win->view.selection, view_cursor_get(&win->view));
	}
}

static void vix_mode_visual_leave(Vix *vix, Mode *new) {
	Win *win = vix->win;
	if (!new->visual && win) {
		if (!vix->action.op)
			window_selection_save(win);
		view_selections_clear_all(&win->view);
	}
}

static void vix_mode_insert_replace_enter(Vix *vix, Mode *old) {
	if (!vix->win || vix->win->parent)
		return;
	if (!vix->action.op) {
		action_reset(&vix->action_prev);
		vix->action_prev.op = &vix_operators[VIX_OP_MODESWITCH];
		vix->action_prev.mode = vix->mode->id;
	}
	macro_operator_record(vix);
}

static void vix_mode_insert_idle(Vix *vix) {
	Win *win = vix->win;
	if (win)
		vix_file_snapshot(vix, win->file);
}

Mode vix_modes[] = {
	[VIX_MODE_OPERATOR_PENDING] = {
		.id = VIX_MODE_OPERATOR_PENDING,
		.name = "OPERATOR-PENDING",
		.input = vix_mode_operator_input,
		.help = "",
	},
	[VIX_MODE_NORMAL] = {
		.id = VIX_MODE_NORMAL,
		.name = "NORMAL",
		.help = "",
		.enter = vix_mode_normal_enter,
	},
	[VIX_MODE_VISUAL] = {
		.id = VIX_MODE_VISUAL,
		.name = "VISUAL",
		.status = "VISUAL",
		.help = "",
		.enter = vix_mode_visual_enter,
		.leave = vix_mode_visual_leave,
		.visual = true,
	},
	[VIX_MODE_VISUAL_LINE] = {
		.id = VIX_MODE_VISUAL_LINE,
		.name = "VISUAL-LINE",
		.parent = &vix_modes[VIX_MODE_VISUAL],
		.status = "VISUAL-LINE",
		.help = "",
		.enter = vix_mode_visual_line_enter,
		.leave = vix_mode_visual_line_leave,
		.visual = true,
	},
	[VIX_MODE_INSERT] = {
		.id = VIX_MODE_INSERT,
		.name = "INSERT",
		.status = "INSERT",
		.help = "",
		.enter = vix_mode_insert_replace_enter,
		.input = vix_insert_key,
		.idle = vix_mode_insert_idle,
		.idle_timeout = 3,
	},
	[VIX_MODE_REPLACE] = {
		.id = VIX_MODE_REPLACE,
		.name = "REPLACE",
		.parent = &vix_modes[VIX_MODE_INSERT],
		.status = "REPLACE",
		.help = "",
		.enter = vix_mode_insert_replace_enter,
		.input = vix_replace_key,
		.idle = vix_mode_insert_idle,
		.idle_timeout = 3,
	},
};

