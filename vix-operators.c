#include "vix-core.h"
#include "text-motions.h"
#include "text-objects.h"
#include "text-util.h"
#include "util.h"

static size_t op_delete(Vix *vix, Text *txt, OperatorContext *c) {
	c->reg->linewise = c->linewise;
	register_slot_put_range(vix, c->reg, c->reg_slot, txt, &c->range);
	text_delete_range(txt, &c->range);
	size_t pos = c->range.start;
	if (c->linewise && pos == text_size(txt))
		pos = text_line_begin(txt, text_line_prev(txt, pos));
	return pos;
}

static size_t op_change(Vix *vix, Text *txt, OperatorContext *c) {
	bool linewise = c->linewise || text_range_is_linewise(txt, &c->range);
	op_delete(vix, txt, c);
	size_t pos = c->range.start;
	if (linewise) {
		size_t newpos = vix_text_insert_nl(vix, txt, pos > 0 ? pos-1 : pos);
		if (pos > 0)
			pos = newpos;
	}
	return pos;
}

static size_t op_yank(Vix *vix, Text *txt, OperatorContext *c) {
	c->reg->linewise = c->linewise;
	register_slot_put_range(vix, c->reg, c->reg_slot, txt, &c->range);
	if (c->reg == &vix->registers[VIX_REG_DEFAULT]) {
		vix->registers[VIX_REG_ZERO].linewise = c->reg->linewise;
		register_slot_put_range(vix, &vix->registers[VIX_REG_ZERO], c->reg_slot, txt, &c->range);
	}
	return c->linewise ? c->pos : c->range.start;
}

static size_t op_put(Vix *vix, Text *txt, OperatorContext *c) {
	char b;
	size_t pos = c->pos;
	bool sel = text_range_size(&c->range) > 0;
	bool sel_linewise = sel && text_range_is_linewise(txt, &c->range);
	if (sel) {
		text_delete_range(txt, &c->range);
		pos = c->pos = c->range.start;
	}
	switch (c->arg->i) {
	case VIX_OP_PUT_AFTER:
	case VIX_OP_PUT_AFTER_END:
		if (c->reg->linewise && !sel_linewise)
			pos = text_line_next(txt, pos);
		else if (!sel && text_byte_get(txt, pos, &b) && b != '\n')
			pos = text_char_next(txt, pos);
		break;
	case VIX_OP_PUT_BEFORE:
	case VIX_OP_PUT_BEFORE_END:
		if (c->reg->linewise)
			pos = text_line_begin(txt, pos);
		break;
	}

	size_t len;
	const char *data = register_slot_get(vix, c->reg, c->reg_slot, &len);

	for (int i = 0; i < c->count; i++) {
		char nl;
		if (c->reg->linewise && pos > 0 && text_byte_get(txt, pos-1, &nl) && nl != '\n')
			pos += text_insert(vix, txt, pos, "\n", 1);
		text_insert(vix, txt, pos, data, len);
		pos += len;
		if (c->reg->linewise && pos > 0 && text_byte_get(txt, pos-1, &nl) && nl != '\n')
			pos += text_insert(vix, txt, pos, "\n", 1);
	}

	if (c->reg->linewise) {
		switch (c->arg->i) {
		case VIX_OP_PUT_BEFORE_END:
		case VIX_OP_PUT_AFTER_END:
			pos = text_line_start(txt, pos);
			break;
		case VIX_OP_PUT_AFTER:
			pos = text_line_start(txt, text_line_next(txt, c->pos));
			break;
		case VIX_OP_PUT_BEFORE:
			pos = text_line_start(txt, c->pos);
			break;
		}
	} else {
		switch (c->arg->i) {
		case VIX_OP_PUT_AFTER:
		case VIX_OP_PUT_BEFORE:
			pos = text_char_prev(txt, pos);
			break;
		}
	}

	return pos;
}

