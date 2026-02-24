#include "vix-core.h"

static DA_COMPARE_FN(ranges_comparator)
{
	const Filerange *r1 = va, *r2 = vb;
	if (!text_range_valid(r1)) {
		return text_range_valid(r2) ? 1 : 0;
	}
	if (!text_range_valid(r2)) {
		return -1;
	}
	return (r1->start < r2->start || (r1->start == r2->start && r1->end < r2->end)) ? -1 : 1;
}

void vix_mark_normalize(FilerangeList *ranges)
{
	for (VixDACount i = 0; i < ranges->count; i++) {
		if (text_range_size(ranges->data + i) == 0) {
			da_unordered_remove(ranges, i);
		}
	}

	if (ranges->count) {
		da_sort(ranges, ranges_comparator);
		Filerange *prev = 0;
		for (VixDACount i = 0; i < ranges->count; i++) {
			Filerange *r = ranges->data + i;
			if (prev && text_range_overlap(prev, r)) {
				*prev = text_range_union(prev, r);
				da_ordered_remove(ranges, i);
			} else {
				prev = r;
				i++;
			}
		}
	}
}

static bool vix_mark_equal(FilerangeList a, FilerangeList b)
{
	bool result = a.count == b.count;
	for (VixDACount i = 0; result && i < a.count; i++) {
		result = text_range_equal(a.data + i, b.data + i);
	}
	return result;
}

static SelectionRegionList *mark_from(Vix *vix, enum VixMark id)
{
	if (vix->win) {
		if (id == VIX_MARK_SELECTION) {
			return &vix->win->saved_selections;
		}
		File *file = vix->win->file;
		if (id < LENGTH(file->marks)) {
			return file->marks + id;
		}
	}
	return 0;
}

enum VixMark vix_mark_used(Vix *vix) {
	return vix->action.mark;
}

void vix_mark(Vix *vix, enum VixMark mark) {
	if (mark < LENGTH(vix->win->file->marks)) {
		vix->action.mark = mark;
	}
}

static FilerangeList mark_get(Vix *vix, Win *win, SelectionRegionList *mark)
{
	FilerangeList result = {0};
	if (mark) {
		da_reserve(vix, &result, mark->count);
		for (VixDACount i = 0; i < mark->count; i++) {
			Filerange r = view_regions_restore(&win->view, mark->data + i);
			if (text_range_valid(&r)) {
				*da_push(vix, &result) = r;
			}
		}
		vix_mark_normalize(&result);
	}
	return result;
}

FilerangeList vix_mark_get(Vix *vix, Win *win, enum VixMark id)
{
	return mark_get(vix, win, mark_from(vix, id));
}

static void mark_set(Vix *vix, Win *win, SelectionRegionList *mark, FilerangeList ranges)
{
	if (mark) {
		mark->count = 0;
		for (VixDACount i = 0; i < ranges.count; i++) {
			SelectionRegion ss;
			if (view_regions_save(&win->view, ranges.data + i, &ss)) {
				*da_push(vix, mark) = ss;
			}
		}
	}
}

void vix_mark_set(Vix *vix, Win *win, enum VixMark id, FilerangeList ranges)
{
	mark_set(vix, win, mark_from(vix, id), ranges);
}

void vix_jumplist(Vix *vix, int advance)
{
	Win  *win  = vix->win;
	View *view = &win->view;
	FilerangeList cur = view_selections_get_all(vix, view);

	size_t cursor = win->mark_set_lru_cursor;
	win->mark_set_lru_cursor += advance;
	if (advance < 0) {
		cursor = win->mark_set_lru_cursor;
	}
	cursor %= VIX_MARK_SET_LRU_COUNT;

	SelectionRegionList *next = win->mark_set_lru_regions + cursor;
	bool done = false;
	if (next->count) {
		FilerangeList sel = mark_get(vix, win, next);
		done = vix_mark_equal(sel, cur);
		if (advance && !done) {
			/* NOTE: set cached selection */
			vix_mode_switch(vix, win->mark_set_lru_modes[cursor]);
			view_selections_set_all(view, sel, view_selections_primary_get(view)->anchored);
		}
		da_release(&sel);
	}

	if (!advance && !done) {
		/* NOTE: save the current selection */
		mark_set(vix, win, next, cur);
		win->mark_set_lru_modes[cursor] = vix->mode->id;
		win->mark_set_lru_cursor++;
	}

	da_release(&cur);
}

enum VixMark vix_mark_from(Vix *vix, char mark) {
	if (mark >= 'a' && mark <= 'z') {
		return VIX_MARK_a + mark - 'a';
	}
	for (size_t i = 0; i < LENGTH(vix_marks); i++) {
		if (vix_marks[i].name == mark) {
			return i;
		}
	}
	return VIX_MARK_INVALID;
}

char vix_mark_to(Vix *vix, enum VixMark mark) {
	if (VIX_MARK_a <= mark && mark <= VIX_MARK_z) {
		return 'a' + mark - VIX_MARK_a;
	}

	if (mark < LENGTH(vix_marks)) {
		return vix_marks[mark].name;
	}

	return '\0';
}

const MarkDef vix_marks[] = {
	[VIX_MARK_DEFAULT]        = { '\'', VIX_HELP("Default mark")    },
	[VIX_MARK_SELECTION]      = { '^',  VIX_HELP("Last selections") },
};
