#include "termkey.h"

#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>

struct TermKey {
	int fd;
	int flags;
	int canonflags;
	int waittime;
	bool abstract;
	bool have_termios;
	bool started;
	struct termios termios_orig;
	unsigned char buf[128];
	size_t len;
	size_t pos;
	unsigned char pending[64];
	size_t pending_len;
};

typedef struct {
	int sym;
	const char *name;
} SymName;

static const SymName sym_names[] = {
	{ TERMKEY_SYM_BACKSPACE, "Backspace" },
	{ TERMKEY_SYM_TAB, "Tab" },
	{ TERMKEY_SYM_ENTER, "Enter" },
	{ TERMKEY_SYM_ESCAPE, "Escape" },
	{ TERMKEY_SYM_SPACE, "Space" },
	{ TERMKEY_SYM_DEL, "Del" },
	{ TERMKEY_SYM_UP, "Up" },
	{ TERMKEY_SYM_DOWN, "Down" },
	{ TERMKEY_SYM_LEFT, "Left" },
	{ TERMKEY_SYM_RIGHT, "Right" },
	{ TERMKEY_SYM_BEGIN, "Begin" },
	{ TERMKEY_SYM_FIND, "Find" },
	{ TERMKEY_SYM_INSERT, "Insert" },
	{ TERMKEY_SYM_DELETE, "Delete" },
	{ TERMKEY_SYM_SELECT, "Select" },
	{ TERMKEY_SYM_PAGEUP, "PageUp" },
	{ TERMKEY_SYM_PAGEDOWN, "PageDown" },
	{ TERMKEY_SYM_HOME, "Home" },
	{ TERMKEY_SYM_END, "End" },
	{ TERMKEY_SYM_CANCEL, "Cancel" },
	{ TERMKEY_SYM_CLEAR, "Clear" },
	{ TERMKEY_SYM_CLOSE, "Close" },
	{ TERMKEY_SYM_COMMAND, "Command" },
	{ TERMKEY_SYM_COPY, "Copy" },
	{ TERMKEY_SYM_EXIT, "Exit" },
	{ TERMKEY_SYM_HELP, "Help" },
	{ TERMKEY_SYM_MARK, "Mark" },
	{ TERMKEY_SYM_MESSAGE, "Message" },
	{ TERMKEY_SYM_MOVE, "Move" },
	{ TERMKEY_SYM_OPEN, "Open" },
	{ TERMKEY_SYM_OPTIONS, "Options" },
	{ TERMKEY_SYM_PRINT, "Print" },
	{ TERMKEY_SYM_REDO, "Redo" },
	{ TERMKEY_SYM_REFERENCE, "Reference" },
	{ TERMKEY_SYM_REFRESH, "Refresh" },
	{ TERMKEY_SYM_REPLACE, "Replace" },
	{ TERMKEY_SYM_RESTART, "Restart" },
	{ TERMKEY_SYM_RESUME, "Resume" },
	{ TERMKEY_SYM_SAVE, "Save" },
	{ TERMKEY_SYM_SUSPEND, "Suspend" },
	{ TERMKEY_SYM_UNDO, "Undo" },
	{ TERMKEY_SYM_KP0, "KP0" },
	{ TERMKEY_SYM_KP1, "KP1" },
	{ TERMKEY_SYM_KP2, "KP2" },
	{ TERMKEY_SYM_KP3, "KP3" },
	{ TERMKEY_SYM_KP4, "KP4" },
	{ TERMKEY_SYM_KP5, "KP5" },
	{ TERMKEY_SYM_KP6, "KP6" },
	{ TERMKEY_SYM_KP7, "KP7" },
	{ TERMKEY_SYM_KP8, "KP8" },
	{ TERMKEY_SYM_KP9, "KP9" },
	{ TERMKEY_SYM_KPENTER, "KPEnter" },
	{ TERMKEY_SYM_KPPLUS, "KPPlus" },
	{ TERMKEY_SYM_KPMINUS, "KPMinus" },
	{ TERMKEY_SYM_KPMULT, "KPMult" },
	{ TERMKEY_SYM_KPDIV, "KPDiv" },
	{ TERMKEY_SYM_KPCOMMA, "KPComma" },
	{ TERMKEY_SYM_KPPERIOD, "KPPeriod" },
	{ TERMKEY_SYM_KPEQUALS, "KPEquals" },
	{ 0, NULL }
};

