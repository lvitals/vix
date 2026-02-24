#ifndef VIX_H
#define VIX_H

#ifndef VIX_EXPORT
  #define VIX_EXPORT
#endif

#include <signal.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct Vix Vix;
typedef struct File File;
typedef struct Win Win;

#include "ui.h"
#include "view.h"
#include "text-regex.h"
#include "buffer.h"

#ifndef CONFIG_HELP
#define CONFIG_HELP 1
#endif

#if CONFIG_HELP
#define VIX_HELP_DECL(x) x
#define VIX_HELP_USE(x) x
#define VIX_HELP(x) (x),
#else
#define VIX_HELP_DECL(x)
#define VIX_HELP_USE(x) NULL
#define VIX_HELP(x)
#endif

/* simplify utility renames by distribution packagers */
#ifndef VIX_OPEN
#define VIX_OPEN "vix-open"
#endif
#ifndef VIX_CLIPBOARD
#define VIX_CLIPBOARD "vix-clipboard"
#endif

/* maximum bytes needed for string representation of a (pseudo) key */
#define VIX_KEY_LENGTH_MAX 64

/** Union used to pass arguments to key action functions. */
typedef union {
	bool b;
	int i;
	const char *s;
	const void *v;
	void (*w)(View*);
	void (*f)(Vix*);
} Arg;

/**
 * Key action handling function.
 * @param vix The editor instance.
 * @param keys Input queue content *after* the binding which invoked this function.
 * @param arg Arguments for the key action.
 * @rst
 * .. note:: An empty string ``""`` indicates that no further input is available.
 * @endrst
 * @return Pointer to first non-consumed key.
 * @rst
 * .. warning:: Must be in range ``[keys, keys+strlen(keys)]`` or ``NULL`` to
 * indicate that not enough input was available. In the latter case
 * the function will be called again once more input has been received.
 * @endrst
 * @ingroup vix_action
 */
#define KEY_ACTION_FN(name) const char *name(Vix *vix, const char *keys, const Arg *arg)
typedef KEY_ACTION_FN(KeyActionFunction);

/** Key action definition. */
typedef struct {
	const char *name;                /**< Name of a pseudo key ``<name>`` which can be used in mappings. */
	VIX_HELP_DECL(const char *help;) /**< One line human readable description, displayed by ``:help``. */
	KeyActionFunction *func;         /**< Key action implementation function. */
	Arg arg;                         /**< Options passes as last argument to ``func``. */
} KeyAction;

/**
 * A key binding, refers to an action or an alias
 * @rst
 * .. note:: Either ``action`` or ``alias`` must be ``NULL``.
 * @endrst
 */
typedef struct {
	const char *key;         /**< Symbolic key to trigger this binding. */
	const KeyAction *action; /**< Action to invoke when triggering this binding. */
	const char *alias;       /**< Replaces ``key`` with ``alias`` at the front of the input queue. */
} KeyBinding;

/*
---
## Core Lifecycle Functions
*/

/**
 * @defgroup vix_lifecycle Vix Lifecycle
 * @{
 */
/**
 * Initializes a new editor instance.
 * @param vix The editor instance.
 */
VIX_EXPORT bool vix_init(Vix*);
/** Release all resources associated with this editor instance, terminates UI. */
VIX_EXPORT void vix_cleanup(Vix*);
/**
 * Enter main loop, start processing user input.
 * @param vix The editor instance.
 * @return The editor exit status code.
 */
VIX_EXPORT int vix_run(Vix*);
/**
 * Terminate editing session, the given ``status`` will be the return value of `vix_run`.
 * @param vix The editor instance.
 * @param status The exit status.
 */
VIX_EXPORT void vix_exit(Vix *vix, int status);
/**
 * Emergency exit, print given message, perform minimal UI cleanup and exit process.
 * @param vix The editor instance.
 * @param msg The message to print.
 * @rst
 * .. note:: This function does not return.
 * @endrst
 */
VIX_EXPORT void vix_die(Vix *vix, const char *msg, ...);

/**
 * Inform the editor core that a signal occurred.
 * @param vix The editor instance.
 * @param signum The signal number.
 * @param siginfo Pointer to `siginfo_t` structure.
 * @param context Pointer to context.
 * @return Whether the signal was handled.
 * @rst
 * .. note:: Being designed as a library the editor core does *not* register any
 * signal handlers on its own.
 * .. note:: The remaining arguments match the prototype of ``sa_sigaction`` as
 * specified in `sigaction(2)`.
 * @endrst
 */
VIX_EXPORT bool vix_signal_handler(Vix *vix, int signum, const siginfo_t *siginfo, const void *context);

/*
---
## Drawing and Redrawing
*/

/**
 * @defgroup vix_draw Vix Drawing
 * @{
 */
/**
 * Draw user interface.
 * @param vix The editor instance.
 */
VIX_EXPORT void vix_draw(Vix*);
/**
 * Completely redraw user interface.
 * @param vix The editor instance.
 */
VIX_EXPORT void vix_redraw(Vix*);
/** @} */

/*
---
## Window Management
*/

/**
 * @defgroup vix_windows Vix Windows
 * @{
 */
/**
 * Create a new window and load the given file.
 * @param vix The editor instance.
 * @param filename If ``NULL`` an unnamed, empty buffer is created.
 * @rst
 * .. note:: If the given file name is already opened in another window,
 * the underlying File object is shared.
 * @endrst
 */
VIX_EXPORT bool vix_window_new(Vix *vix, const char *filename);
/**
 * Create a new window associated with a file descriptor.
 * @param vix The editor instance.
 * @param fd The file descriptor to associate with the window.
 * @rst
 * .. note:: No data is read from `fd`, but write commands without an
 * explicit filename will instead write to the file descriptor.
 * @endrst
 */
VIX_EXPORT bool vix_window_new_fd(Vix *vix, int fd);
/**
 * Reload the file currently displayed in the window from disk.
 * @param win The window to reload.
 */
