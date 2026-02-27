#include <string.h>
#include <stdlib.h>
#include "text-regex.h"
#include "text-motions.h"

struct Regex {
	regex_t regex;
	char *pattern;
	bool is_literal;
};

size_t text_regex_nsub(Regex *r) {
	if (!r) {
		return 0;
	}
	return r->regex.re_nsub;
}

Regex *text_regex_new(void) {
	Regex *r = calloc(1, sizeof(*r));
	if (!r) {
		return NULL;
	}
	tre_regcomp(&r->regex, "\0\0", 0);
	return r;
}

void text_regex_free(Regex *r) {
	if (!r) {
		return;
	}
	tre_regfree(&r->regex);
	free(r->pattern);
	free(r);
}

static bool check_literal(const char *s) {
	if (!s) return false;
	const char *specials = "^$()[]{}*+?.|\\";
	while (*s) {
		if (strchr(specials, *s)) return false;
		s++;
	}
	return true;
}

int text_regex_compile(Regex *regex, const char *string, int cflags) {
	tre_regfree(&regex->regex);
	free(regex->pattern);
	regex->pattern = strdup(string);
	regex->is_literal = check_literal(string) && !(cflags & REG_ICASE);
	int r = tre_regcomp(&regex->regex, string, cflags);
	if (r) {
		tre_regfree(&regex->regex);
		tre_regcomp(&regex->regex, "\0\0", 0);
	}
	return r;
}

int text_regex_match(Regex *r, const char *data, int eflags) {
	return tre_regexec(&r->regex, data, 0, NULL, eflags);
}

int text_search_range_forward(Text *txt, size_t pos, size_t len, Regex *r, size_t nmatch, RegexMatch pmatch[], int eflags) {
	int ret = REG_NOMATCH;
	size_t range_start = pos;
	size_t range_end = pos + len;
	size_t chunk_size = 256 * 1024;
	size_t overlap = 4096;
	char *buf = malloc(chunk_size);
	if (!buf) {
		return REG_ESPACE;
	}

	size_t patlen = r->is_literal ? strlen(r->pattern) : 0;

	size_t cur_start = range_start;
	while (cur_start < range_end) {
		size_t cur_end = (cur_start + chunk_size < range_end) ? cur_start + chunk_size : range_end;
		size_t cur_len = cur_end - cur_start;

		text_bytes_get(txt, cur_start, cur_len, buf);

		if (r->is_literal) {
			char *match = memmem(buf, cur_len, r->pattern, patlen);
			if (match) {
				if (nmatch > 0) {
					pmatch[0].start = cur_start + (match - buf);
					pmatch[0].end = pmatch[0].start + patlen;
				}
				for (size_t i = 1; i < nmatch; i++) {
					pmatch[i].start = pmatch[i].end = EPOS;
				}
				free(buf);
				return 0;
			}
		} else {
			int cur_eflags = eflags;
			if (cur_start > range_start) cur_eflags |= REG_NOTBOL;
			if (cur_end < range_end) cur_eflags |= REG_NOTEOL;

			regmatch_t match[MAX_REGEX_SUB];
			ret = tre_regnexec(&r->regex, buf, cur_len, nmatch, match, cur_eflags);
			if (!ret) {
				for (size_t i = 0; i < nmatch; i++) {
					pmatch[i].start = match[i].rm_so == -1 ? EPOS : cur_start + match[i].rm_so;
					pmatch[i].end = match[i].rm_eo == -1 ? EPOS : cur_start + match[i].rm_eo;
				}
				free(buf);
				return 0;
			}
		}

		if (cur_end == range_end) break;
		cur_start = cur_end - overlap;
		if (cur_start < range_start) cur_start = range_start;
	}

	free(buf);
	return ret;
}

int text_search_range_backward(Text *txt, size_t pos, size_t len, Regex *r, size_t nmatch, RegexMatch pmatch[], int eflags) {
	int ret = REG_NOMATCH;
	size_t range_start = pos;
	size_t range_end = pos + len;
	size_t chunk_size = 256 * 1024;
	size_t overlap = 4096;
	char *buf = malloc(chunk_size);
	if (!buf) {
		return REG_ESPACE;
	}

	size_t patlen = r->is_literal ? strlen(r->pattern) : 0;

	size_t cur_end = range_end;
	while (cur_end > range_start) {
		size_t cur_start = (cur_end > range_start + chunk_size) ? cur_end - chunk_size : range_start;
		size_t cur_len = cur_end - cur_start;
		
		text_bytes_get(txt, cur_start, cur_len, buf);

		if (r->is_literal) {
			char *last_match = NULL;
			char *curr = buf;
			while (curr <= buf + cur_len - patlen) {
				char *match = memmem(curr, (buf + cur_len) - curr, r->pattern, patlen);
				if (!match) break;
				last_match = match;
				curr = match + 1;
			}
			if (last_match) {
				if (nmatch > 0) {
					pmatch[0].start = cur_start + (last_match - buf);
					pmatch[0].end = pmatch[0].start + patlen;
				}
				for (size_t i = 1; i < nmatch; i++) {
					pmatch[i].start = pmatch[i].end = EPOS;
				}
				free(buf);
				return 0;
			}
		} else {
			bool found_in_chunk = false;
			RegexMatch last_match[MAX_REGEX_SUB];
			size_t search_off = 0;
			int cur_eflags = eflags;
			if (cur_start > range_start) cur_eflags |= REG_NOTBOL;
			if (cur_end < range_end) cur_eflags |= REG_NOTEOL;

			regmatch_t match[MAX_REGEX_SUB];
			while (search_off < cur_len && !tre_regnexec(&r->regex, buf + search_off, cur_len - search_off, nmatch, match, cur_eflags)) {
				found_in_chunk = true;
				ret = 0;
				for (size_t i = 0; i < nmatch; i++) {
					last_match[i].start = match[i].rm_so == -1 ? EPOS : cur_start + search_off + match[i].rm_so;
					last_match[i].end = match[i].rm_eo == -1 ? EPOS : cur_start + search_off + match[i].rm_eo;
				}
				size_t next_off = search_off + match[0].rm_eo;
				if (next_off <= search_off) next_off = search_off + 1;
				search_off = next_off;
				if (search_off > 0 && buf[search_off-1] == '\n') cur_eflags &= ~REG_NOTBOL;
				else cur_eflags |= REG_NOTBOL;
			}

			if (found_in_chunk) {
				memcpy(pmatch, last_match, nmatch * sizeof(RegexMatch));
				free(buf);
				return 0;
			}
		}

		if (cur_start == range_start) break;
		cur_end = cur_start + overlap;
	}

	free(buf);
	return ret;
}