static void key_clear(TermKeyKey *key)
{
	memset(key, 0, sizeof(*key));
}

static void key_sym(TermKeyKey *key, int sym, int modifiers)
{
	key_clear(key);
	key->type = TERMKEY_TYPE_KEYSYM;
	key->code.sym = sym;
	key->modifiers = modifiers;
}

static void key_unicode(TermKeyKey *key, long codepoint, const char *utf8, size_t len, int modifiers)
{
	key_clear(key);
	key->type = TERMKEY_TYPE_UNICODE;
	key->code.codepoint = codepoint;
	key->modifiers = modifiers;
	if (len >= sizeof(key->utf8)) {
		len = sizeof(key->utf8) - 1;
	}
	memcpy(key->utf8, utf8, len);
	key->utf8[len] = '\0';
}

static int utf8_len(unsigned char c)
{
	if ((c & 0x80) == 0) return 1;
	if ((c & 0xe0) == 0xc0) return 2;
	if ((c & 0xf0) == 0xe0) return 3;
	if ((c & 0xf8) == 0xf0) return 4;
	return 1;
}

static long utf8_decode(const unsigned char *s, size_t len)
{
	if (len == 1) return s[0];
	if (len == 2) return ((long)(s[0] & 0x1f) << 6) | (s[1] & 0x3f);
	if (len == 3) return ((long)(s[0] & 0x0f) << 12) | ((long)(s[1] & 0x3f) << 6) | (s[2] & 0x3f);
	if (len == 4) return ((long)(s[0] & 0x07) << 18) | ((long)(s[1] & 0x3f) << 12) | ((long)(s[2] & 0x3f) << 6) | (s[3] & 0x3f);
	return -1;
}

static int mod_from_csi(int value)
{
	switch (value) {
	case 2: return TERMKEY_KEYMOD_SHIFT;
	case 3: return TERMKEY_KEYMOD_ALT;
	case 4: return TERMKEY_KEYMOD_SHIFT | TERMKEY_KEYMOD_ALT;
	case 5: return TERMKEY_KEYMOD_CTRL;
	case 6: return TERMKEY_KEYMOD_SHIFT | TERMKEY_KEYMOD_CTRL;
	case 7: return TERMKEY_KEYMOD_ALT | TERMKEY_KEYMOD_CTRL;
	case 8: return TERMKEY_KEYMOD_SHIFT | TERMKEY_KEYMOD_ALT | TERMKEY_KEYMOD_CTRL;
	default: return 0;
	}
}

static int sym_from_csi_final(unsigned char final)
{
	switch (final) {
	case 'A': return TERMKEY_SYM_UP;
	case 'B': return TERMKEY_SYM_DOWN;
	case 'C': return TERMKEY_SYM_RIGHT;
	case 'D': return TERMKEY_SYM_LEFT;
	case 'F': return TERMKEY_SYM_END;
	case 'H': return TERMKEY_SYM_HOME;
	default: return TERMKEY_SYM_UNKNOWN;
	}
}

static int sym_from_tilde(int value)
{
	switch (value) {
	case 1: return TERMKEY_SYM_HOME;
	case 2: return TERMKEY_SYM_INSERT;
	case 3: return TERMKEY_SYM_DELETE;
	case 4: return TERMKEY_SYM_END;
	case 5: return TERMKEY_SYM_PAGEUP;
	case 6: return TERMKEY_SYM_PAGEDOWN;
	case 7: return TERMKEY_SYM_HOME;
	case 8: return TERMKEY_SYM_END;
	case 11: return 1;
	case 12: return 2;
	case 13: return 3;
	case 14: return 4;
	case 15: return 5;
	case 17: return 6;
	case 18: return 7;
	case 19: return 8;
	case 20: return 9;
	case 21: return 10;
	case 23: return 11;
	case 24: return 12;
	default: return TERMKEY_SYM_UNKNOWN;
	}
}