static size_t op_shift_right(Vix *vix, Text *txt, OperatorContext *c) {
	char spaces[9] = "        ";
	spaces[MIN(vix->win->view.tabwidth, LENGTH(spaces) - 1)] = '\0';
	const char *tab = vix->win->expandtab ? spaces : "\t";
	size_t tablen = strlen(tab);
	size_t pos = text_line_begin(txt, c->range.end), prev_pos;
	size_t newpos = c->pos;

	/* if range ends at the begin of a line, skip line break */
	if (pos == c->range.end)
		pos = text_line_prev(txt, pos);
	bool multiple_lines = text_line_prev(txt, pos) >= c->range.start;

	do {
		size_t end = text_line_end(txt, pos);
		prev_pos = pos = text_line_begin(txt, end);
		if ((!multiple_lines || pos != end) &&
		    text_insert(vix, txt, pos, tab, tablen) && pos <= c->pos)
			newpos += tablen;
		pos = text_line_prev(txt, pos);
	}  while (pos >= c->range.start && pos != prev_pos);

	return newpos;
}

static size_t op_shift_left(Vix *vix, Text *txt, OperatorContext *c) {
	size_t pos = text_line_begin(txt, c->range.end), prev_pos;
	size_t tabwidth = vix->win->view.tabwidth, tablen;
	size_t newpos = c->pos;

	/* if range ends at the begin of a line, skip line break */
	if (pos == c->range.end)
		pos = text_line_prev(txt, pos);

	do {
		char b;
		size_t len = 0;
		prev_pos = pos = text_line_begin(txt, pos);
		Iterator it = text_iterator_get(txt, pos);
		if (text_iterator_byte_get(&it, &b) && b == '\t') {
			len = 1;
		} else {
			for (len = 0; text_iterator_byte_get(&it, &b) && b == ' '; len++)
				text_iterator_byte_next(&it, NULL);
		}
		tablen = MIN(len, tabwidth);
		if (text_delete(txt, pos, tablen) && pos < c->pos) {
			size_t delta = c->pos - pos;
			if (delta > tablen)
				delta = tablen;
			if (delta > newpos)
				delta = newpos;
			newpos -= delta;
		}
		pos = text_line_prev(txt, pos);
	}  while (pos >= c->range.start && pos != prev_pos);

	return newpos;
}

static size_t op_cursor(Vix *vix, Text *txt, OperatorContext *c) {
	Filerange r = text_range_linewise(txt, &c->range);
	for (size_t line = text_range_line_first(txt, &r); line != EPOS; line = text_range_line_next(txt, &r, line)) {
		size_t pos;
		if (c->arg->i == VIX_OP_CURSOR_EOL)
			pos = text_line_finish(txt, line);
		else
			pos = text_line_start(txt, line);
		view_selections_new_force(&vix->win->view, pos);
	}
	return EPOS;
}

static size_t op_join(Vix *vix, Text *txt, OperatorContext *c) {
	size_t pos = text_line_begin(txt, c->range.end), prev_pos;
	Mark mark = EMARK;

	/* if operator and range are both linewise, skip last line break */
	if (c->linewise && text_range_is_linewise(txt, &c->range)) {
		size_t line_prev = text_line_prev(txt, pos);
		size_t line_prev_prev = text_line_prev(txt, line_prev);
		if (line_prev_prev >= c->range.start)
			pos = line_prev;
	}

	size_t len = c->arg->s ? strlen(c->arg->s) : 0;

	do {
		prev_pos = pos;
		size_t end = text_line_start(txt, pos);
		pos = text_line_prev(txt, end);
		if (pos < c->range.start || end <= pos)
			break;
		text_delete(txt, pos, end - pos);
		char prev, next;
		if (text_byte_get(txt, pos-1, &prev) && !isspace((unsigned char)prev) &&
		    text_byte_get(txt, pos, &next) && next != '\n')
			text_insert(vix, txt, pos, c->arg->s, len);
		if (mark == EMARK)
			mark = text_mark_set(txt, pos);
	} while (pos != prev_pos);

	size_t newpos = text_mark_get(txt, mark);
	return newpos != EPOS ? newpos : c->range.start;
}

static size_t op_modeswitch(Vix *vix, Text *txt, OperatorContext *c) {
	return c->newpos != EPOS ? c->newpos : c->pos;
}

