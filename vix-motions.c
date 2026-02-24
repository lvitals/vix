#include "vix-core.h"
#include "text-motions.h"
#include "text-objects.h"
#include "text-util.h"
#include "util.h"

static Regex *search_word(Vix *vix, Text *txt, size_t pos) {
	char expr[512];
	Filerange word = text_object_word(txt, pos);
	if (!text_range_valid(&word)) {
		return NULL;
	}
	char *buf = text_bytes_alloc0(txt, word.start, text_range_size(&word));
	if (!buf) {
		return NULL;
	}
	snprintf(expr, sizeof(expr), "[[:<:]]%s[[:>:]]", buf);
	Regex *regex = vix_regex(vix, expr);
	if (!regex) {
		snprintf(expr, sizeof(expr), "\\<%s\\>", buf);
		regex = vix_regex(vix, expr);
	}
	free(buf);
	return regex;
}

static size_t search_word_forward(Vix *vix, Text *txt, size_t pos) {
	Regex *regex = search_word(vix, txt, pos);
	if (regex) {
		vix->search_direction = VIX_MOVE_SEARCH_REPEAT_FORWARD;
		pos = text_search_forward(txt, pos, regex);
	}
	text_regex_free(regex);
	return pos;
}

static size_t search_word_backward(Vix *vix, Text *txt, size_t pos) {
	Regex *regex = search_word(vix, txt, pos);
	if (regex) {
		vix->search_direction = VIX_MOVE_SEARCH_REPEAT_BACKWARD;
		pos = text_search_backward(txt, pos, regex);
	}
	text_regex_free(regex);
	return pos;
}

static size_t search_common(Vix *vix, Text *txt, size_t pos, bool backward) {
	const char *pattern = register_get(vix, &vix->registers[VIX_REG_SEARCH], NULL);
	Regex *regex = vix_regex(vix, pattern);
	if (regex) {
		size_t newpos = backward ?
			text_search_backward(txt, pos, regex) :
			text_search_forward(txt, pos, regex);
		if (newpos == pos) {
			vix_info_show(vix, "Pattern not found: `%s'", pattern);
		}
		pos = newpos;
	}
	text_regex_free(regex);
	return pos;
}

static size_t search_forward(Vix *vix, Text *txt, size_t pos) {
	return search_common(vix, txt, pos, false);
}

static size_t search_backward(Vix *vix, Text *txt, size_t pos) {
	return search_common(vix, txt, pos, true);
}

static size_t common_word_next(Vix *vix, Text *txt, size_t pos,
                               enum VixMotion start_next, enum VixMotion end_next,
                               int (*isboundary)(int)) {
	char c;
	Iterator it = text_iterator_get(txt, pos);
	if (!text_iterator_byte_get(&it, &c)) {
		return pos;
	}
	const Movement *motion = NULL;
	int count = VIX_COUNT_DEFAULT(vix->action.count, 1);
	if (isspace((unsigned char)c)) {
		motion = &vix_motions[start_next];
	} else if (!isboundary((unsigned char)c) && text_iterator_char_next(&it, &c) && isboundary((unsigned char)c)) {
		/* we are on the last character of a word */
		if (count == 1) {
			/* map `cw` to `cl` */
			motion = &vix_motions[VIX_MOVE_CHAR_NEXT];
		} else {
			/* map `c{n}w` to `c{n-1}e` */
			count--;
			motion = &vix_motions[end_next];
		}
	} else {
		/* map `cw` to `ce` */
		motion = &vix_motions[end_next];
	}

	while (count--) {
		if (vix->interrupted) {
			return pos;
		}
		size_t newpos = motion->txt(txt, pos);
		if (newpos == pos) {
			break;
		}
		pos = newpos;
	}

	if (motion->type & INCLUSIVE) {
		pos = text_char_next(txt, pos);
	}

	return pos;
}

static size_t word_next(Vix *vix, Text *txt, size_t pos) {
	return common_word_next(vix, txt, pos, VIX_MOVE_WORD_START_NEXT,
	                        VIX_MOVE_WORD_END_NEXT, is_word_boundary);
}

static size_t longword_next(Vix *vix, Text *txt, size_t pos) {
	return common_word_next(vix, txt, pos, VIX_MOVE_LONGWORD_START_NEXT,
	                        VIX_MOVE_LONGWORD_END_NEXT, isspace);
}

static size_t to_right(Vix *vix, Text *txt, size_t pos) {
	char c;
	size_t hit = text_find_next(txt, pos+1, vix->search_char);
	if (!text_byte_get(txt, hit, &c) || c != vix->search_char[0]) {
		return pos;
	}
	return hit;
}