static int parse_number(const unsigned char **p, const unsigned char *end)
{
	int n = 0;
	while (*p < end && isdigit(**p)) {
		n = n * 10 + (**p - '0');
		(*p)++;
	}
	return n;
}

static TermKeyResult parse_escape_sequence(TermKeyKey *key, const unsigned char *seq, size_t len)
{
	if (len < 2 || seq[0] != 0x1b) {
		return TERMKEY_RES_NONE;
	}
	if (seq[1] == 'O' && len >= 3) {
		int sym = sym_from_csi_final(seq[2]);
		switch (seq[2]) {
		case 'P': key_clear(key); key->type = TERMKEY_TYPE_FUNCTION; key->code.number = 1; return TERMKEY_RES_KEY;
		case 'Q': key_clear(key); key->type = TERMKEY_TYPE_FUNCTION; key->code.number = 2; return TERMKEY_RES_KEY;
		case 'R': key_clear(key); key->type = TERMKEY_TYPE_FUNCTION; key->code.number = 3; return TERMKEY_RES_KEY;
		case 'S': key_clear(key); key->type = TERMKEY_TYPE_FUNCTION; key->code.number = 4; return TERMKEY_RES_KEY;
		default:
			if (sym != TERMKEY_SYM_UNKNOWN) {
				key_sym(key, sym, 0);
				return TERMKEY_RES_KEY;
			}
		}
	}
	if (seq[1] != '[' || len < 3) {
		return TERMKEY_RES_NONE;
	}

	unsigned char final = seq[len - 1];
	if (final < 0x40 || final > 0x7e) {
		return TERMKEY_RES_AGAIN;
	}

	const unsigned char *p = seq + 2;
	const unsigned char *end = seq + len - 1;
	int args[8] = { 0 };
	size_t nargs = 0;
	while (p < end && nargs < sizeof(args) / sizeof(args[0])) {
		args[nargs++] = parse_number(&p, end);
		if (p < end && (*p == ';' || *p == ':')) {
			p++;
		} else if (p < end) {
			p++;
		}
	}

	if (final == '~') {
		int value = nargs ? args[0] : 0;
		int sym = sym_from_tilde(value);
		int modifiers = nargs >= 2 ? mod_from_csi(args[1]) : 0;
		if (sym >= 1 && sym <= 12 && value >= 11) {
			key_clear(key);
			key->type = TERMKEY_TYPE_FUNCTION;
			key->code.number = sym;
			key->modifiers = modifiers;
		} else if (sym != TERMKEY_SYM_UNKNOWN) {
			key_sym(key, sym, modifiers);
		} else {
			key_clear(key);
			key->type = TERMKEY_TYPE_UNKNOWN_CSI;
			key->csi_len = len - 2 < sizeof(key->csi) - 1 ? len - 2 : sizeof(key->csi) - 1;
			memcpy(key->csi, seq + 2, key->csi_len);
			key->csi[key->csi_len] = '\0';
		}
		return TERMKEY_RES_KEY;
	}

	int sym = sym_from_csi_final(final);
	if (sym != TERMKEY_SYM_UNKNOWN) {
		int modifiers = nargs >= 2 ? mod_from_csi(args[1]) : 0;
		key_sym(key, sym, modifiers);
		return TERMKEY_RES_KEY;
	}

	key_clear(key);
	key->type = TERMKEY_TYPE_UNKNOWN_CSI;
	key->csi_len = len - 2 < sizeof(key->csi) - 1 ? len - 2 : sizeof(key->csi) - 1;
	memcpy(key->csi, seq + 2, key->csi_len);
	key->csi[key->csi_len] = '\0';
	return TERMKEY_RES_KEY;
}

