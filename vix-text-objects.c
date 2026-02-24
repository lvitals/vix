#include "vix-core.h"
#include "text-objects.h"
#include "util.h"

int vix_textobject_register(Vix *vix, int type, void *data, VixTextObjectFunction *textobject)
{
	*da_push(vix, &vix->textobjects) = (TextObject){
		.user = textobject,
		.type = type,
		.data = data,
	};
	return LENGTH(vix_textobjects) + vix->textobjects.count - 1;
}

bool vix_textobject(Vix *vix, enum VixTextObject id) {

	vix->action.textobj = 0;
	if (id < LENGTH(vix_textobjects))
		vix->action.textobj = vix_textobjects + id;
	else if ((VixDACount)id - LENGTH(vix_textobjects) < vix->textobjects.count)
		vix->action.textobj = vix->textobjects.data + id - LENGTH(vix_textobjects);

	if (!vix->action.textobj)
		return false;
	vix_do(vix);
	return true;
}

static Filerange vix_text_object_search_forward(Vix *vix, Text *txt, size_t pos) {
	Filerange range = text_range_empty();
	Regex *regex = vix_regex(vix, NULL);
	if (regex)
		range = text_object_search_forward(txt, pos, regex);
	text_regex_free(regex);
	return range;
}

static Filerange vix_text_object_search_backward(Vix *vix, Text *txt, size_t pos) {
	Filerange range = text_range_empty();
	Regex *regex = vix_regex(vix, NULL);
	if (regex)
		range = text_object_search_backward(txt, pos, regex);
	text_regex_free(regex);
	return range;
}

static Filerange object_unpaired(Text *txt, size_t pos, char obj) {
	char c;
	bool before = false;
	Iterator it = text_iterator_get(txt, pos), rit = it;

	while (text_iterator_byte_get(&rit, &c) && c != '\n') {
		if (c == obj) {
			before = true;
			break;
		}
		text_iterator_byte_prev(&rit, NULL);
	}

	/* if there is no previous occurrence on the same line, advance starting position */
	if (!before) {
		while (text_iterator_byte_get(&it, &c) && c != '\n') {
			if (c == obj) {
				pos = it.pos;
				break;
			}
			text_iterator_byte_next(&it, NULL);
		}
	}

	switch (obj) {
	case '"':
		return text_object_quote(txt, pos);
	case '\'':
		return text_object_single_quote(txt, pos);
	case '`':
		return text_object_backtick(txt, pos);
	default:
		return text_range_empty();
	}
}

static Filerange object_quote(Text *txt, size_t pos) {
	return object_unpaired(txt, pos, '"');
}

static Filerange object_single_quote(Text *txt, size_t pos) {
	return object_unpaired(txt, pos, '\'');
}

static Filerange object_backtick(Text *txt, size_t pos) {
	return object_unpaired(txt, pos, '`');
}

const TextObject vix_textobjects[] = {
	[VIX_TEXTOBJECT_INNER_WORD] = {
		.txt = text_object_word,
	},
	[VIX_TEXTOBJECT_OUTER_WORD] = {
		.txt = text_object_word_outer,
	},
	[VIX_TEXTOBJECT_INNER_LONGWORD] = {
		.txt = text_object_longword,
	},
	[VIX_TEXTOBJECT_OUTER_LONGWORD] = {
		.txt = text_object_longword_outer,
	},
	[VIX_TEXTOBJECT_SENTENCE] = {
		.txt = text_object_sentence,
	},
	[VIX_TEXTOBJECT_PARAGRAPH] = {
		.txt = text_object_paragraph,
	},
	[VIX_TEXTOBJECT_PARAGRAPH_OUTER] = {
		.txt = text_object_paragraph_outer,
	},
	[VIX_TEXTOBJECT_OUTER_SQUARE_BRACKET] = {
		.txt = text_object_square_bracket,
		.type = TEXTOBJECT_DELIMITED_OUTER,
	},
	[VIX_TEXTOBJECT_INNER_SQUARE_BRACKET] = {
		.txt = text_object_square_bracket,
		.type = TEXTOBJECT_DELIMITED_INNER,
	},
	[VIX_TEXTOBJECT_OUTER_CURLY_BRACKET] = {
		.txt = text_object_curly_bracket,
		.type = TEXTOBJECT_DELIMITED_OUTER,
	},
	[VIX_TEXTOBJECT_INNER_CURLY_BRACKET] = {
		.txt = text_object_curly_bracket,
		.type = TEXTOBJECT_DELIMITED_INNER,
	},
	[VIX_TEXTOBJECT_OUTER_ANGLE_BRACKET] = {
		.txt = text_object_angle_bracket,
		.type = TEXTOBJECT_DELIMITED_OUTER,
	},
	[VIX_TEXTOBJECT_INNER_ANGLE_BRACKET] = {
		.txt = text_object_angle_bracket,
		.type = TEXTOBJECT_DELIMITED_INNER,
	},
	[VIX_TEXTOBJECT_OUTER_PARENTHESIS] = {
		.txt = text_object_parenthesis,
		.type = TEXTOBJECT_DELIMITED_OUTER,
	},
	[VIX_TEXTOBJECT_INNER_PARENTHESIS] = {
		.txt = text_object_parenthesis,
		.type = TEXTOBJECT_DELIMITED_INNER,
	},
	[VIX_TEXTOBJECT_OUTER_QUOTE] = {
		.txt = object_quote,
		.type = TEXTOBJECT_DELIMITED_OUTER,
	},
	[VIX_TEXTOBJECT_INNER_QUOTE] = {
		.txt = object_quote,
		.type = TEXTOBJECT_DELIMITED_INNER,
	},
	[VIX_TEXTOBJECT_OUTER_SINGLE_QUOTE] = {
		.txt = object_single_quote,
		.type = TEXTOBJECT_DELIMITED_OUTER,
	},
	[VIX_TEXTOBJECT_INNER_SINGLE_QUOTE] = {
		.txt = object_single_quote,
		.type = TEXTOBJECT_DELIMITED_INNER,
	},
	[VIX_TEXTOBJECT_OUTER_BACKTICK] = {
		.txt = object_backtick,
		.type = TEXTOBJECT_DELIMITED_OUTER,
	},
	[VIX_TEXTOBJECT_INNER_BACKTICK] = {
		.txt = object_backtick,
		.type = TEXTOBJECT_DELIMITED_INNER,
	},
	[VIX_TEXTOBJECT_OUTER_LINE] = {
		.txt = text_object_line,
	},
	[VIX_TEXTOBJECT_INNER_LINE] = {
		.txt = text_object_line_inner,
	},
	[VIX_TEXTOBJECT_INDENTATION] = {
		.txt = text_object_indentation,
	},
	[VIX_TEXTOBJECT_SEARCH_FORWARD] = {
		.vix = vix_text_object_search_forward,
		.type = TEXTOBJECT_NON_CONTIGUOUS|TEXTOBJECT_EXTEND_FORWARD,
	},
	[VIX_TEXTOBJECT_SEARCH_BACKWARD] = {
		.vix = vix_text_object_search_backward,
		.type = TEXTOBJECT_NON_CONTIGUOUS|TEXTOBJECT_EXTEND_BACKWARD,
	},
};