static size_t till_right(Vix *vix, Text *txt, size_t pos) {
	size_t hit = to_right(vix, txt, pos+1);
	if (hit != pos) {
		return text_char_prev(txt, hit);
	}
	return pos;
}

static size_t to_left(Vix *vix, Text *txt, size_t pos) {
	return text_find_prev(txt, pos, vix->search_char);
}

static size_t till_left(Vix *vix, Text *txt, size_t pos) {
	size_t hit = to_left(vix, txt, pos-1);
	if (hit != pos-1) {
		return text_char_next(txt, hit);
	}
	return pos;
}

static size_t to_line_right(Vix *vix, Text *txt, size_t pos) {
	char c;
	if (pos == text_line_end(txt, pos)) {
		return pos;
	}
	size_t hit = text_line_find_next(txt, pos+1, vix->search_char);
	if (!text_byte_get(txt, hit, &c) || c != vix->search_char[0]) {
		return pos;
	}
	return hit;
}

static size_t till_line_right(Vix *vix, Text *txt, size_t pos) {
	size_t hit = to_line_right(vix, txt, pos+1);
	if (pos == text_line_end(txt, pos)) {
		return pos;
	}
	if (hit != pos) {
		return text_char_prev(txt, hit);
	}
	return pos;
}

static size_t to_line_left(Vix *vix, Text *txt, size_t pos) {
	return text_line_find_prev(txt, pos, vix->search_char);
}

static size_t till_line_left(Vix *vix, Text *txt, size_t pos) {
	if (pos == text_line_begin(txt, pos)) {
		return pos;
	}
	size_t hit = to_line_left(vix, txt, pos-1);
	if (hit != pos-1) {
		return text_char_next(txt, hit);
	}
	return pos;
}

static size_t firstline(Text *txt, size_t pos) {
	return text_line_start(txt, 0);
}

static size_t line(Vix *vix, Text *txt, size_t pos) {
	int count = VIX_COUNT_DEFAULT(vix->action.count, 1);
	return text_line_start(txt, text_pos_by_lineno(txt, count));
}

static size_t lastline(Text *txt, size_t pos) {
	pos = text_size(txt);
	return text_line_start(txt, pos > 0 ? pos-1 : pos);
}

static size_t column(Vix *vix, Text *txt, size_t pos) {
	return text_line_offset(txt, pos, VIX_COUNT_DEFAULT(vix->action.count, 0));
}

static size_t view_lines_top(Vix *vix, View *view) {
	return view_screenline_goto(view, VIX_COUNT_DEFAULT(vix->action.count, 1));
}

static size_t view_lines_middle(Vix *vix, View *view) {
	int h = view->height;
	return view_screenline_goto(view, h/2);
}

static size_t view_lines_bottom(Vix *vix, View *view) {
	int h = view->height;
	return view_screenline_goto(view, h - VIX_COUNT_DEFAULT(vix->action.count, 0));
}

static size_t window_nop(Vix *vix, Win *win, size_t pos) {
	return pos;
}

static size_t bracket_match(Text *txt, size_t pos) {
	size_t hit = text_bracket_match_symbol(txt, pos, "(){}[]<>'\"`", NULL);
	if (hit != pos) {
		return hit;
	}
	char current;
	Iterator it = text_iterator_get(txt, pos);
	while (text_iterator_byte_get(&it, &current)) {
		switch (current) {
		case '(':
		case ')':
		case '{':
		case '}':
		case '[':
		case ']':
		case '<':
		case '>':
		case '"':
		case '\'':
		case '`':
			return it.pos;
		}
		text_iterator_byte_next(&it, NULL);
	}
	return pos;
}

static size_t percent(Vix *vix, Text *txt, size_t pos) {
	int ratio = VIX_COUNT_DEFAULT(vix->action.count, 0);
	if (ratio > 100) {
		ratio = 100;
	}
	return text_size(txt) * ratio / 100;
}

static size_t byte(Vix *vix, Text *txt, size_t pos) {
	pos = VIX_COUNT_DEFAULT(vix->action.count, 0);
	size_t max = text_size(txt);
	return pos <= max ? pos : max;
}

static size_t byte_left(Vix *vix, Text *txt, size_t pos) {
	size_t off = VIX_COUNT_DEFAULT(vix->action.count, 1);
	return off <= pos ? pos-off : 0;
}

static size_t byte_right(Vix *vix, Text *txt, size_t pos) {
	size_t off = VIX_COUNT_DEFAULT(vix->action.count, 1);
	size_t new = pos + off;
	size_t max = text_size(txt);
	return new <= max && new > pos ? new : max;
}