VIX_EXPORT bool vix_window_reload(Win*);
/**
 * Change the file currently displayed in the window.
 * @param win The window to change.
 * @param filename The new file to display.
 */
VIX_EXPORT bool vix_window_change_file(Win *win, const char *filename);
/**
 * Check whether closing the window would loose unsaved changes.
 * @param win The window to check.
 */
VIX_EXPORT bool vix_window_closable(Win*);
/**
 * Close window, redraw user interface.
 * @param win The window to close.
 */
VIX_EXPORT void vix_window_close(Win*);
/**
 * Split the window, shares the underlying file object.
 * @param original The window to split.
 */
VIX_EXPORT bool vix_window_split(Win*);
/**
 * Draw a specific window.
 * @param win The window to draw.
 */
VIX_EXPORT void vix_window_draw(Win*);
/**
 * Invalidate a window, forcing a redraw.
 * @param win The window to invalidate.
 */
VIX_EXPORT void vix_window_invalidate(Win*);
/**
 * Focus next window.
 * @param vix The editor instance.
 */
VIX_EXPORT void vix_window_next(Vix*);
/**
 * Focus previous window.
 * @param vix The editor instance.
 */
VIX_EXPORT void vix_window_prev(Vix*);
/**
 * Change currently focused window, receiving user input.
 * @param win The window to focus.
 */
VIX_EXPORT void vix_window_focus(Win*);
/**
 * Swap location of two windows.
 * @param win1 The first window.
 * @param win2 The second window.
 */
VIX_EXPORT void vix_window_swap(Win *win1, Win *win2);
/** @} */

/*
---
## Information and Messaging
*/

/**
 * @defgroup vix_info Vix Info
 * @{
 */
/**
 * Display a user prompt with a certain title.
 * @param vix The editor instance.
 * @param title The title of the prompt.
 * @rst
 * .. note:: The prompt is currently implemented as a single line height window.
 * @endrst
 */
VIX_EXPORT void vix_prompt_show(Vix *vix, const char *title);

/**
 * Display a single line message.
 * @param vix The editor instance.
 * @param msg The message to display.
 * @rst
 * .. note:: The message will automatically be hidden upon next input.
 * @endrst
 */
VIX_EXPORT void vix_info_show(Vix *vix, const char *msg, ...);

/**
 * Display arbitrary long message in a dedicated window.
 * @param vix The editor instance.
 * @param msg The message to display.
 */
VIX_EXPORT void vix_message_show(Vix *vix, const char *msg);
/** @} */

/*
---
## Text Changes
*/

/**
 * @defgroup vix_changes Vix Changes
 * @{
 */
/**
 * Insert data into the file.
 * @param vix The editor instance.
 * @param pos The position to insert at.
 * @param data The data to insert.
 * @param len The length of the data to insert.
 */
VIX_EXPORT void vix_insert(Vix *vix, size_t pos, const char *data, size_t len);
/**
 * Replace data in the file.
 * @param vix The editor instance.
 * @param pos The position to replace at.
 * @param data The new data.
 * @param len The length of the new data.
 */
VIX_EXPORT void vix_replace(Vix *vix, size_t pos, const char *data, size_t len);
/**
 * Perform insertion at all cursor positions.
 * @param vix The editor instance.
 * @param data The data to insert.
 * @param len The length of the data.
 */
VIX_EXPORT void vix_insert_key(Vix *vix, const char *data, size_t len);
/**
 * Perform character substitution at all cursor positions.
 * @param vix The editor instance.
 * @param data The character data to substitute.
 * @param len The length of the data.
 * @rst
 * .. note:: Does not replace new line characters.
 * @endrst
 */
VIX_EXPORT void vix_replace_key(Vix *vix, const char *data, size_t len);
/**
 * Insert a tab at all cursor positions.
 * @param vix The editor instance.
 * @rst
 * .. note:: Performs tab expansion according to current settings.
 * @endrst
 */
VIX_EXPORT void vix_insert_tab(Vix*);
/**
 * Inserts a new line character at every cursor position.
 * @param vix The editor instance.
 * @rst
 * .. note:: Performs auto indentation according to current settings.
 * @endrst
 */
VIX_EXPORT void vix_insert_nl(Vix*);

/** @} */

/*
---
## Editor Modes
*/

/**
 * @defgroup vix_modes Vix Modes
 * @{
 */

/** Mode specifiers. */
enum VixMode {
	VIX_MODE_NORMAL,
	VIX_MODE_OPERATOR_PENDING,
	VIX_MODE_VISUAL,
	VIX_MODE_VISUAL_LINE, /**< Sub mode of `VIX_MODE_VISUAL`. */
	VIX_MODE_INSERT,
	VIX_MODE_REPLACE, /**< Sub mode of `VIX_MODE_INSERT`. */
	VIX_MODE_INVALID,
};

/**
 * Switch mode.
 * @param vix The editor instance.
 * @param mode The mode to switch to.
 * @rst
 * .. note:: Will first trigger the leave event of the currently active
 * mode, followed by an enter event of the new mode.
 * No events are emitted, if the specified mode is already active.
 * @endrst
 */
VIX_EXPORT void vix_mode_switch(Vix *vix, enum VixMode mode);
/**
 * Translate human readable mode name to constant.
 * @param vix The editor instance.
 * @param name The name of the mode.
 */
VIX_EXPORT enum VixMode vix_mode_from(Vix *vix, const char *name);

/** @} */

/*
---
## Keybinding and Actions
*/

/**
 * @defgroup vix_keybind Vix Keybindings
 * @{
 */
/**
 * Create a new key binding.
 * @param vix The editor instance.
 */
VIX_EXPORT KeyBinding *vix_binding_new(Vix*);
/**
 * Free a key binding.
 * @param vix The editor instance.
 * @param binding The key binding to free.
 */
VIX_EXPORT void vix_binding_free(Vix *vix, KeyBinding *binding);