static TermKeyResult parse_bytes(TermKeyKey *key, const unsigned char *buf, size_t len)
{
	if (!len) {
		return TERMKEY_RES_AGAIN;
	}
	unsigned char c = buf[0];
	if (c == 0x1b) {
		if (len == 1) {
			return TERMKEY_RES_AGAIN;
		}
		if (buf[1] == '[' || buf[1] == 'O') {
			return parse_escape_sequence(key, buf, len);
		}
		TermKeyResult ret = parse_bytes(key, buf + 1, len - 1);
		if (ret == TERMKEY_RES_KEY) {
			key->modifiers |= TERMKEY_KEYMOD_ALT;
		}
		return ret;
	}
	if (c == '\r' || c == '\n') {
		key_sym(key, TERMKEY_SYM_ENTER, 0);
		return TERMKEY_RES_KEY;
	}
	if (c == '\t') {
		key_sym(key, TERMKEY_SYM_TAB, 0);
		return TERMKEY_RES_KEY;
	}
	if (c == 0x7f) {
		key_sym(key, TERMKEY_SYM_BACKSPACE, 0);
		return TERMKEY_RES_KEY;
	}
	if (c < 0x20) {
		char ch = (char)(c + 0x40);
		if (ch >= 'A' && ch <= 'Z') {
			ch = (char)(ch + ('a' - 'A'));
		}
		key_unicode(key, ch, &ch, 1, TERMKEY_KEYMOD_CTRL);
		return TERMKEY_RES_KEY;
	}
	int n = utf8_len(c);
	if ((size_t)n > len) {
		return TERMKEY_RES_AGAIN;
	}
	key_unicode(key, utf8_decode(buf, (size_t)n), (const char *)buf, (size_t)n, 0);
	return TERMKEY_RES_KEY;
}

static int read_byte(TermKey *tk, unsigned char *byte, int timeout)
{
	if (tk->pos < tk->len) {
		*byte = tk->buf[tk->pos++];
		if (tk->pos == tk->len) {
			tk->pos = tk->len = 0;
		}
		return 1;
	}
	struct pollfd pfd = { .fd = tk->fd, .events = POLLIN };
	int pret = poll(&pfd, 1, timeout);
	if (pret == 0) {
		return 0;
	}
	if (pret < 0) {
		return -1;
	}
	ssize_t n = read(tk->fd, byte, 1);
	if (n == 0) {
		return -2;
	}
	if (n < 0) {
		return -1;
	}
	return 1;
}

static bool sequence_complete(const unsigned char *seq, size_t len)
{
	if (len == 0) {
		return false;
	}
	if (seq[0] == 0x1b) {
		if (len == 1) {
			return false;
		}
		if (seq[1] == 'O') {
			return len >= 3;
		}
		if (seq[1] == '[') {
			if (len < 3) {
				return false;
			}
			unsigned char final = seq[len - 1];
			return final >= 0x40 && final <= 0x7e;
		}
		return true;
	}
	return (size_t)utf8_len(seq[0]) <= len;
}

static void pending_store(TermKey *tk, const unsigned char *seq, size_t len)
{
	if (len > sizeof(tk->pending)) {
		len = sizeof(tk->pending);
	}
	memcpy(tk->pending, seq, len);
	tk->pending_len = len;
}

static TermKeyResult read_key(TermKey *tk, TermKeyKey *key, bool force)
{
	unsigned char seq[64];
	size_t len = 0;
	int r;

	if (tk->pending_len) {
		len = tk->pending_len;
		memcpy(seq, tk->pending, len);
		tk->pending_len = 0;
	} else {
		r = read_byte(tk, &seq[0], 0);
		if (r == 0) {
			return TERMKEY_RES_AGAIN;
		}
		if (r == -2) {
			return TERMKEY_RES_EOF;
		}
		if (r < 0) {
			return errno == EAGAIN || errno == EWOULDBLOCK ? TERMKEY_RES_AGAIN : TERMKEY_RES_ERROR;
		}
		len = 1;
	}

	while (!sequence_complete(seq, len) && len < sizeof(seq)) {
		r = read_byte(tk, &seq[len], 0);
		if (r == -2) {
			break;
		}
		if (r <= 0) {
			break;
		}
		len++;
	}

	if (!sequence_complete(seq, len)) {
		if (force && len == 1 && seq[0] == 0x1b) {
			key_sym(key, TERMKEY_SYM_ESCAPE, 0);
			return TERMKEY_RES_KEY;
		}
		pending_store(tk, seq, len);
		return TERMKEY_RES_AGAIN;
	}

	TermKeyResult ret = parse_bytes(key, seq, len);
	if (ret == TERMKEY_RES_AGAIN || ret == TERMKEY_RES_NONE) {
		pending_store(tk, seq, len);
	}
	return ret;
}