void vix_motion_type(Vix *vix, enum VixMotionType type) {
	vix->action.type = type;
}

int vix_motion_register(Vix *vix, void *data, VixMotionFunction *motion)
{
	*da_push(vix, &vix->motions) = (Movement){
		.user = motion,
		.data = data,
	};
	return VIX_MOVE_LAST + vix->motions.count - 1;
}

bool vix_motion(Vix *vix, enum VixMotion motion, ...) {
	va_list ap;
	va_start(ap, motion);

	switch (motion) {
	case VIX_MOVE_WORD_START_NEXT:
		if (vix->action.op == &vix_operators[VIX_OP_CHANGE]) {
			motion = VIX_MOVE_WORD_NEXT;
		}
		break;
	case VIX_MOVE_LONGWORD_START_NEXT:
		if (vix->action.op == &vix_operators[VIX_OP_CHANGE]) {
			motion = VIX_MOVE_LONGWORD_NEXT;
		}
		break;
	case VIX_MOVE_SEARCH_FORWARD:
	case VIX_MOVE_SEARCH_BACKWARD:
	{
		const char *pattern = va_arg(ap, char*);
		Regex *regex = vix_regex(vix, pattern);
		if (!regex) {
			vix_cancel(vix);
			goto err;
		}
		text_regex_free(regex);
		if (motion == VIX_MOVE_SEARCH_FORWARD) {
			motion = VIX_MOVE_SEARCH_REPEAT_FORWARD;
		} else {
			motion = VIX_MOVE_SEARCH_REPEAT_BACKWARD;
		}
		vix->search_direction = motion;
		break;
	}
	case VIX_MOVE_SEARCH_REPEAT:
	case VIX_MOVE_SEARCH_REPEAT_REVERSE:
	{
		if (!vix->search_direction) {
			vix->search_direction = VIX_MOVE_SEARCH_REPEAT_FORWARD;
		}
		if (motion == VIX_MOVE_SEARCH_REPEAT) {
			motion = vix->search_direction;
		} else {
			motion = vix->search_direction == VIX_MOVE_SEARCH_REPEAT_FORWARD ?
			                                  VIX_MOVE_SEARCH_REPEAT_BACKWARD :
			                                  VIX_MOVE_SEARCH_REPEAT_FORWARD;
		}
		break;
	}
	case VIX_MOVE_TO_RIGHT:
	case VIX_MOVE_TO_LEFT:
	case VIX_MOVE_TO_LINE_RIGHT:
	case VIX_MOVE_TO_LINE_LEFT:
	case VIX_MOVE_TILL_RIGHT:
	case VIX_MOVE_TILL_LEFT:
	case VIX_MOVE_TILL_LINE_RIGHT:
	case VIX_MOVE_TILL_LINE_LEFT:
	{
		const char *key = va_arg(ap, char*);
		if (!key) {
			goto err;
		}
		strncpy(vix->search_char, key, sizeof(vix->search_char));
		vix->search_char[sizeof(vix->search_char)-1] = '\0';
		vix->last_totill = motion;
		break;
	}
	case VIX_MOVE_TOTILL_REPEAT:
		if (!vix->last_totill) {
			goto err;
		}
		motion = vix->last_totill;
		break;
	case VIX_MOVE_TOTILL_REVERSE:
		switch (vix->last_totill) {
		case VIX_MOVE_TO_RIGHT:
			motion = VIX_MOVE_TO_LEFT;
			break;
		case VIX_MOVE_TO_LEFT:
			motion = VIX_MOVE_TO_RIGHT;
			break;
		case VIX_MOVE_TO_LINE_RIGHT:
			motion = VIX_MOVE_TO_LINE_LEFT;
			break;
		case VIX_MOVE_TO_LINE_LEFT:
			motion = VIX_MOVE_TO_LINE_RIGHT;
			break;
		case VIX_MOVE_TILL_RIGHT:
			motion = VIX_MOVE_TILL_LEFT;
			break;
		case VIX_MOVE_TILL_LEFT:
			motion = VIX_MOVE_TILL_RIGHT;
			break;
		case VIX_MOVE_TILL_LINE_RIGHT:
			motion = VIX_MOVE_TILL_LINE_LEFT;
			break;
		case VIX_MOVE_TILL_LINE_LEFT:
			motion = VIX_MOVE_TILL_LINE_RIGHT;
			break;
		default:
			goto err;
		}
		break;
	default:
		break;
	}

	vix->action.movement = 0;
	if (motion < LENGTH(vix_motions)) {
		vix->action.movement = vix_motions + motion;
	} else if ((VixDACount)motion - VIX_MOVE_LAST < vix->motions.count) {
		vix->action.movement = vix->motions.data + motion - VIX_MOVE_LAST;
	}

	if (!vix->action.movement) {
		goto err;
	}

	va_end(ap);
	vix_do(vix);
	return true;
err:
	va_end(ap);
	return false;
}