/**
 * Set up a key binding.
 * @param vix The editor instance.
 * @param mode The mode in which the binding applies.
 * @param force Whether an existing mapping should be discarded.
 * @param key The symbolic key to map.
 * @param binding The binding to map.
 * @rst
 * .. note:: ``binding->key`` is always ignored in favor of ``key``.
 * @endrst
 */
VIX_EXPORT bool vix_mode_map(Vix *vix, enum VixMode mode, bool force, const char *key, const KeyBinding *binding);
/**
 * Analogous to `vix_mode_map`, but window specific.
 * @param win The window for the mapping.
 * @param mode The mode in which the binding applies.
 * @param force Whether an existing mapping should be discarded.
 * @param key The symbolic key to map.
 * @param binding The binding to map.
 */
VIX_EXPORT bool vix_window_mode_map(Win *win, enum VixMode mode, bool force, const char *key, const KeyBinding *binding);
/**
 * Unmap a symbolic key in a given mode.
 * @param vix The editor instance.
 * @param mode The mode from which to unmap.
 * @param key The symbolic key to unmap.
 */
VIX_EXPORT bool vix_mode_unmap(Vix *vix, enum VixMode mode, const char *key);
/**
 * Analogous to `vix_mode_unmap`, but window specific.
 * @param win The window from which to unmap.
 * @param mode The mode from which to unmap.
 * @param key The symbolic key to unmap.
 */
VIX_EXPORT bool vix_window_mode_unmap(Win *win, enum VixMode mode, const char *key);
/** @} */

/*
---
## Key Actions
*/

/**
 * @defgroup vix_action Vix Actions
 * @{
 */
/**
 * Create new key action.
 * @param vix The editor instance.
 * @param name The name to be used as symbolic key when registering.
 * @param help Optional single line help text.
 * @param func The function implementing the key action logic.
 * @param arg Argument passed to function.
 */
VIX_EXPORT KeyAction *vix_action_new(Vix *vix, const char *name, const char *help, KeyActionFunction *func, Arg arg);
/**
 * Free a key action.
 * @param vix The editor instance.
 * @param action The key action to free.
 */
VIX_EXPORT void vix_action_free(Vix *vix, KeyAction *action);
/**
 * Register key action.
 * @param vix The editor instance.
 * @param keyaction The key action to register.
 * @rst
 * .. note:: Makes the key action available under the pseudo key name specified
 * in ``keyaction->name``.
 * @endrst
 */
VIX_EXPORT bool vix_action_register(Vix *vix, const KeyAction *keyaction);

/** @} */

/*
---
## Keymap
*/

/**
 * @defgroup vix_keymap Vix Keymap
 * @{
 */

/**
 * Add a key translation.
 * @param vix The editor instance.
 * @param key The key to translate.
 * @param mapping The string to map the key to.
 */
VIX_EXPORT bool vix_keymap_add(Vix *vix, const char *key, const char *mapping);
/**
 * Temporarily disable the keymap for the next key press.
 * @param vix The editor instance.
 */
VIX_EXPORT void vix_keymap_disable(Vix*);

/** @} */

/*
---
## Operators
*/

/**
 * @defgroup vix_operators Vix Operators
 * @{
 */

/** Operator specifiers. */
enum VixOperator {
	VIX_OP_DELETE,
	VIX_OP_CHANGE,
	VIX_OP_YANK,
	VIX_OP_PUT_AFTER,
	VIX_OP_SHIFT_RIGHT,
	VIX_OP_SHIFT_LEFT,
	VIX_OP_JOIN,
	VIX_OP_MODESWITCH,
	VIX_OP_REPLACE,
	VIX_OP_CURSOR_SOL,
	VIX_OP_INVALID, /* denotes the end of the "real" operators */
	/* pseudo operators: keep them at the end to save space in array definition */
	VIX_OP_CURSOR_EOL,
	VIX_OP_PUT_AFTER_END,
	VIX_OP_PUT_BEFORE,
	VIX_OP_PUT_BEFORE_END,
	VIX_OP_LAST, /* has to be last enum member */
};

typedef struct OperatorContext OperatorContext;

/**
 * An operator performs a certain function on a given text range.
 * @param vix The editor instance.
 * @param text The text buffer to operate on.
 * @param context Operator-specific context.
 * @rst
 * .. note:: The operator must return the new cursor position or ``EPOS`` if
 * the cursor should be disposed.
 * .. note:: The last used operator can be repeated using `vix_repeat`.
 * @endrst
 */
typedef size_t (VixOperatorFunction)(Vix *vix, Text *text, OperatorContext *context);

/**
 * Register an operator.
 * @param vix The editor instance.
 * @param func The function implementing the operator logic.
 * @param context User-supplied context for the operator.
 * @return Operator ID to be used with `vix_operator`.
 */
VIX_EXPORT int vix_operator_register(Vix *vix, VixOperatorFunction *func, void *context);

/**
 * Set operator to execute.
 * @param vix The editor instance.
 * @param op The operator to perform.
 * @param ... Additional arguments depending on the operator type.
 *
 * Has immediate effect if:
 * - A visual mode is active.
 * - The same operator was already set (range will be the current line).
 *
 * Otherwise the operator will be executed on the range determined by:
 * - A motion (see `vix_motion`).
 * - A text object (`vix_textobject`).
 *
 * The expected varying arguments are:
 *
 * - `VIX_OP_JOIN`         a char pointer referring to the text to insert between lines.
 * - `VIX_OP_MODESWITCH`   an ``enum VixMode`` indicating the mode to switch to.
 * - `VIX_OP_REPLACE`      a char pointer referring to the replacement character.
 */
VIX_EXPORT bool vix_operator(Vix *vix, enum VixOperator op, ...);

/**
 * Repeat last operator, possibly with a new count if one was provided in the meantime.
 * @param vix The editor instance.
 */
VIX_EXPORT void vix_repeat(Vix*);

/**
 * Cancel pending operator, reset count, motion, text object, register etc.
 * @param vix The editor instance.
 */
VIX_EXPORT void vix_cancel(Vix*);

/** @} */

/*
---
## Motions
*/

