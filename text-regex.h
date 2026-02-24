#ifndef TEXT_REGEX_H
#define TEXT_REGEX_H

/* make the REG_* constants available */
#if CONFIG_TRE
#include <tre/tre.h>
#else
#include <regex.h>
#endif
#include "text.h"

#define MAX_REGEX_SUB 10

typedef struct Regex Regex;
typedef Filerange RegexMatch;

VIX_INTERNAL Regex *text_regex_new(void);
VIX_INTERNAL int text_regex_compile(Regex*, const char *pattern, int cflags);
VIX_INTERNAL size_t text_regex_nsub(Regex*);
VIX_INTERNAL void text_regex_free(Regex*);
VIX_INTERNAL int text_regex_match(Regex*, const char *data, int eflags);
VIX_INTERNAL int text_search_range_forward(Text*, size_t pos, size_t len, Regex *r, size_t nmatch, RegexMatch pmatch[], int eflags);
VIX_INTERNAL int text_search_range_backward(Text*, size_t pos, size_t len, Regex *r, size_t nmatch, RegexMatch pmatch[], int eflags);

#endif