static int termkey_setup_termios(TermKey *tk)
{
	if (!tk || tk->abstract || (tk->flags & TERMKEY_FLAG_NOTERMIOS)) {
		return 0;
	}
	if (!tk->have_termios) {
		if (tcgetattr(tk->fd, &tk->termios_orig) == -1) {
			if (errno == ENOTTY) {
				return 0;
			}
			return -1;
		}
		tk->have_termios = true;
	}

	struct termios raw = tk->termios_orig;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= CS8;
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(tk->fd, TCSANOW, &raw) == -1) {
		return -1;
	}
	tk->started = true;
	return 0;
}

static void termkey_restore_termios(TermKey *tk)
{
	if (!tk || !tk->started || !tk->have_termios || (tk->flags & TERMKEY_FLAG_NOTERMIOS)) {
		return;
	}
	tcsetattr(tk->fd, TCSANOW, &tk->termios_orig);
	tk->started = false;
}

TermKey *termkey_new(int fd, int flags)
{
	TermKey *tk = calloc(1, sizeof(*tk));
	if (!tk) {
		return NULL;
	}
	tk->fd = fd;
	tk->flags = flags;
	tk->waittime = 50;
	if (termkey_setup_termios(tk) == -1) {
		free(tk);
		return NULL;
	}
	return tk;
}

TermKey *termkey_new_abstract(const char *term, int flags)
{
	(void)term;
	TermKey *tk = calloc(1, sizeof(*tk));
	if (!tk) {
		return NULL;
	}
	tk->fd = STDIN_FILENO;
	tk->flags = flags;
	tk->waittime = 50;
	tk->abstract = true;
	return tk;
}

void termkey_destroy(TermKey *tk) { termkey_restore_termios(tk); free(tk); }
void termkey_start(TermKey *tk) { if (termkey_setup_termios(tk) == -1) { /* ignore */ } }
void termkey_stop(TermKey *tk) { termkey_restore_termios(tk); }
void termkey_advisereadable(TermKey *tk) { (void)tk; }
void termkey_set_canonflags(TermKey *tk, int flags) { if (tk) tk->canonflags = flags; }
void termkey_set_waittime(TermKey *tk, int msec) { if (tk) tk->waittime = msec; }
int termkey_get_waittime(TermKey *tk) { return tk ? tk->waittime : 50; }
TermKeyResult termkey_getkey(TermKey *tk, TermKeyKey *key) { return read_key(tk, key, false); }
TermKeyResult termkey_getkey_force(TermKey *tk, TermKeyKey *key) { return read_key(tk, key, true); }

const char *termkey_get_keyname(TermKey *tk, int sym)
{
	(void)tk;
	for (const SymName *s = sym_names; s->name; s++) {
		if (s->sym == sym) {
			return s->name;
		}
	}
	return "Unknown";
}

static int sym_from_name(const char *name, size_t len)
{
	if (len == 3 && strncasecmp(name, "Esc", len) == 0) {
		return TERMKEY_SYM_ESCAPE;
	}
	if (len == 2 && strncasecmp(name, "BS", len) == 0) {
		return TERMKEY_SYM_BACKSPACE;
	}
	if (len == 4 && strncasecmp(name, "PgUp", len) == 0) {
		return TERMKEY_SYM_PAGEUP;
	}
	if (len == 4 && strncasecmp(name, "PgDn", len) == 0) {
		return TERMKEY_SYM_PAGEDOWN;
	}
	if (len == 2 && strncasecmp(name, "CR", len) == 0) {
		return TERMKEY_SYM_ENTER;
	}
	for (const SymName *s = sym_names; s->name; s++) {
		if (strlen(s->name) == len && strncasecmp(s->name, name, len) == 0) {
			return s->sym;
		}
	}
	return TERMKEY_SYM_UNKNOWN;
}