/**
 * @defgroup vix_motions Vix Motions
 * @{
 */

/** Motion specifiers. */
enum VixMotion {
	VIX_MOVE_LINE_DOWN,
	VIX_MOVE_LINE_UP,
	VIX_MOVE_SCREEN_LINE_UP,
	VIX_MOVE_SCREEN_LINE_DOWN,
	VIX_MOVE_SCREEN_LINE_BEGIN,
	VIX_MOVE_SCREEN_LINE_MIDDLE,
	VIX_MOVE_SCREEN_LINE_END,
	VIX_MOVE_LINE_PREV,
	VIX_MOVE_LINE_BEGIN,
	VIX_MOVE_LINE_START,
	VIX_MOVE_LINE_FINISH,
	VIX_MOVE_LINE_LASTCHAR,
	VIX_MOVE_LINE_END,
	VIX_MOVE_LINE_NEXT,
	VIX_MOVE_LINE,
	VIX_MOVE_COLUMN,
	VIX_MOVE_CHAR_PREV,
	VIX_MOVE_CHAR_NEXT,
	VIX_MOVE_LINE_CHAR_PREV,
	VIX_MOVE_LINE_CHAR_NEXT,
	VIX_MOVE_CODEPOINT_PREV,
	VIX_MOVE_CODEPOINT_NEXT,
	VIX_MOVE_WORD_NEXT,
	VIX_MOVE_WORD_START_NEXT,
	VIX_MOVE_WORD_END_PREV,
	VIX_MOVE_WORD_END_NEXT,
	VIX_MOVE_WORD_START_PREV,
	VIX_MOVE_LONGWORD_NEXT,
	VIX_MOVE_LONGWORD_START_PREV,
	VIX_MOVE_LONGWORD_START_NEXT,
	VIX_MOVE_LONGWORD_END_PREV,
	VIX_MOVE_LONGWORD_END_NEXT,
	VIX_MOVE_SENTENCE_PREV,
	VIX_MOVE_SENTENCE_NEXT,
	VIX_MOVE_PARAGRAPH_PREV,
	VIX_MOVE_PARAGRAPH_NEXT,
	VIX_MOVE_FUNCTION_START_PREV,
	VIX_MOVE_FUNCTION_START_NEXT,
	VIX_MOVE_FUNCTION_END_PREV,
	VIX_MOVE_FUNCTION_END_NEXT,
	VIX_MOVE_BLOCK_START,
	VIX_MOVE_BLOCK_END,
	VIX_MOVE_PARENTHESIS_START,
	VIX_MOVE_PARENTHESIS_END,
	VIX_MOVE_BRACKET_MATCH,
	VIX_MOVE_TO_LEFT,
	VIX_MOVE_TO_RIGHT,
	VIX_MOVE_TO_LINE_LEFT,
	VIX_MOVE_TO_LINE_RIGHT,
	VIX_MOVE_TILL_LEFT,
	VIX_MOVE_TILL_RIGHT,
	VIX_MOVE_TILL_LINE_LEFT,
	VIX_MOVE_TILL_LINE_RIGHT,
	VIX_MOVE_FILE_BEGIN,
	VIX_MOVE_FILE_END,
	VIX_MOVE_SEARCH_WORD_FORWARD,
	VIX_MOVE_SEARCH_WORD_BACKWARD,
	VIX_MOVE_SEARCH_REPEAT_FORWARD,
	VIX_MOVE_SEARCH_REPEAT_BACKWARD,
	VIX_MOVE_WINDOW_LINE_TOP,
	VIX_MOVE_WINDOW_LINE_MIDDLE,
	VIX_MOVE_WINDOW_LINE_BOTTOM,
	VIX_MOVE_CHANGELIST_NEXT,
	VIX_MOVE_CHANGELIST_PREV,
	VIX_MOVE_NOP,
	VIX_MOVE_PERCENT,
	VIX_MOVE_BYTE,
	VIX_MOVE_BYTE_LEFT,
	VIX_MOVE_BYTE_RIGHT,
	VIX_MOVE_INVALID, /* denotes the end of the "real" motions */
	/* pseudo motions: keep them at the end to save space in array definition */
	VIX_MOVE_TOTILL_REPEAT,
	VIX_MOVE_TOTILL_REVERSE,
	VIX_MOVE_SEARCH_FORWARD,
	VIX_MOVE_SEARCH_BACKWARD,
	VIX_MOVE_SEARCH_REPEAT,
	VIX_MOVE_SEARCH_REPEAT_REVERSE,
	VIX_MOVE_LAST, /* denotes the end of all motions */
};

/**
 * Set motion to perform.
 * @param vix The editor instance.
 * @param motion The motion to perform.
 * @param ... Additional arguments depending on the motion type.
 *
 * The following motions take an additional argument:
 *
 * - `VIX_MOVE_SEARCH_FORWARD` and `VIX_MOVE_SEARCH_BACKWARD`
 *
 * The search pattern as ``const char *``.
 *
 * - ``VIX_MOVE_{LEFT,RIGHT}_{TO,TILL}``
 *
 * The character to search for as ``const char *``.
 */
VIX_EXPORT bool vix_motion(Vix *vix, enum VixMotion motion, ...);

enum VixMotionType {
	VIX_MOTIONTYPE_LINEWISE  = 1 << 0,
	VIX_MOTIONTYPE_CHARWISE  = 1 << 1,
};

/**
 * Force currently specified motion to behave in line or character wise mode.
 * @param vix The editor instance.
 * @param type The motion type (line-wise or character-wise).
 */
VIX_EXPORT void vix_motion_type(Vix *vix, enum VixMotionType type);

/**
 * Motions take a starting position and transform it to an end position.
 * @param vix The editor instance.
 * @param win The window in which the motion is performed.
 * @param context User-supplied context for the motion.
 * @param pos The starting position.
 * @rst
 * .. note:: Should a motion not be possible, the original position must be returned.
 * TODO: we might want to change that to ``EPOS``?
 * @endrst
 */