static size_t op_replace(Vix *vix, Text *txt, OperatorContext *c) {
	size_t count = 0;
	Iterator it = text_iterator_get(txt, c->range.start);
	while (it. pos < c->range.end && text_iterator_char_next(&it, NULL))
		count++;
	op_delete(vix, txt, c);
	size_t pos = c->range.start;
	for (size_t len = strlen(c->arg->s); count > 0; pos += len, count--)
		text_insert(vix, txt, pos, c->arg->s, len);
	return c->range.start;
}

int vix_operator_register(Vix *vix, VixOperatorFunction *func, void *context)
{
	*da_push(vix, &vix->operators) = (Operator){
		.func    = func,
		.context = context,
	};
	return VIX_OP_LAST + vix->operators.count - 1;
}

bool vix_operator(Vix *vix, enum VixOperator id, ...)
{
	bool result = true;
	va_list ap;
	va_start(ap, id);

	switch (id) {
	case VIX_OP_MODESWITCH:
		vix->action.mode = va_arg(ap, int);
		break;
	case VIX_OP_CURSOR_SOL:
	case VIX_OP_CURSOR_EOL:
		vix->action.arg.i = id;
		id = VIX_OP_CURSOR_SOL;
		break;
	case VIX_OP_PUT_AFTER:
	case VIX_OP_PUT_AFTER_END:
	case VIX_OP_PUT_BEFORE:
	case VIX_OP_PUT_BEFORE_END:
		vix->action.arg.i = id;
		id = VIX_OP_PUT_AFTER;
		break;
	case VIX_OP_JOIN:
		vix->action.arg.s = va_arg(ap, char*);
		break;
	case VIX_OP_SHIFT_LEFT:
	case VIX_OP_SHIFT_RIGHT:
		vix_motion_type(vix, VIX_MOTIONTYPE_LINEWISE);
		break;
	case VIX_OP_REPLACE:
	{
		Macro *macro = macro_get(vix, VIX_REG_DOT);
		macro->len   = 0;
		macro_append(macro, va_arg(ap, char*));
		vix->action.arg.s = macro->data;
		break;
	}
	case VIX_OP_DELETE:
	{
		enum VixMode mode = vix->mode->id;
		enum VixRegister reg = vix_register_used(vix);
		if (reg == VIX_REG_DEFAULT && (mode == VIX_MODE_INSERT || mode == VIX_MODE_REPLACE))
			vix_register(vix, VIX_REG_BLACKHOLE);
		break;
	}
	default:
		break;
	}

	const Operator *op = 0;
	if (id < LENGTH(vix_operators))
		op = vix_operators + id;
	else if ((VixDACount)id - VIX_OP_LAST < vix->operators.count)
		op = vix->operators.data + id - VIX_OP_LAST;

	if (!op) {
		result = false;
		goto out;
	}

	if (vix->mode->visual) {
		vix->action.op = op;
		vix_do(vix);
		goto out;
	}

	/* switch to operator mode inorder to make operator options and
	 * text-object available */
	vix_mode_switch(vix, VIX_MODE_OPERATOR_PENDING);
	if (vix->action.op == op) {
		/* hacky way to handle double operators i.e. things like
		 * dd, yy etc where the second char isn't a movement */
		vix_motion_type(vix, VIX_MOTIONTYPE_LINEWISE);
		vix_motion(vix, VIX_MOVE_LINE_NEXT);
	} else {
		vix->action.op = op;
	}

	/* put is not a real operator, does not need a range to operate on */
	if (id == VIX_OP_PUT_AFTER)
		vix_motion(vix, VIX_MOVE_NOP);

out:
	va_end(ap);
	return result;
}

const Operator vix_operators[] = {
	[VIX_OP_DELETE]      = { op_delete      },
	[VIX_OP_CHANGE]      = { op_change      },
	[VIX_OP_YANK]        = { op_yank        },
	[VIX_OP_PUT_AFTER]   = { op_put         },
	[VIX_OP_SHIFT_RIGHT] = { op_shift_right },
	[VIX_OP_SHIFT_LEFT]  = { op_shift_left  },
	[VIX_OP_JOIN]        = { op_join        },
	[VIX_OP_MODESWITCH]  = { op_modeswitch  },
	[VIX_OP_REPLACE]     = { op_replace     },
	[VIX_OP_CURSOR_SOL]  = { op_cursor      },
};