static const char *parse_modifiers(const char *p, int *modifiers)
{
	*modifiers = 0;
	bool again = true;
	while (again) {
		again = false;
		if ((p[0] == 'C' || p[0] == 'c') && p[1] == '-') {
			*modifiers |= TERMKEY_KEYMOD_CTRL;
			p += 2;
			again = true;
		} else if ((p[0] == 'M' || p[0] == 'm' || p[0] == 'A' || p[0] == 'a') && p[1] == '-') {
			*modifiers |= TERMKEY_KEYMOD_ALT;
			p += 2;
			again = true;
		} else if ((p[0] == 'S' || p[0] == 's') && p[1] == '-') {
			*modifiers |= TERMKEY_KEYMOD_SHIFT;
			p += 2;
			again = true;
		}
	}
	return p;
}

const char *termkey_strpkey(TermKey *tk, const char *str, TermKeyKey *key, int format)
{
	(void)tk;
	(void)format;
	if (!str || !*str) {
		return NULL;
	}

	int modifiers = 0;
	const char *p = parse_modifiers(str, &modifiers);
	const char *end = p;
	while (*end && *end != '>') {
		end++;
	}
	size_t len = (size_t)(end - p);
	if (len == 0) {
		return NULL;
	}

	if ((p[0] == 'F' || p[0] == 'f') && len > 1 && isdigit((unsigned char)p[1])) {
		int n = atoi(p + 1);
		if (n > 0) {
			key_clear(key);
			key->type = TERMKEY_TYPE_FUNCTION;
			key->code.number = n;
			key->modifiers = modifiers;
			return end;
		}
	}

	int sym = sym_from_name(p, len);
	if (sym != TERMKEY_SYM_UNKNOWN) {
		key_sym(key, sym, modifiers);
		return end;
	}

	int n = utf8_len((unsigned char)*p);
	if (n > 0 && (size_t)n == len) {
		char utf8[8];
		memcpy(utf8, p, len);
		key_unicode(key, utf8_decode((const unsigned char *)p, len), utf8, len, modifiers);
		return end;
	}

	return NULL;
}

size_t termkey_strfkey(TermKey *tk, char *buffer, size_t len, const TermKeyKey *key, int format)
{
	(void)tk;
	(void)format;
	char tmp[64];
	char mods[8] = "";
	if (key->modifiers & TERMKEY_KEYMOD_CTRL) strcat(mods, "C-");
	if (key->modifiers & TERMKEY_KEYMOD_ALT) strcat(mods, "M-");
	if (key->modifiers & TERMKEY_KEYMOD_SHIFT) strcat(mods, "S-");

	switch (key->type) {
	case TERMKEY_TYPE_UNICODE:
		if (key->modifiers) {
			snprintf(tmp, sizeof(tmp), "<%s%s>", mods, key->utf8);
		} else {
			snprintf(tmp, sizeof(tmp), "%s", key->utf8);
		}
		break;
	case TERMKEY_TYPE_FUNCTION:
		snprintf(tmp, sizeof(tmp), "<%sF%d>", mods, key->code.number);
		break;
	case TERMKEY_TYPE_KEYSYM:
		snprintf(tmp, sizeof(tmp), "<%s%s>", mods, termkey_get_keyname(NULL, key->code.sym));
		break;
	default:
		snprintf(tmp, sizeof(tmp), "<Unknown>");
		break;
	}

	size_t needed = strlen(tmp);
	if (len) {
		snprintf(buffer, len, "%s", tmp);
	}
	return needed;
}

TermKeyResult termkey_interpret_csi(TermKey *tk, const TermKeyKey *key, long args[], size_t *nargs, unsigned long *cmd)
{
	(void)tk;
	if (!key || key->type != TERMKEY_TYPE_UNKNOWN_CSI || key->csi_len == 0) {
		return TERMKEY_RES_NONE;
	}
	const char *p = key->csi;
	const char *end = key->csi + key->csi_len;
	*nargs = 0;
	while (p < end && *nargs < 16) {
		char *next = NULL;
		args[*nargs] = strtol(p, &next, 10);
		if (next == p) {
			break;
		}
		(*nargs)++;
		p = next;
		if (*p == ';' || *p == ':') {
			p++;
		}
	}
	*cmd = key->csi[key->csi_len - 1];
	return TERMKEY_RES_KEY;
}