typedef size_t (VixMotionFunction)(Vix *vix, Win *win, void *context, size_t pos);

/**
 * Register a motion function.
 * @param vix The editor instance.
 * @param context User-supplied context for the motion.
 * @param func The function implementing the motion logic.
 * @return Motion ID to be used with `vix_motion`.
 */
VIX_EXPORT int vix_motion_register(Vix *vix, void *context, VixMotionFunction *func);

/** @} */

/*
---
## Count Iteration
*/

/**
 * @defgroup vix_count Vix Count
 * @{
 */
/** No count was specified. */
#define VIX_COUNT_UNKNOWN (-1)
#define VIX_COUNT_DEFAULT(count, def) ((count) == VIX_COUNT_UNKNOWN ? (def) : (count))
#define VIX_COUNT_NORMALIZE(count)    ((count) < 0 ? VIX_COUNT_UNKNOWN : (count))
/**
 * Set the shell.
 * @param vix The editor instance.
 * @param new_shell The new shell to set.
 */
VIX_EXPORT void vix_shell_set(Vix *vix, const char *new_shell);

typedef struct {
	Vix *vix;
	int iteration;
	int count;
} VixCountIterator;

/**
 * Get iterator initialized with current count or ``def`` if not specified.
 * @param vix The editor instance.
 * @param def The default count if none is specified.
 */
VIX_EXPORT VixCountIterator vix_count_iterator_get(Vix *vix, int def);
/**
 * Get iterator initialized with a count value.
 * @param vix The editor instance.
 * @param count The count value to initialize with.
 */
VIX_EXPORT VixCountIterator vix_count_iterator_init(Vix *vix, int count);
/**
 * Increment iterator counter.
 * @param iter Pointer to the iterator.
 * @return Whether iteration should continue.
 * @rst
 * .. note:: Terminates iteration if the editor was
 * `interrupted <vix_interrupt>`_ in the meantime.
 * @endrst
 */
VIX_EXPORT bool vix_count_iterator_next(VixCountIterator *iter);

/** @} */

/*
---
## Text Objects
*/

/**
 * @defgroup vix_textobjs Vix Text Objects
 * @{
 */

/** Text object specifier. */
enum VixTextObject {
	VIX_TEXTOBJECT_INNER_WORD,
	VIX_TEXTOBJECT_OUTER_WORD,
	VIX_TEXTOBJECT_INNER_LONGWORD,
	VIX_TEXTOBJECT_OUTER_LONGWORD,
	VIX_TEXTOBJECT_SENTENCE,
	VIX_TEXTOBJECT_PARAGRAPH,
	VIX_TEXTOBJECT_PARAGRAPH_OUTER,
	VIX_TEXTOBJECT_OUTER_SQUARE_BRACKET,
	VIX_TEXTOBJECT_INNER_SQUARE_BRACKET,
	VIX_TEXTOBJECT_OUTER_CURLY_BRACKET,
	VIX_TEXTOBJECT_INNER_CURLY_BRACKET,
	VIX_TEXTOBJECT_OUTER_ANGLE_BRACKET,
	VIX_TEXTOBJECT_INNER_ANGLE_BRACKET,
	VIX_TEXTOBJECT_OUTER_PARENTHESIS,
	VIX_TEXTOBJECT_INNER_PARENTHESIS,
	VIX_TEXTOBJECT_OUTER_QUOTE,
	VIX_TEXTOBJECT_INNER_QUOTE,
	VIX_TEXTOBJECT_OUTER_SINGLE_QUOTE,
	VIX_TEXTOBJECT_INNER_SINGLE_QUOTE,
	VIX_TEXTOBJECT_OUTER_BACKTICK,
	VIX_TEXTOBJECT_INNER_BACKTICK,
	VIX_TEXTOBJECT_OUTER_LINE,
	VIX_TEXTOBJECT_INNER_LINE,
	VIX_TEXTOBJECT_INDENTATION,
	VIX_TEXTOBJECT_SEARCH_FORWARD,
	VIX_TEXTOBJECT_SEARCH_BACKWARD,
	VIX_TEXTOBJECT_INVALID,
};

/**
 * Text objects take a starting position and return a text range.
 * @param vix The editor instance.
 * @param win The window in which the text object is applied.
 * @param context User-supplied context for the text object.
 * @param pos The starting position.
 * @rst
 * .. note:: The originating position does not necessarily have to be contained in
 * the resulting range.
 * @endrst
 */
typedef Filerange (VixTextObjectFunction)(Vix *vix, Win *win, void *context, size_t pos);

/**
 * Register a new text object.
 * @param vix The editor instance.
 * @param type The type of the text object.
 * @param data User-supplied data for the text object.
 * @param func The function implementing the text object logic.
 * @return Text object ID to be used with `vix_textobject`.
 */
VIX_EXPORT int vix_textobject_register(Vix *vix, int type, void *data, VixTextObjectFunction *func);

/**
 * Set text object to use.
 * @param vix The editor instance.
 * @param textobj The text object to set.
 */
VIX_EXPORT bool vix_textobject(Vix *vix, enum VixTextObject textobj);

/** @} */

/*
---
## Marks
*/

/**
 * @defgroup vix_marks Vix Marks
 * @{
 */

/** Mark specifiers. */
enum VixMark {
	VIX_MARK_DEFAULT,
	VIX_MARK_SELECTION,
	VIX_MARK_a, VIX_MARK_b, VIX_MARK_c, VIX_MARK_d, VIX_MARK_e,
	VIX_MARK_f, VIX_MARK_g, VIX_MARK_h, VIX_MARK_i, VIX_MARK_j,
	VIX_MARK_k, VIX_MARK_l, VIX_MARK_m, VIX_MARK_n, VIX_MARK_o,
	VIX_MARK_p, VIX_MARK_q, VIX_MARK_r, VIX_MARK_s, VIX_MARK_t,
	VIX_MARK_u, VIX_MARK_v, VIX_MARK_w, VIX_MARK_x, VIX_MARK_y,
	VIX_MARK_z,
	VIX_MARK_INVALID,     /* has to be the last enum member */
};