const Movement vix_motions[] = {
	[VIX_MOVE_LINE_UP] = {
		.cur = view_line_up,
		.type = LINEWISE|LINEWISE_INCLUSIVE,
	},
	[VIX_MOVE_LINE_DOWN] = {
		.cur = view_line_down,
		.type = LINEWISE|LINEWISE_INCLUSIVE,
	},
	[VIX_MOVE_SCREEN_LINE_UP] = {
		.cur = view_screenline_up,
	},
	[VIX_MOVE_SCREEN_LINE_DOWN] = {
		.cur = view_screenline_down,
	},
	[VIX_MOVE_SCREEN_LINE_BEGIN] = {
		.cur = view_screenline_begin,
		.type = CHARWISE,
	},
	[VIX_MOVE_SCREEN_LINE_MIDDLE] = {
		.cur = view_screenline_middle,
		.type = CHARWISE,
	},
	[VIX_MOVE_SCREEN_LINE_END] = {
		.cur = view_screenline_end,
		.type = CHARWISE|INCLUSIVE,
	},
	[VIX_MOVE_LINE_PREV] = {
		.txt = text_line_prev,
	},
	[VIX_MOVE_LINE_BEGIN] = {
		.txt = text_line_begin,
		.type = IDEMPOTENT,
	},
	[VIX_MOVE_LINE_START] = {
		.txt = text_line_start,
		.type = IDEMPOTENT,
	},
	[VIX_MOVE_LINE_FINISH] = {
		.txt = text_line_finish,
		.type = INCLUSIVE|IDEMPOTENT,
	},
	[VIX_MOVE_LINE_END] = {
		.txt = text_line_end,
		.type = IDEMPOTENT,
	},
	[VIX_MOVE_LINE_NEXT] = {
		.txt = text_line_next,
	},
	[VIX_MOVE_LINE] = {
		.vix = line,
		.type = LINEWISE|IDEMPOTENT|JUMP,
	},
	[VIX_MOVE_COLUMN] = {
		.vix = column,
		.type = CHARWISE|IDEMPOTENT,
	},
	[VIX_MOVE_CHAR_PREV] = {
		.txt = text_char_prev,
		.type = CHARWISE,
	},
	[VIX_MOVE_CHAR_NEXT] = {
		.txt = text_char_next,
		.type = CHARWISE,
	},
	[VIX_MOVE_LINE_CHAR_PREV] = {
		.txt = text_line_char_prev,
		.type = CHARWISE,
	},
	[VIX_MOVE_LINE_CHAR_NEXT] = {
		.txt = text_line_char_next,
		.type = CHARWISE,
	},
	[VIX_MOVE_CODEPOINT_PREV] = {
		.txt = text_codepoint_prev,
		.type = CHARWISE,
	},
	[VIX_MOVE_CODEPOINT_NEXT] = {
		.txt = text_codepoint_next,
		.type = CHARWISE,
	},
	[VIX_MOVE_WORD_NEXT] = {
		.vix = word_next,
		.type = CHARWISE|IDEMPOTENT,
	},
	[VIX_MOVE_WORD_START_PREV] = {
		.txt = text_word_start_prev,
		.type = CHARWISE,
	},
	[VIX_MOVE_WORD_START_NEXT] = {
		.txt = text_word_start_next,
		.type = CHARWISE,
	},
	[VIX_MOVE_WORD_END_PREV] = {
		.txt = text_word_end_prev,
		.type = CHARWISE|INCLUSIVE,
	},
	[VIX_MOVE_WORD_END_NEXT] = {
		.txt = text_word_end_next,
		.type = CHARWISE|INCLUSIVE,
	},
	[VIX_MOVE_LONGWORD_NEXT] = {
		.vix = longword_next,
		.type = CHARWISE|IDEMPOTENT,
	},
	[VIX_MOVE_LONGWORD_START_PREV] = {
		.txt = text_longword_start_prev,
		.type = CHARWISE,
	},
	[VIX_MOVE_LONGWORD_START_NEXT] = {
		.txt = text_longword_start_next,
		.type = CHARWISE,
	},
	[VIX_MOVE_LONGWORD_END_PREV] = {
		.txt = text_longword_end_prev,
		.type = CHARWISE|INCLUSIVE,
	},
	[VIX_MOVE_LONGWORD_END_NEXT] = {
		.txt = text_longword_end_next,
		.type = CHARWISE|INCLUSIVE,
	},
	[VIX_MOVE_SENTENCE_PREV] = {
		.txt = text_sentence_prev,
		.type = CHARWISE,
	},
	[VIX_MOVE_SENTENCE_NEXT] = {
		.txt = text_sentence_next,
		.type = CHARWISE,
	},
	[VIX_MOVE_PARAGRAPH_PREV] = {
		.txt = text_paragraph_prev,
		.type = LINEWISE|JUMP,
	},
	[VIX_MOVE_PARAGRAPH_NEXT] = {
		.txt = text_paragraph_next,
		.type = LINEWISE|JUMP,
	},
	[VIX_MOVE_BLOCK_START] = {
		.txt = text_block_start,
		.type = JUMP,
	},
	[VIX_MOVE_BLOCK_END] = {
		.txt = text_block_end,
		.type = JUMP,
	},
	[VIX_MOVE_PARENTHESIS_START] = {
		.txt = text_parenthesis_start,
		.type = JUMP,
	},
	[VIX_MOVE_PARENTHESIS_END] = {
		.txt = text_parenthesis_end,
		.type = JUMP,
	},
	[VIX_MOVE_BRACKET_MATCH] = {
		.txt = bracket_match,
		.type = INCLUSIVE|JUMP,
	},
	[VIX_MOVE_FILE_BEGIN] = {
		.txt = firstline,
		.type = LINEWISE|LINEWISE_INCLUSIVE|JUMP|IDEMPOTENT,
	},
	[VIX_MOVE_FILE_END] = {
		.txt = lastline,
		.type = LINEWISE|LINEWISE_INCLUSIVE|JUMP|IDEMPOTENT,
	},
	[VIX_MOVE_TO_LEFT] = {
		.vix = to_left,
		.type = COUNT_EXACT,
	},
	[VIX_MOVE_TO_RIGHT] = {
		.vix = to_right,
		.type = INCLUSIVE|COUNT_EXACT,
	},
	[VIX_MOVE_TO_LINE_LEFT] = {
		.vix = to_line_left,
		.type = COUNT_EXACT,
	},
	[VIX_MOVE_TO_LINE_RIGHT] = {
		.vix = to_line_right,
		.type = INCLUSIVE|COUNT_EXACT,
	},
	[VIX_MOVE_TILL_LEFT] = {
		.vix = till_left,
		.type = COUNT_EXACT,
	},
	[VIX_MOVE_TILL_RIGHT] = {
		.vix = till_right,
		.type = INCLUSIVE|COUNT_EXACT,
	},
	[VIX_MOVE_TILL_LINE_LEFT] = {
		.vix = till_line_left,
		.type = COUNT_EXACT,
	},
	[VIX_MOVE_TILL_LINE_RIGHT] = {
		.vix = till_line_right,
		.type = INCLUSIVE|COUNT_EXACT,
	},
	[VIX_MOVE_SEARCH_WORD_FORWARD] = {
		.vix = search_word_forward,
		.type = JUMP,
	},
	[VIX_MOVE_SEARCH_WORD_BACKWARD] = {
		.vix = search_word_backward,
		.type = JUMP,
	},
	[VIX_MOVE_SEARCH_REPEAT_FORWARD] = {
		.vix = search_forward,
		.type = JUMP,
	},
	[VIX_MOVE_SEARCH_REPEAT_BACKWARD] = {
		.vix = search_backward,
		.type = JUMP,
	},
	[VIX_MOVE_WINDOW_LINE_TOP] = {
		.view = view_lines_top,
		.type = LINEWISE|JUMP|IDEMPOTENT,
	},
	[VIX_MOVE_WINDOW_LINE_MIDDLE] = {
		.view = view_lines_middle,
		.type = LINEWISE|JUMP|IDEMPOTENT,
	},
	[VIX_MOVE_WINDOW_LINE_BOTTOM] = {
		.view = view_lines_bottom,
		.type = LINEWISE|JUMP|IDEMPOTENT,
	},
	[VIX_MOVE_NOP] = {
		.win = window_nop,
		.type = IDEMPOTENT,
	},
	[VIX_MOVE_PERCENT] = {
		.vix = percent,
		.type = IDEMPOTENT,
	},
	[VIX_MOVE_BYTE] = {
		.vix = byte,
		.type = IDEMPOTENT,
	},
	[VIX_MOVE_BYTE_LEFT] = {
		.vix = byte_left,
		.type = IDEMPOTENT,
	},
	[VIX_MOVE_BYTE_RIGHT] = {
		.vix = byte_right,
		.type = IDEMPOTENT,
	},
};
