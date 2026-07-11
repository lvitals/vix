#ifndef VIX_TERMKEY_H
#define VIX_TERMKEY_H

#include <stddef.h>
#include <stdint.h>

#define TERMKEY_FLAG_UTF8      (1 << 0)
#define TERMKEY_FLAG_NOTERMIOS (1 << 1)

#define TERMKEY_CANON_DELBS    (1 << 0)

#define TERMKEY_FORMAT_VIM     (1 << 0)

#define TERMKEY_KEYMOD_SHIFT   (1 << 0)
#define TERMKEY_KEYMOD_ALT     (1 << 1)
#define TERMKEY_KEYMOD_CTRL    (1 << 2)

typedef struct TermKey TermKey;

typedef enum {
	TERMKEY_RES_NONE,
	TERMKEY_RES_KEY,
	TERMKEY_RES_EOF,
	TERMKEY_RES_AGAIN,
	TERMKEY_RES_ERROR
} TermKeyResult;

typedef enum {
	TERMKEY_TYPE_UNICODE,
	TERMKEY_TYPE_FUNCTION,
	TERMKEY_TYPE_KEYSYM,
	TERMKEY_TYPE_MOUSE,
	TERMKEY_TYPE_POSITION,
	TERMKEY_TYPE_MODEREPORT,
	TERMKEY_TYPE_UNKNOWN_CSI
} TermKeyType;

typedef enum {
	TERMKEY_SYM_UNKNOWN,
	TERMKEY_SYM_NONE,
	TERMKEY_SYM_BACKSPACE,
	TERMKEY_SYM_TAB,
	TERMKEY_SYM_ENTER,
	TERMKEY_SYM_ESCAPE,
	TERMKEY_SYM_SPACE,
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
	TERMKEY_SYM_KPEQUALS
} TermKeySym;

typedef struct {
	TermKeyType type;
	union {
		long codepoint;
		int sym;
		int number;
	} code;
	int modifiers;
	char utf8[8];
	char csi[64];
	size_t csi_len;
} TermKeyKey;

TermKey *termkey_new(int fd, int flags);
TermKey *termkey_new_abstract(const char *term, int flags);
void termkey_destroy(TermKey *tk);
void termkey_start(TermKey *tk);
void termkey_stop(TermKey *tk);
void termkey_advisereadable(TermKey *tk);
void termkey_set_canonflags(TermKey *tk, int flags);
void termkey_set_waittime(TermKey *tk, int msec);
int termkey_get_waittime(TermKey *tk);
TermKeyResult termkey_getkey(TermKey *tk, TermKeyKey *key);
TermKeyResult termkey_getkey_force(TermKey *tk, TermKeyKey *key);
const char *termkey_strpkey(TermKey *tk, const char *str, TermKeyKey *key, int format);
size_t termkey_strfkey(TermKey *tk, char *buffer, size_t len, const TermKeyKey *key, int format);
const char *termkey_get_keyname(TermKey *tk, int sym);
TermKeyResult termkey_interpret_csi(TermKey *tk, const TermKeyKey *key, long args[], size_t *nargs, unsigned long *cmd);

#endif