/**
 * Translate between single character mark name and corresponding constant.
 * @param vix The editor instance.
 * @param mark The character representing the mark.
 */
VIX_EXPORT enum VixMark vix_mark_from(Vix *vix, char mark);
/**
 * Translate between mark constant and single character mark name.
 * @param vix The editor instance.
 * @param mark The mark constant.
 */
VIX_EXPORT char vix_mark_to(Vix *vix, enum VixMark mark);
/**
 * Specify mark to use.
 * @param vix The editor instance.
 * @param mark The mark to use.
 * @rst
 * .. note:: If none is specified `VIX_MARK_DEFAULT` will be used.
 * @endrst
 */
VIX_EXPORT void vix_mark(Vix *vix, enum VixMark mark);
/**
 * Get the currently used mark.
 * @param vix The editor instance.
 */
VIX_EXPORT enum VixMark vix_mark_used(Vix*);
/**
 * Store a set of ``Filerange``s in a mark.
 *
 * @param vix The editor instance.
 * @param win The window whose selections to store.
 * @param id The mark ID to use.
 * @param ranges The list of file ranges.
 */
VIX_EXPORT void vix_mark_set(Vix *vix, Win *win, enum VixMark id, FilerangeList ranges);
/**
 * Get an array of file ranges stored in the mark.
 * @param vix The editor instance.
 * @param win The window whose mark to retrieve.
 * @param id The mark ID to retrieve.
 * @return A list of file ranges.
 * @rst
 * .. warning:: The caller is responsible for freeing the list with ``da_release``.
 * @endrst
 */
VIX_EXPORT FilerangeList vix_mark_get(Vix *vix, Win *win, enum VixMark id);
/**
 * Normalize a list of Fileranges.
 * @param ranges The list of file ranges to normalize.
 *
 * Removes invalid ranges, merges overlapping ones and sorts
 * according to the start position.
 */
VIX_EXPORT void vix_mark_normalize(FilerangeList *ranges);
/**
 * Add selections of focused window to jump list. Equivalent to vix_jumplist(vix, 0).
 * @param vix The editor instance.
 */
#define vix_jumplist_save(vix) vix_jumplist((vix), 0)
/**
 * Navigate jump list by a specified amount. Wraps if advance exceeds list size.
 * @param vix The editor instance.
 * @param advance The amount to advance the cursor by. 0 saves the current selections.
 */
VIX_EXPORT void vix_jumplist(Vix *, int advance);
/** @} */

/*
---
## Registers
*/

/**
 * @defgroup vix_registers Vix Registers
 * @{
 */

/** Register specifiers. */
enum VixRegister {
	VIX_REG_DEFAULT,    /* used when no other register is specified */
	VIX_REG_ZERO,       /* yank register */
	VIX_REG_AMPERSAND,  /* last regex match */
	VIX_REG_1,          /* 1-9 last sub-expression matches */
	VIX_REG_2,
	VIX_REG_3,
	VIX_REG_4,
	VIX_REG_5,
	VIX_REG_6,
	VIX_REG_7,
	VIX_REG_8,
	VIX_REG_9,
	VIX_REG_BLACKHOLE,  /* /dev/null register */
	VIX_REG_CLIPBOARD,  /* system clipboard register */
	VIX_REG_PRIMARY,    /* system primary clipboard register */
	VIX_REG_DOT,        /* last inserted text, copy of VIX_MACRO_OPERATOR */
	VIX_REG_SEARCH,     /* last used search pattern "/ */
	VIX_REG_COMMAND,    /* last used :-command ": */
	VIX_REG_SHELL,      /* last used shell command given to either <, >, |, or ! */
	VIX_REG_NUMBER,     /* cursor number */
	VIX_REG_a, VIX_REG_b, VIX_REG_c, VIX_REG_d, VIX_REG_e,
	VIX_REG_f, VIX_REG_g, VIX_REG_h, VIX_REG_i, VIX_REG_j,
	VIX_REG_k, VIX_REG_l, VIX_REG_m, VIX_REG_n, VIX_REG_o,
	VIX_REG_p, VIX_REG_q, VIX_REG_r, VIX_REG_s, VIX_REG_t,
	VIX_REG_u, VIX_REG_v, VIX_REG_w, VIX_REG_x, VIX_REG_y,
	VIX_REG_z,
	VIX_MACRO_OPERATOR, /* records entered keys after an operator */
	VIX_REG_PROMPT, /* internal register which shadows DEFAULT in PROMPT mode */
	VIX_REG_INVALID, /* has to be the last 'real' register */
	VIX_REG_A, VIX_REG_B, VIX_REG_C, VIX_REG_D, VIX_REG_E,
	VIX_REG_F, VIX_REG_G, VIX_REG_H, VIX_REG_I, VIX_REG_J,
	VIX_REG_K, VIX_REG_L, VIX_REG_M, VIX_REG_N, VIX_REG_O,
	VIX_REG_P, VIX_REG_Q, VIX_REG_R, VIX_REG_S, VIX_REG_T,
	VIX_REG_U, VIX_REG_V, VIX_REG_W, VIX_REG_X, VIX_REG_Y,
	VIX_REG_Z,
	VIX_MACRO_LAST_RECORDED, /* pseudo macro referring to last recorded one */
};

/**
 * Translate between single character register name and corresponding constant.
 * @param vix The editor instance.
 * @param reg The character representing the register.
 */
VIX_EXPORT enum VixRegister vix_register_from(Vix *vix, char reg);
/**
 * Translate between register constant and single character register name.
 * @param vix The editor instance.
 * @param reg The register constant.
 */
VIX_EXPORT char vix_register_to(Vix *vix, enum VixRegister reg);
/**
 * Specify register to use.
 * @param vix The editor instance.
 * @param reg The register to use.
 * @rst
 * .. note:: If none is specified `VIX_REG_DEFAULT` will be used.
 * @endrst
 */
VIX_EXPORT void vix_register(Vix *vix, enum VixRegister reg);
/**
 * Get the currently used register.
 * @param vix The editor instance.
 */
VIX_EXPORT enum VixRegister vix_register_used(Vix*);
/**
 * Get register content.
 * @param vix The editor instance.
 * @param id The register ID to retrieve.
 * @return An array of ``TextString`` structs.
 * @rst
 * .. warning:: The caller must eventually free the array resources using
 * ``array_release``.
 * @endrst
 */
VIX_EXPORT str8_list vix_register_get(Vix *vix, enum VixRegister id);
/**
 * Set register content.
 * @param vix The editor instance.
 * @param id The register ID to set.
 * @param strings The list of ``TextString``s.
 */
VIX_EXPORT bool vix_register_set(Vix *vix, enum VixRegister id, str8_list strings);
/** @} */

/*
---
## Macros
*/

/**
 * @defgroup vix_macros Vix Macros
 * @{
 */
/**
 * Start recording a macro.
 * @param vix The editor instance.
 * @param reg The register to record the macro into.
 * @rst
 * .. note:: Fails if a recording is already ongoing.
 * @endrst
 */
VIX_EXPORT bool vix_macro_record(Vix *vix, enum VixRegister reg);
/**
 * Stop recording, fails if there is nothing to stop.
 * @param vix The editor instance.
 */
VIX_EXPORT bool vix_macro_record_stop(Vix*);
/**
 * Check whether a recording is currently ongoing.
 * @param vix The editor instance.
 */
VIX_EXPORT bool vix_macro_recording(Vix*);
/**
 * Replay a macro.
 * @param vix The editor instance.
 * @param reg The register containing the macro to replay.
 * @rst
 * .. note:: A macro currently being recorded can not be replayed.
 * @endrst
 */
VIX_EXPORT bool vix_macro_replay(Vix *vix, enum VixRegister reg);

/** @} */

/*
---
## Commands
*/

/**
 * @defgroup vix_cmds Vix Commands
 * @{
 */

/**
 * Execute a ``:``-command.
 * @param vix The editor instance.
 * @param cmd The command string to execute.
 */
VIX_EXPORT bool vix_cmd(Vix *vix, const char *cmd);

/** Command handler function. */
typedef bool (VixCommandFunction)(Vix*, Win*, void *data, bool force,
	const char *argv[], Selection*, Filerange*);
/**
 * Register new ``:``-command.
 * @param vix The editor instance.
 * @param name The command name.
 * @param help Optional single line help text.
 * @param context User supplied context pointer passed to the handler function.
 * @param func The function implementing the command logic.
 * @rst
 * .. note:: Any unique prefix of the command name will invoke the command.
 * @endrst
 */
VIX_EXPORT bool vix_cmd_register(Vix *vix, const char *name, const char *help, void *context, VixCommandFunction *func);

/**
 * Unregister ``:``-command.
 * @param vix The editor instance.
 * @param name The name of the command to unregister.
 */
VIX_EXPORT bool vix_cmd_unregister(Vix *vix, const char *name);

/** @} */

/*
---
## Options
*/

/**
 * @defgroup vix_options Vix Options
 * @{
 */
/** Option properties. */
enum VixOption {
	VIX_OPTION_TYPE_BOOL = 1 << 0,
	VIX_OPTION_TYPE_STRING = 1 << 1,
	VIX_OPTION_TYPE_NUMBER = 1 << 2,
	VIX_OPTION_VALUE_OPTIONAL = 1 << 3,
	VIX_OPTION_NEED_WINDOW = 1 << 4,
	VIX_OPTION_DEPRECATED = 1 << 5,
};

/**
 * Option handler function.
 * @param vix The editor instance.
 * @param win The window to which option should apply, might be ``NULL``.
 * @param context User provided context pointer as given to `vix_option_register`.
 * @param force Whether the option was specified with a bang ``!``.
 * @param option_flags The applicable option flags.
 * @param name Name of option which was set.
 * @param value The new option value.
 */
typedef bool (VixOptionFunction)(Vix *vix, Win *win, void *context, bool force,
                                 enum VixOption option_flags, const char *name, Arg *value);

/**
 * Register a new ``:set`` option.
 * @param vix The editor instance.
 * @param names A ``NULL`` terminated array of option names.
 * @param option_flags The applicable option flags.
 * @param func The function handling the option.
 * @param context User supplied context pointer passed to the handler function.
 * @param help Optional single line help text.
 * @rst
 * .. note:: Fails if any of the given option names is already registered.
 * @endrst
 */
VIX_EXPORT bool vix_option_register(Vix *vix, const char *names[], enum VixOption option_flags,
                                    VixOptionFunction *func, void *context, const char *help);
/**
 * Unregister an existing ``:set`` option.
 * @param vix The editor instance.
 * @param name The name of the option to unregister.
 * @rst
 * .. note:: Also unregisters all aliases as given to `vix_option_register`.
 * @endrst
 */
VIX_EXPORT bool vix_option_unregister(Vix *vix, const char *name);

/**
 * Execute any kind (``:``, ``?``, ``/``) of prompt command
 * @param vix The editor instance.
 * @param cmd The command string.
 */
VIX_EXPORT bool vix_prompt_cmd(Vix *vix, const char *cmd);

/**
 * Write newline separated list of available commands to ``buf``
 * @param vix The editor instance.
 * @param buf The buffer to write to.
 * @param prefix Prefix to filter command list by.
 */
VIX_EXPORT void vix_print_cmds(Vix*, Buffer *buf, const char *prefix);

/**
 * List all set options with a given prefix.
 * @param vix The editor instance.
 * @param buf The buffer to write to.
 * @param prefix Prefix to filter option list by.
 */
VIX_EXPORT void vix_print_options(Vix*, Buffer *buf, const char *prefix);

/**
 * Get current value of an option as string.
 * @param vix The editor instance.
 * @param name The name of the option.
 * @param buf The buffer to write the value to.
 */
VIX_EXPORT void vix_print_option_value(Vix *vix, const char *name, Buffer *buf);

/**
 * Pipe a given file range to an external process.
 * @param vix The editor instance.
 * @param file The file to pipe.
 * @param range The file range to pipe.
 * @param argv Argument list, must be ``NULL`` terminated.
 * @param stdout_context Context for stdout callback.
 * @param read_stdout Callback for stdout data.
 * @param stderr_context Context for stderr callback.
 * @param read_stderr Callback for stderr data.
 * @param fullscreen Whether the external process is a fullscreen program (e.g. curses based)
 *
 * If the range is invalid 'interactive' mode is enabled, meaning that
 * stdin and stderr are passed through the underlying command, stdout
 * points to vix' stderr.
 *
 * If ``argv`` contains only one non-NULL element the command is executed
 * through an intermediate shell (using ``/bin/sh -c argv[0]``) that is
 * argument expansion is performed by the shell. Otherwise the argument
 * list will be passed unmodified to ``execvp(argv[0], argv)``.
 *
 * If the ``read_stdout`` and ``read_stderr`` callbacks are non-NULL they
 * will be invoked when output from the forked process is available.
 *
 * If ``fullscreen`` is set to ``true`` the external process is assumed to
 * be a fullscreen program (e.g. curses based) and the ui context is
 * restored accordingly.
 *
 * @rst
 * .. warning:: The editor core is blocked until this function returns.
 * @endrst
 *
 * @return The exit status of the forked process.
 */
VIX_EXPORT int vix_pipe(Vix *vix, File *file, Filerange *range, const char *argv[],
	void *stdout_context, ssize_t (*read_stdout)(void *stdout_context, char *data, size_t len),
	void *stderr_context, ssize_t (*read_stderr)(void *stderr_context, char *data, size_t len),
	bool fullscreen);

/**
 * Pipe a Filerange to an external process, return its exit status and capture
 * everything that is written to stdout/stderr.
 * @param vix The editor instance.
 * @param file The file to pipe.
 * @param range The file range to pipe.
 * @param argv Argument list, must be ``NULL`` terminated.
 * @param out Data written to ``stdout``, will be ``NUL`` terminated.
 * @param err Data written to ``stderr``, will be ``NUL`` terminated.
 * @param fullscreen Whether the external process is a fullscreen program (e.g. curses based)
 * @rst
 * .. warning:: The pointers stored in ``out`` and ``err`` need to be `free(3)`-ed
 * by the caller.
 * @endrst
 */
VIX_EXPORT int vix_pipe_collect(Vix *vix, File *file, Filerange *range, const char *argv[], char **out, char **err, bool fullscreen);

/**
 * Pipe a buffer to an external process, return its exit status and capture
 * everything that is written to stdout/stderr.
 * @param vix The editor instance.
 * @param buf The buffer to pipe.
 * @param argv Argument list, must be ``NULL`` terminated.
 * @param out Data written to ``stdout``, will be ``NUL`` terminated.
 * @param err Data written to ``stderr``, will be ``NUL`` terminated.
 * @param fullscreen Whether the external process is a fullscreen program (e.g. curses based)
 * @rst
 * .. warning:: The pointers stored in ``out`` and ``err`` need to be `free(3)`-ed
 * by the caller.
 * @endrst
 */
VIX_EXPORT int vix_pipe_buf_collect(Vix *vix, const char *buf, const char *argv[], char **out, char **err, bool fullscreen);

/** @} */

/*
---
## Keys
*/

/**
 * @defgroup vix_keys Vix Keys
 * @{
 */
/**
 * Advance to the start of the next symbolic key.
 * @param vix The editor instance.
 * @param keys The current symbolic key string.
 *
 * Given the start of a symbolic key, returns a pointer to the start of the one
 * immediately following it.
 */
VIX_EXPORT const char *vix_keys_next(Vix *vix, const char *keys);
/**
 * Convert next symbolic key to an Unicode code point, returns ``-1`` for unknown keys.
 * @param vix The editor instance.
 * @param keys The symbolic key string.
 */
VIX_EXPORT long vix_keys_codepoint(Vix *vix, const char *keys);
/**
 * Convert next symbolic key to a UTF-8 sequence.
 * @param vix The editor instance.
 * @param keys The symbolic key string.
 * @param utf8 Buffer to store the UTF-8 sequence.
 * @return Whether conversion was successful, if not ``utf8`` is left unmodified.
 * @rst
 * .. note:: Guarantees that ``utf8`` is NUL terminated on success.
 * @endrst
 */
VIX_EXPORT bool vix_keys_utf8(Vix *vix, const char *keys, char utf8[4+1]);
/**
 * Process symbolic keys as if they were user originated input.
 * @param vix The editor instance.
 * @param keys The symbolic key string to feed.
 */
VIX_EXPORT void vix_keys_feed(Vix *vix, const char *keys);
/** @} */

/*
---
## Miscellaneous
*/

/**
 * @defgroup vix_misc Vix Miscellaneous
 * @{
 */

/**
 * Get a regex object matching pattern.
 * @param vix The editor instance.
 * @param pattern The regex pattern to compile, if ``NULL`` the most recently used
 * one is substituted.
 * @return A Regex object or ``NULL`` in case of an error.
 * @rst
 * .. warning:: The caller must free the regex object using `text_regex_free`.
 * @endrst
 */
VIX_EXPORT Regex *vix_regex(Vix *vix, const char *pattern);

/**
 * Take an undo snapshot to which we can later revert.
 * @param vix The editor instance.
 * @param file The file for which to take a snapshot.
 * @rst
 * .. note:: Does nothing when invoked while replaying a macro.
 * @endrst
 */
VIX_EXPORT void vix_file_snapshot(Vix *vix, File *file);
/** @} */

/* TODO: expose proper API to iterate through files etc */
VIX_EXPORT Text *vix_text(Vix*);
VIX_EXPORT View *vix_view(Vix*);

#endif
