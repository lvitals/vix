#include "vix.c"

static Vix vix[1];

#define PAGE      INT_MAX
#define PAGE_HALF (INT_MAX-1)
#define WINDOW_RESIZE_STEP 20

/** functions to be called from keybindings */
/* X(impl, enum, argument, lua_name, help) */
#define KEY_ACTION_LIST(X) \
	X(ka_call,                            INSERT_NEWLINE,                   .f = vix_insert_nl,                       "vix-insert-newline",                  "Insert a line break (depending on file type)") \
	X(ka_call,                            INSERT_TAB,                       .f = vix_insert_tab,                      "vix-insert-tab",                      "Insert a tab (might be converted to spaces)") \
	X(ka_call,                            REDRAW,                           .f = vix_redraw,                          "vix-redraw",                          "Redraw current editor content") \
	X(ka_call,                            WINDOW_NEXT,                      .f = vix_window_next,                     "vix-window-next",                     "Focus next window") \
	X(ka_call,                            WINDOW_PREV,                      .f = vix_window_prev,                     "vix-window-prev",                     "Focus previous window") \
	X(ka_tab_new,                         TAB_NEW,                          0,                                        "vix-tab-new",                         "Create a new tab") \
	X(ka_tabview_toggle,                  TAB_VIEW_TOGGLE,                  0,                                        "vix-tabview-toggle",                  "Toggle between tabbed and split view") \
	X(ka_tab_next,                        TAB_NEXT,                         0,                                        "vix-tab-next",                        "Switch to next tab") \
	X(ka_tab_prev,                        TAB_PREV,                         0,                                        "vix-tab-prev",                        "Switch to previous tab") \
	X(ka_window_next_max,                 WINDOW_NEXT_MAX,                  0,                                        "vix-window-next-max",                 "Focus next window and maximize it") \
	X(ka_window_prev_max,                 WINDOW_PREV_MAX,                  0,                                        "vix-window-prev-max",                 "Focus previous window and maximize it") \
	X(ka_window_maximize,                 WINDOW_MAXIMIZE,                  0,                                        "vix-window-maximize",                 "Maximize current window") \
	X(ka_layout,                          WINDOW_LAYOUT_HORIZONTAL,         .i = UI_LAYOUT_HORIZONTAL,                "vix-window-layout-horizontal",        "Use horizontal window layout") \
	X(ka_layout,                          WINDOW_LAYOUT_VERTICAL,           .i = UI_LAYOUT_VERTICAL,                  "vix-window-layout-vertical",          "Use vertical window layout") \
	X(ka_window_resize,                   WINDOW_RESIZE_INC,                .i = +10,                                 "vix-window-resize-inc",               "Increase window size") \
	X(ka_window_resize,                   WINDOW_RESIZE_DEC,                .i = -10,                                 "vix-window-resize-dec",               "Decrease window size") \
	X(ka_window_resize,                   WINDOW_RESIZE_RESET,              .i = 0,                                   "vix-window-resize-reset",             "Reset window sizes to equal") \
	X(ka_count,                           COUNT,                            0,                                        "vix-count",                           "Count specifier") \
	X(ka_delete,                          DELETE_CHAR_NEXT,                 .i = VIX_MOVE_CHAR_NEXT,                  "vix-delete-char-next",                "Delete the next character") \
	X(ka_delete,                          DELETE_CHAR_PREV,                 .i = VIX_MOVE_CHAR_PREV,                  "vix-delete-char-prev",                "Delete the previous character") \
	X(ka_delete,                          DELETE_LINE_BEGIN,                .i = VIX_MOVE_LINE_BEGIN,                 "vix-delete-line-begin",               "Delete until the start of the current line") \
	X(ka_delete,                          DELETE_WORD_PREV,                 .i = VIX_MOVE_WORD_START_PREV,            "vix-delete-word-prev",                "Delete the previous WORD") \
	X(ka_earlier,                         EARLIER,                          0,                                        "vix-earlier",                         "Goto older text state") \
	X(ka_gotoline,                        CURSOR_LINE_FIRST,                .i = -1,                                  "vix-motion-line-first",               "Move cursor to given line (defaults to first)") \
	X(ka_gotoline,                        CURSOR_LINE_LAST,                 .i = +1,                                  "vix-motion-line-last",                "Move cursor to given line (defaults to last)") \
	X(ka_insert_register,                 INSERT_REGISTER,                  0,                                        "vix-insert-register",                 "Insert specified register content") \
	X(ka_insert_verbatim,                 INSERT_VERBATIM,                  0,                                        "vix-insert-verbatim",                 "Insert Unicode character based on code point") \
	X(ka_insertmode,                      APPEND_CHAR_NEXT,                 .i = VIX_MOVE_LINE_CHAR_NEXT,             "vix-append-char-next",                "Append text after the cursor") \
	X(ka_insertmode,                      APPEND_LINE_END,                  .i = VIX_MOVE_LINE_END,                   "vix-append-line-end",                 "Append text after the end of the line") \
	X(ka_insertmode,                      INSERT_LINE_START,                .i = VIX_MOVE_LINE_START,                 "vix-insert-line-start",               "Insert text before the first non-blank in the line") \
	X(ka_insertmode,                      MODE_INSERT,                      .i = VIX_MOVE_NOP,                        "vix-mode-insert",                     "Enter insert mode") \
	X(ka_join,                            JOIN_LINES,                       .s = " ",                                 "vix-join-lines",                      "Join selected lines") \
	X(ka_join,                            JOIN_LINES_TRIM,                  .s = "",                                  "vix-join-lines-trim",                 "Join selected lines, remove white space") \
	X(ka_jumplist,                        JUMPLIST_NEXT,                    .i = +1,                                  "vix-jumplist-next",                   "Go to newer cursor position in jump list") \
	X(ka_jumplist,                        JUMPLIST_PREV,                    .i = -1,                                  "vix-jumplist-prev",                   "Go to older cursor position in jump list") \
	X(ka_jumplist,                        JUMPLIST_SAVE,                    .i = 0,                                   "vix-jumplist-save",                   "Save current selections in jump list") \
	X(ka_later,                           LATER,                            0,                                        "vix-later",                           "Goto newer text state") \
	X(ka_macro_record,                    MACRO_RECORD,                     0,                                        "vix-macro-record",                    "Record macro into given register") \
	X(ka_macro_replay,                    MACRO_REPLAY,                     0,                                        "vix-macro-replay",                    "Replay macro, execute the content of the given register") \
	X(ka_mark,                            MARK,                             0,                                        "vix-mark",                            "Use given mark for next action") \
	X(ka_movement,                        CURSOR_BLOCK_END,                 .i = VIX_MOVE_BLOCK_END,                  "vix-motion-block-end",                "Move cursor to the closing curly brace in a block") \
	X(ka_movement,                        CURSOR_BLOCK_START,               .i = VIX_MOVE_BLOCK_START,                "vix-motion-block-start",              "Move cursor to the opening curly brace in a block") \
	X(ka_movement,                        CURSOR_BYTE,                      .i = VIX_MOVE_BYTE,                       "vix-motion-byte",                     "Move to absolute byte position") \
	X(ka_movement,                        CURSOR_BYTE_LEFT,                 .i = VIX_MOVE_BYTE_LEFT,                  "vix-motion-byte-left",                "Move count bytes to the left") \
	X(ka_movement,                        CURSOR_BYTE_RIGHT,                .i = VIX_MOVE_BYTE_RIGHT,                 "vix-motion-byte-right",               "Move count bytes to the right") \
	X(ka_movement,                        CURSOR_CHAR_NEXT,                 .i = VIX_MOVE_CHAR_NEXT,                  "vix-motion-char-next",                "Move cursor right, to the next character") \
	X(ka_movement,                        CURSOR_CHAR_PREV,                 .i = VIX_MOVE_CHAR_PREV,                  "vix-motion-char-prev",                "Move cursor left, to the previous character") \
	X(ka_movement,                        CURSOR_CODEPOINT_NEXT,            .i = VIX_MOVE_CODEPOINT_NEXT,             "vix-motion-codepoint-next",           "Move to the next Unicode codepoint") \
	X(ka_movement,                        CURSOR_CODEPOINT_PREV,            .i = VIX_MOVE_CODEPOINT_PREV,             "vix-motion-codepoint-prev",           "Move to the previous Unicode codepoint") \
	X(ka_movement,                        CURSOR_COLUMN,                    .i = VIX_MOVE_COLUMN,                     "vix-motion-column",                   "Move cursor to given column of current line") \
	X(ka_movement,                        CURSOR_LINE_BEGIN,                .i = VIX_MOVE_LINE_BEGIN,                 "vix-motion-line-begin",               "Move cursor to first character of the line") \
	X(ka_movement,                        CURSOR_LINE_CHAR_NEXT,            .i = VIX_MOVE_LINE_CHAR_NEXT,             "vix-motion-line-char-next",           "Move cursor right, to the next character on the same line") \
	X(ka_movement,                        CURSOR_LINE_CHAR_PREV,            .i = VIX_MOVE_LINE_CHAR_PREV,             "vix-motion-line-char-prev",           "Move cursor left,  to the previous character on the same line") \
	X(ka_movement,                        CURSOR_LINE_DOWN,                 .i = VIX_MOVE_LINE_DOWN,                  "vix-motion-line-down",                "Move cursor line downwards") \
	X(ka_movement,                        CURSOR_LINE_END,                  .i = VIX_MOVE_LINE_END,                   "vix-motion-line-end",                 "Move cursor to end of the line") \
	X(ka_movement,                        CURSOR_LINE_FINISH,               .i = VIX_MOVE_LINE_FINISH,                "vix-motion-line-finish",              "Move cursor to last non-blank character of the line") \
	X(ka_movement,                        CURSOR_LINE_START,                .i = VIX_MOVE_LINE_START,                 "vix-motion-line-start",               "Move cursor to first non-blank character of the line") \
	X(ka_movement,                        CURSOR_LINE_UP,                   .i = VIX_MOVE_LINE_UP,                    "vix-motion-line-up",                  "Move cursor line upwards") \
	X(ka_movement,                        CURSOR_LONGWORD_END_NEXT,         .i = VIX_MOVE_LONGWORD_END_NEXT,          "vix-motion-bigword-end-next",         "Move cursor forward to the end of WORD") \
	X(ka_movement,                        CURSOR_LONGWORD_END_PREV,         .i = VIX_MOVE_LONGWORD_END_PREV,          "vix-motion-bigword-end-prev",         "Move cursor backwards to the end of WORD") \
	X(ka_movement,                        CURSOR_LONGWORD_START_NEXT,       .i = VIX_MOVE_LONGWORD_START_NEXT,        "vix-motion-bigword-start-next",       "Move cursor WORDS forwards") \
	X(ka_movement,                        CURSOR_LONGWORD_START_PREV,       .i = VIX_MOVE_LONGWORD_START_PREV,        "vix-motion-bigword-start-prev",       "Move cursor WORDS backwards") \
	X(ka_movement,                        CURSOR_PARAGRAPH_NEXT,            .i = VIX_MOVE_PARAGRAPH_NEXT,             "vix-motion-paragraph-next",           "Move cursor paragraph forward") \
	X(ka_movement,                        CURSOR_PARAGRAPH_PREV,            .i = VIX_MOVE_PARAGRAPH_PREV,             "vix-motion-paragraph-prev",           "Move cursor paragraph backward") \
	X(ka_movement,                        CURSOR_PARENTHESIS_END,           .i = VIX_MOVE_PARENTHESIS_END,            "vix-motion-parenthesis-end",          "Move cursor to the closing parenthesis inside a pair of parentheses") \
	X(ka_movement,                        CURSOR_PARENTHESIS_START,         .i = VIX_MOVE_PARENTHESIS_START,          "vix-motion-parenthesis-start",        "Move cursor to the opening parenthesis inside a pair of parentheses") \
	X(ka_movement,                        CURSOR_SCREEN_LINE_BEGIN,         .i = VIX_MOVE_SCREEN_LINE_BEGIN,          "vix-motion-screenline-begin",         "Move cursor to beginning of screen/display line") \
	X(ka_movement,                        CURSOR_SCREEN_LINE_DOWN,          .i = VIX_MOVE_SCREEN_LINE_DOWN,           "vix-motion-screenline-down",          "Move cursor screen/display line downwards") \
	X(ka_movement,                        CURSOR_SCREEN_LINE_END,           .i = VIX_MOVE_SCREEN_LINE_END,            "vix-motion-screenline-end",           "Move cursor to end of screen/display line") \
	X(ka_movement,                        CURSOR_SCREEN_LINE_MIDDLE,        .i = VIX_MOVE_SCREEN_LINE_MIDDLE,         "vix-motion-screenline-middle",        "Move cursor to middle of screen/display line") \
	X(ka_movement,                        CURSOR_SCREEN_LINE_UP,            .i = VIX_MOVE_SCREEN_LINE_UP,             "vix-motion-screenline-up",            "Move cursor screen/display line upwards") \
	X(ka_movement,                        CURSOR_SEARCH_REPEAT,             .i = VIX_MOVE_SEARCH_REPEAT,              "vix-motion-search-repeat",            "Move cursor to next match") \
	X(ka_movement,                        CURSOR_SEARCH_REPEAT_BACKWARD,    .i = VIX_MOVE_SEARCH_REPEAT_BACKWARD,     "vix-motion-search-repeat-backward",   "Move cursor to previous match in backward direction") \
	X(ka_movement,                        CURSOR_SEARCH_REPEAT_FORWARD,     .i = VIX_MOVE_SEARCH_REPEAT_FORWARD,      "vix-motion-search-repeat-forward",    "Move cursor to next match in forward direction") \
	X(ka_movement,                        CURSOR_SEARCH_REPEAT_REVERSE,     .i = VIX_MOVE_SEARCH_REPEAT_REVERSE,      "vix-motion-search-repeat-reverse",    "Move cursor to next match in opposite direction") \
	X(ka_movement,                        CURSOR_SEARCH_WORD_BACKWARD,      .i = VIX_MOVE_SEARCH_WORD_BACKWARD,       "vix-motion-search-word-backward",     "Move cursor to previous occurrence of the word under cursor") \
	X(ka_movement,                        CURSOR_SEARCH_WORD_FORWARD,       .i = VIX_MOVE_SEARCH_WORD_FORWARD,        "vix-motion-search-word-forward",      "Move cursor to next occurrence of the word under cursor") \
	X(ka_movement,                        CURSOR_SENTENCE_NEXT,             .i = VIX_MOVE_SENTENCE_NEXT,              "vix-motion-sentence-next",            "Move cursor sentence forward") \
	X(ka_movement,                        CURSOR_SENTENCE_PREV,             .i = VIX_MOVE_SENTENCE_PREV,              "vix-motion-sentence-prev",            "Move cursor sentence backward") \
	X(ka_movement,                        CURSOR_WINDOW_LINE_BOTTOM,        .i = VIX_MOVE_WINDOW_LINE_BOTTOM,         "vix-motion-window-line-bottom",       "Move cursor to bottom line of the window") \
	X(ka_movement,                        CURSOR_WINDOW_LINE_MIDDLE,        .i = VIX_MOVE_WINDOW_LINE_MIDDLE,         "vix-motion-window-line-middle",       "Move cursor to middle line of the window") \
	X(ka_movement,                        CURSOR_WINDOW_LINE_TOP,           .i = VIX_MOVE_WINDOW_LINE_TOP,            "vix-motion-window-line-top",          "Move cursor to top line of the window") \
	X(ka_movement,                        CURSOR_WORD_END_NEXT,             .i = VIX_MOVE_WORD_END_NEXT,              "vix-motion-word-end-next",            "Move cursor forward to the end of word") \
	X(ka_movement,                        CURSOR_WORD_END_PREV,             .i = VIX_MOVE_WORD_END_PREV,              "vix-motion-word-end-prev",            "Move cursor backwards to the end of word") \
	X(ka_movement,                        CURSOR_WORD_START_NEXT,           .i = VIX_MOVE_WORD_START_NEXT,            "vix-motion-word-start-next",          "Move cursor words forwards") \
	X(ka_movement,                        CURSOR_WORD_START_PREV,           .i = VIX_MOVE_WORD_START_PREV,            "vix-motion-word-start-prev",          "Move cursor words backwards") \
	X(ka_movement,                        TOTILL_REPEAT,                    .i = VIX_MOVE_TOTILL_REPEAT,              "vix-motion-totill-repeat",            "Repeat latest to/till motion") \
	X(ka_movement,                        TOTILL_REVERSE,                   .i = VIX_MOVE_TOTILL_REVERSE,             "vix-motion-totill-reverse",           "Repeat latest to/till motion but in opposite direction") \
	X(ka_movement_key,                    TILL_LEFT,                        .i = VIX_MOVE_TILL_LEFT,                  "vix-motion-till-left",                "Till after the occurrence of character to the left") \
	X(ka_movement_key,                    TILL_LINE_LEFT,                   .i = VIX_MOVE_TILL_LINE_LEFT,             "vix-motion-till-line-left",           "Till after the occurrence of character to the left on the current line") \
	X(ka_movement_key,                    TILL_LINE_RIGHT,                  .i = VIX_MOVE_TILL_LINE_RIGHT,            "vix-motion-till-line-right",          "Till before the occurrence of character to the right on the current line") \
	X(ka_movement_key,                    TILL_RIGHT,                       .i = VIX_MOVE_TILL_RIGHT,                 "vix-motion-till-right",               "Till before the occurrence of character to the right") \
	X(ka_movement_key,                    TO_LEFT,                          .i = VIX_MOVE_TO_LEFT,                    "vix-motion-to-left",                  "To the first occurrence of character to the left") \
	X(ka_movement_key,                    TO_LINE_LEFT,                     .i = VIX_MOVE_TO_LINE_LEFT,               "vix-motion-to-line-left",             "To the first occurrence of character to the left on the current line") \
	X(ka_movement_key,                    TO_LINE_RIGHT,                    .i = VIX_MOVE_TO_LINE_RIGHT,              "vix-motion-to-line-right",            "To the first occurrence of character to the right on the current line") \
	X(ka_movement_key,                    TO_RIGHT,                         .i = VIX_MOVE_TO_RIGHT,                   "vix-motion-to-right",                 "To the first occurrence of character to the right") \
	X(ka_nop,                             NOP,                              0,                                        "vix-nop",                             "Ignore key, do nothing") \
	X(ka_normalmode_escape,               MODE_NORMAL_ESCAPE,               0,                                        "vix-mode-normal-escape",              "Reset count or remove all non-primary selections") \
	X(ka_openline,                        OPEN_LINE_ABOVE,                  .i = -1,                                  "vix-open-line-above",                 "Begin a new line above the cursor") \
	X(ka_openline,                        OPEN_LINE_BELOW,                  .i = +1,                                  "vix-open-line-below",                 "Begin a new line below the cursor") \
	X(ka_operator,                        OPERATOR_CHANGE,                  .i = VIX_OP_CHANGE,                       "vix-operator-change",                 "Change operator") \
	X(ka_operator,                        OPERATOR_DELETE,                  .i = VIX_OP_DELETE,                       "vix-operator-delete",                 "Delete operator") \
	X(ka_operator,                        OPERATOR_SHIFT_LEFT,              .i = VIX_OP_SHIFT_LEFT,                   "vix-operator-shift-left",             "Shift left operator") \
	X(ka_operator,                        OPERATOR_SHIFT_RIGHT,             .i = VIX_OP_SHIFT_RIGHT,                  "vix-operator-shift-right",            "Shift right operator") \
	X(ka_operator,                        OPERATOR_YANK,                    .i = VIX_OP_YANK,                         "vix-operator-yank",                   "Yank operator") \
	X(ka_operator,                        PUT_AFTER,                        .i = VIX_OP_PUT_AFTER,                    "vix-put-after",                       "Put text after the cursor") \
	X(ka_operator,                        PUT_BEFORE,                       .i = VIX_OP_PUT_BEFORE,                   "vix-put-before",                      "Put text before the cursor") \
	X(ka_operator,                        SELECTIONS_NEW_LINES_BEGIN,       .i = VIX_OP_CURSOR_SOL,                   "vix-selection-new-lines-begin",       "Create a new selection at the start of every line covered by selection") \
	X(ka_operator,                        SELECTIONS_NEW_LINES_END,         .i = VIX_OP_CURSOR_EOL,                   "vix-selection-new-lines-end",         "Create a new selection at the end of every line covered by selection") \
	X(ka_percent,                         CURSOR_PERCENT,                   0,                                        "vix-motion-percent",                  "Move to count % of file or matching item") \
	X(ka_prompt_show,                     PROMPT_SEARCH_BACKWARD,           .s = "?",                                 "vix-search-backward",                 "Search backward") \
	X(ka_prompt_show,                     PROMPT_SEARCH_FORWARD,            .s = "/",                                 "vix-search-forward",                  "Search forward") \
	X(ka_prompt_show,                     PROMPT_SHOW,                      .s = ":",                                 "vix-prompt-show",                     "Show editor command line prompt") \
	X(ka_redo,                            REDO,                             0,                                        "vix-redo",                            "Redo last change") \
	X(ka_reg,                             REGISTER,                         0,                                        "vix-register",                        "Use given register for next operator") \
	X(ka_repeat,                          REPEAT,                           0,                                        "vix-repeat",                          "Repeat latest editor command") \
	X(ka_replace,                         REPLACE_CHAR,                     0,                                        "vix-replace-char",                    "Replace the character under the cursor") \
	X(ka_replacemode,                     MODE_REPLACE,                     .i = VIX_MOVE_NOP,                        "vix-mode-replace",                    "Enter replace mode") \
	X(ka_selection_end,                   SELECTION_FLIP,                   0,                                        "vix-selection-flip",                  "Flip selection, move cursor to other end") \
	X(ka_selections_align,                SELECTIONS_ALIGN,                 0,                                        "vix-selections-align",                "Try to align all selections on the same column") \
	X(ka_selections_align_indent,         SELECTIONS_ALIGN_INDENT_LEFT,     .i = -1,                                  "vix-selections-align-indent-left",    "Left-align all selections by inserting spaces") \
	X(ka_selections_align_indent,         SELECTIONS_ALIGN_INDENT_RIGHT,    .i = +1,                                  "vix-selections-align-indent-right",   "Right-align all selections by inserting spaces") \
	X(ka_selections_clear,                SELECTIONS_REMOVE_ALL,            0,                                        "vix-selections-remove-all",           "Remove all but the primary selection") \
	X(ka_selections_complement,           SELECTIONS_COMPLEMENT,            0,                                        "vix-selections-complement",           "Complement selections") \
	X(ka_selections_intersect,            SELECTIONS_INTERSECT,             0,                                        "vix-selections-intersect",            "Intersect with selections from mark") \
	X(ka_selections_match_next,           SELECTIONS_NEW_MATCH_ALL,         .b = true,                                "vix-selection-new-match-all",         "Select all regions matching the current selection") \
	X(ka_selections_match_next,           SELECTIONS_NEW_MATCH_NEXT,        0,                                        "vix-selection-new-match-next",        "Select the next region matching the current selection") \
	X(ka_selections_match_skip,           SELECTIONS_NEW_MATCH_SKIP,        0,                                        "vix-selection-new-match-skip",        "Clear current selection, but select next match") \
	X(ka_selections_minus,                SELECTIONS_MINUS,                 0,                                        "vix-selections-minus",                "Subtract selections from mark") \
	X(ka_selections_navigate,             SELECTIONS_NEXT,                  .i = +PAGE_HALF,                          "vix-selection-next",                  "Move to the next selection") \
	X(ka_selections_navigate,             SELECTIONS_PREV,                  .i = -PAGE_HALF,                          "vix-selection-prev",                  "Move to the previous selection") \
	X(ka_selections_new,                  SELECTIONS_NEW_LINE_ABOVE,        .i = -1,                                  "vix-selection-new-lines-above",       "Create a new selection on the line above") \
	X(ka_selections_new,                  SELECTIONS_NEW_LINE_ABOVE_FIRST,  .i = INT_MIN,                             "vix-selection-new-lines-above-first", "Create a new selection on the line above the first selection") \
	X(ka_selections_new,                  SELECTIONS_NEW_LINE_BELOW,        .i = +1,                                  "vix-selection-new-lines-below",       "Create a new selection on the line below") \
	X(ka_selections_new,                  SELECTIONS_NEW_LINE_BELOW_LAST,   .i = INT_MAX,                             "vix-selection-new-lines-below-last",  "Create a new selection on the line below the last selection") \
	X(ka_selections_remove,               SELECTIONS_REMOVE_LAST,           0,                                        "vix-selections-remove-last",          "Remove primary selection") \
	X(ka_selections_remove_column,        SELECTIONS_REMOVE_COLUMN,         .i = 1,                                   "vix-selections-remove-column",        "Remove count selection column") \
	X(ka_selections_remove_column_except, SELECTIONS_REMOVE_COLUMN_EXCEPT,  .i = 1,                                   "vix-selections-remove-column-except", "Remove all but the count selection column") \
	X(ka_selections_restore,              SELECTIONS_RESTORE,               0,                                        "vix-selections-restore",              "Restore selections from mark") \
	X(ka_selections_rotate,               SELECTIONS_ROTATE_LEFT,           .i = -1,                                  "vix-selections-rotate-left",          "Rotate selections left") \
	X(ka_selections_rotate,               SELECTIONS_ROTATE_RIGHT,          .i = +1,                                  "vix-selections-rotate-right",         "Rotate selections right") \
	X(ka_selections_save,                 SELECTIONS_SAVE,                  0,                                        "vix-selections-save",                 "Save currently active selections to mark") \
	X(ka_selections_trim,                 SELECTIONS_TRIM,                  0,                                        "vix-selections-trim",                 "Remove leading and trailing white space from selections") \
	X(ka_selections_union,                SELECTIONS_UNION,                 0,                                        "vix-selections-union",                "Add selections from mark") \
	X(ka_suspend,                         EDITOR_SUSPEND,                   0,                                        "vix-suspend",                         "Suspend the editor") \
	X(ka_switchmode,                      MODE_NORMAL,                      .i = VIX_MODE_NORMAL,                     "vix-mode-normal",                     "Enter normal mode") \
	X(ka_switchmode,                      MODE_WINDOW,                      .i = VIX_MODE_WINDOW,                     "vix-mode-window",                     "Enter window management mode") \
	X(ka_switchmode,                      MODE_VISUAL,                      .i = VIX_MODE_VISUAL,                     "vix-mode-visual-charwise",            "Enter characterwise visual mode") \
	X(ka_switchmode,                      MODE_VISUAL_LINE,                 .i = VIX_MODE_VISUAL_LINE,                "vix-mode-visual-linewise",            "Enter linewise visual mode") \
	X(ka_text_object,                     TEXT_OBJECT_ANGLE_BRACKET_INNER,  .i = VIX_TEXTOBJECT_INNER_ANGLE_BRACKET,  "vix-textobject-angle-bracket-inner",  "<> block (inner variant)") \
	X(ka_text_object,                     TEXT_OBJECT_ANGLE_BRACKET_OUTER,  .i = VIX_TEXTOBJECT_OUTER_ANGLE_BRACKET,  "vix-textobject-angle-bracket-outer",  "<> block (outer variant)") \
	X(ka_text_object,                     TEXT_OBJECT_BACKTICK_INNER,       .i = VIX_TEXTOBJECT_INNER_BACKTICK,       "vix-textobject-backtick-inner",       "A backtick delimited string (inner variant)") \
	X(ka_text_object,                     TEXT_OBJECT_BACKTICK_OUTER,       .i = VIX_TEXTOBJECT_OUTER_BACKTICK,       "vix-textobject-backtick-outer",       "A backtick delimited string (outer variant)") \
	X(ka_text_object,                     TEXT_OBJECT_CURLY_BRACKET_INNER,  .i = VIX_TEXTOBJECT_INNER_CURLY_BRACKET,  "vix-textobject-curly-bracket-inner",  "{} block (inner variant)") \
	X(ka_text_object,                     TEXT_OBJECT_CURLY_BRACKET_OUTER,  .i = VIX_TEXTOBJECT_OUTER_CURLY_BRACKET,  "vix-textobject-curly-bracket-outer",  "{} block (outer variant)") \
	X(ka_text_object,                     TEXT_OBJECT_INDENTATION,          .i = VIX_TEXTOBJECT_INDENTATION,          "vix-textobject-indentation",          "All adjacent lines with the same indentation level as the current one") \
	X(ka_text_object,                     TEXT_OBJECT_LINE_INNER,           .i = VIX_TEXTOBJECT_INNER_LINE,           "vix-textobject-line-inner",           "The whole line, excluding leading and trailing whitespace") \
	X(ka_text_object,                     TEXT_OBJECT_LINE_OUTER,           .i = VIX_TEXTOBJECT_OUTER_LINE,           "vix-textobject-line-outer",           "The whole line") \
	X(ka_text_object,                     TEXT_OBJECT_LONGWORD_INNER,       .i = VIX_TEXTOBJECT_INNER_LONGWORD,       "vix-textobject-bigword-inner",        "A WORD leading and trailing whitespace excluded") \
	X(ka_text_object,                     TEXT_OBJECT_LONGWORD_OUTER,       .i = VIX_TEXTOBJECT_OUTER_LONGWORD,       "vix-textobject-bigword-outer",        "A WORD leading and trailing whitespace included") \
	X(ka_text_object,                     TEXT_OBJECT_PARAGRAPH,            .i = VIX_TEXTOBJECT_PARAGRAPH,            "vix-textobject-paragraph",            "A paragraph") \
	X(ka_text_object,                     TEXT_OBJECT_PARAGRAPH_OUTER,      .i = VIX_TEXTOBJECT_PARAGRAPH_OUTER,      "vix-textobject-paragraph-outer",      "A paragraph (outer variant)") \
	X(ka_text_object,                     TEXT_OBJECT_PARENTHESIS_INNER,    .i = VIX_TEXTOBJECT_INNER_PARENTHESIS,    "vix-textobject-parenthesis-inner",    "() block (inner variant)") \
	X(ka_text_object,                     TEXT_OBJECT_PARENTHESIS_OUTER,    .i = VIX_TEXTOBJECT_OUTER_PARENTHESIS,    "vix-textobject-parenthesis-outer",    "() block (outer variant)") \
	X(ka_text_object,                     TEXT_OBJECT_QUOTE_INNER,          .i = VIX_TEXTOBJECT_INNER_QUOTE,          "vix-textobject-quote-inner",          "A quoted string, excluding the quotation marks") \
	X(ka_text_object,                     TEXT_OBJECT_QUOTE_OUTER,          .i = VIX_TEXTOBJECT_OUTER_QUOTE,          "vix-textobject-quote-outer",          "A quoted string, including the quotation marks") \
	X(ka_text_object,                     TEXT_OBJECT_SEARCH_BACKWARD,      .i = VIX_TEXTOBJECT_SEARCH_BACKWARD,      "vix-textobject-search-backward",      "The next search match in backward direction") \
	X(ka_text_object,                     TEXT_OBJECT_SEARCH_FORWARD,       .i = VIX_TEXTOBJECT_SEARCH_FORWARD,       "vix-textobject-search-forward",       "The next search match in forward direction") \
	X(ka_text_object,                     TEXT_OBJECT_SENTENCE,             .i = VIX_TEXTOBJECT_SENTENCE,             "vix-textobject-sentence",             "A sentence") \
	X(ka_text_object,                     TEXT_OBJECT_SINGLE_QUOTE_INNER,   .i = VIX_TEXTOBJECT_INNER_SINGLE_QUOTE,   "vix-textobject-single-quote-inner",   "A single quoted string, excluding the quotation marks") \
	X(ka_text_object,                     TEXT_OBJECT_SINGLE_QUOTE_OUTER,   .i = VIX_TEXTOBJECT_OUTER_SINGLE_QUOTE,   "vix-textobject-single-quote-outer",   "A single quoted string, including the quotation marks") \
	X(ka_text_object,                     TEXT_OBJECT_SQUARE_BRACKET_INNER, .i = VIX_TEXTOBJECT_INNER_SQUARE_BRACKET, "vix-textobject-square-bracket-inner", "[] block (inner variant)") \
	X(ka_text_object,                     TEXT_OBJECT_SQUARE_BRACKET_OUTER, .i = VIX_TEXTOBJECT_OUTER_SQUARE_BRACKET, "vix-textobject-square-bracket-outer", "[] block (outer variant)") \
	X(ka_text_object,                     TEXT_OBJECT_WORD_INNER,           .i = VIX_TEXTOBJECT_INNER_WORD,           "vix-textobject-word-inner",           "A word leading and trailing whitespace excluded") \
	X(ka_text_object,                     TEXT_OBJECT_WORD_OUTER,           .i = VIX_TEXTOBJECT_OUTER_WORD,           "vix-textobject-word-outer",           "A word leading and trailing whitespace included") \
	X(ka_undo,                            UNDO,                             0,                                        "vix-undo",                            "Undo last change") \
	X(ka_unicode_info,                    UNICODE_INFO,                     .i = VIX_ACTION_UNICODE_INFO,             "vix-unicode-info",                    "Show Unicode codepoint(s) of character under cursor") \
	X(ka_unicode_info,                    UTF8_INFO,                        .i = VIX_ACTION_UTF8_INFO,                "vix-utf8-info",                       "Show UTF-8 encoded codepoint(s) of character under cursor") \
	X(ka_visualmode_escape,               MODE_VISUAL_ESCAPE,               0,                                        "vix-mode-visual-escape",              "Reset count or switch to normal mode") \
	X(ka_window,                          WINDOW_REDRAW_BOTTOM,             .w = view_redraw_bottom,                  "vix-window-redraw-bottom",            "Redraw cursor line at the bottom of the window") \
	X(ka_window,                          WINDOW_REDRAW_CENTER,             .w = view_redraw_center,                  "vix-window-redraw-center",            "Redraw cursor line at the center of the window") \
	X(ka_window,                          WINDOW_REDRAW_TOP,                .w = view_redraw_top,                     "vix-window-redraw-top",               "Redraw cursor line at the top of the window") \
	X(ka_wscroll,                         WINDOW_HALFPAGE_DOWN,             .i = +PAGE_HALF,                          "vix-window-halfpage-down",            "Scroll window half pages forwards (downwards)") \
	X(ka_wscroll,                         WINDOW_HALFPAGE_UP,               .i = -PAGE_HALF,                          "vix-window-halfpage-up",              "Scroll window half pages backwards (upwards)") \
	X(ka_wscroll,                         WINDOW_PAGE_DOWN,                 .i = +PAGE,                               "vix-window-page-down",                "Scroll window pages forwards (downwards)") \
	X(ka_wscroll,                         WINDOW_PAGE_UP,                   .i = -PAGE,                               "vix-window-page-up",                  "Scroll window pages backwards (upwards)") \
	X(ka_wslide,                          WINDOW_SLIDE_DOWN,                .i = +1,                                  "vix-window-slide-down",               "Slide window content downwards") \
	X(ka_wslide,                          WINDOW_SLIDE_UP,                  .i = -1,                                  "vix-window-slide-up",                 "Slide window content upwards") \

#define ENUM(_, e, ...) VIX_ACTION_##e,
typedef enum { KEY_ACTION_LIST(ENUM) } VixActionKind;
#undef ENUM

/* NOTE: must conform to the vix library signature, but we can rename the parameters */
#undef KEY_ACTION_FN
#define KEY_ACTION_FN(name) const char *name(Vix *_unused, const char *keys, const Arg *arg)

/** key bindings functions */

static KEY_ACTION_FN(ka_nop)
{
	return keys;
}

static KEY_ACTION_FN(ka_layout)
{
	ui_arrange(&vix->ui, arg->i);
	vix_draw(vix);
	return keys;
}

static KEY_ACTION_FN(ka_tabview_toggle)
{
	vix->ui.tabview = !vix->ui.tabview;
	ui_arrange(&vix->ui, vix->ui.seltab->layout);
	vix_draw(vix);
	return keys;
}

static KEY_ACTION_FN(ka_tab_new)
{
	ui_tab_new(&vix->ui);
	return keys;
}

static KEY_ACTION_FN(ka_tab_next)
{
	ui_tab_next(&vix->ui);
	return keys;
}

static KEY_ACTION_FN(ka_tab_prev)
{
	ui_tab_prev(&vix->ui);
	return keys;
}

static KEY_ACTION_FN(ka_window_next_max)
{
	vix_window_next(vix);
	int n = 0, m = !!vix->ui.info[0];
	for (Win *w = vix->ui.seltab->windows; w; w = w->next) {
		if (!(w->options & UI_OPTION_ONELINE)) n++;
		else m++;
	}
	if (n > 1) {
		int total_dim = (vix->ui.seltab->layout == UI_LAYOUT_HORIZONTAL) ? (vix->ui.height - m) : (vix->ui.width - (n - 1));
		for (Win *w = vix->ui.seltab->windows; w; w = w->next) {
			if (w->options & UI_OPTION_ONELINE) continue;
			w->weight = (w == vix->win) ? MAX(1, total_dim - (n - 1)) * 100 : 100;
		}
		ui_arrange(&vix->ui, vix->ui.seltab->layout);
	}
	vix_draw(vix);
	return keys;
}

static KEY_ACTION_FN(ka_window_prev_max)
{
	vix_window_prev(vix);
	int n = 0, m = !!vix->ui.info[0];
	for (Win *w = vix->ui.seltab->windows; w; w = w->next) {
		if (!(w->options & UI_OPTION_ONELINE)) n++;
		else m++;
	}
	if (n > 1) {
		int total_dim = (vix->ui.seltab->layout == UI_LAYOUT_HORIZONTAL) ? (vix->ui.height - m) : (vix->ui.width - (n - 1));
		for (Win *w = vix->ui.seltab->windows; w; w = w->next) {
			if (w->options & UI_OPTION_ONELINE) continue;
			w->weight = (w == vix->win) ? MAX(1, total_dim - (n - 1)) * 100 : 100;
		}
		ui_arrange(&vix->ui, vix->ui.seltab->layout);
	}
	vix_draw(vix);
	return keys;
}

static KEY_ACTION_FN(ka_window_maximize)
{
	int n = 0, m = !!vix->ui.info[0];
	for (Win *w = vix->ui.seltab->windows; w; w = w->next) {
		if (!(w->options & UI_OPTION_ONELINE)) n++;
		else m++;
	}
	if (n > 1) {
		int total_dim = (vix->ui.seltab->layout == UI_LAYOUT_HORIZONTAL) ? (vix->ui.height - m) : (vix->ui.width - (n - 1));
		for (Win *w = vix->ui.seltab->windows; w; w = w->next) {
			if (w->options & UI_OPTION_ONELINE) continue;
			w->weight = (w == vix->win) ? MAX(1, total_dim - (n - 1)) * 100 : 100;
		}
		ui_arrange(&vix->ui, vix->ui.seltab->layout);
	}
	vix_draw(vix);
	return keys;
}

static KEY_ACTION_FN(ka_window_resize)
{
	if (!vix->win || !vix->windows || !vix->windows->next) {
		return keys;
	}
	int increment = arg->i;

	if (increment == 0) {
		for (Win *win = vix->windows; win; win = win->next) {
			win->weight = 100;
		}
		ui_arrange(&vix->ui, vix->ui.seltab->layout);
	} else {
		/* Geometric Weighting: calculate weights based on target pixel dimensions */
		int n = 0, m = !!vix->ui.info[0];
		for (Win *w = vix->ui.seltab->windows; w; w = w->next) {
			if (!(w->options & UI_OPTION_ONELINE)) n++;
			else m++;
		}
		if (n <= 1) return keys;

		int is_horiz = (vix->ui.seltab->layout == UI_LAYOUT_HORIZONTAL);
		int total_dim = is_horiz ? (vix->ui.height - m) : (vix->ui.width - (n - 1));
		
		int current_sizes[64];
		Win *win_ptr[64];
		int focus_idx = -1;
		int num_wins = 0;
		
		for (Win *w = vix->ui.seltab->windows; w; w = w->next) {
			if (w->options & UI_OPTION_ONELINE) continue;
			if (num_wins < 64) {
				win_ptr[num_wins] = w;
				current_sizes[num_wins] = is_horiz ? w->height : w->width;
				if (w == vix->win) focus_idx = num_wins;
				num_wins++;
			}
		}
		if (focus_idx == -1) return keys;

		int old_s = current_sizes[focus_idx];
		int new_s = old_s + (increment > 0 ? 1 : -1);
		int max_s = total_dim - (num_wins - 1);
		if (new_s < 1) new_s = 1;
		if (new_s > max_s) new_s = max_s;
		
		if (new_s != old_s) {
			int delta = new_s - old_s;
			current_sizes[focus_idx] = new_s;
			
			if (delta > 0) {
				/* Growing: take pixels from others */
				while (delta > 0) {
					bool changed = false;
					for (int j = 0; j < num_wins; j++) {
						if (j != focus_idx && current_sizes[j] > 1) {
							current_sizes[j]--;
							delta--;
							changed = true;
							if (delta == 0) break;
						}
					}
					if (!changed) break;
				}
			} else {
				/* Shrinking: give pixels to others */
				while (delta < 0) {
					for (int j = 0; j < num_wins; j++) {
						if (j != focus_idx) {
							current_sizes[j]++;
							delta++;
							if (delta == 0) break;
						}
					}
				}
			}

			for (int j = 0; j < num_wins; j++) {
				win_ptr[j]->weight = current_sizes[j] * 100;
			}
			ui_arrange(&vix->ui, vix->ui.seltab->layout);
		}
	}
	vix_draw(vix);
	return keys;
}

static KEY_ACTION_FN(ka_macro_record)
{
	if (!vix_macro_record_stop(vix)) {
		if (!keys[0]) {
			return NULL;
		}
		const char *next = vix_keys_next(vix, keys);
		if (next - keys > 1) {
			return next;
		}
		enum VixRegister reg = vix_register_from(vix, keys[0]);
		vix_macro_record(vix, reg);
		keys++;
	}
	vix_draw(vix);
	return keys;
}

static KEY_ACTION_FN(ka_macro_replay)
{
	if (!keys[0]) {
		return NULL;
	}
	const char *next = vix_keys_next(vix, keys);
	if (next - keys > 1) {
		return next;
	}
	enum VixRegister reg = vix_register_from(vix, keys[0]);
	vix_macro_replay(vix, reg);
	return keys+1;
}

static KEY_ACTION_FN(ka_suspend)
{
	ui_terminal_suspend(&vix->ui);
	return keys;
}

static KEY_ACTION_FN(ka_repeat)
{
	vix_repeat(vix);
	return keys;
}

static KEY_ACTION_FN(ka_selections_new)
{
	View *view = vix_view(vix);
	bool anchored = view_selections_primary_get(view)->anchored;
	VixCountIterator it = vix_count_iterator_get(vix, 1);
	while (vix_count_iterator_next(&it)) {
		Selection *sel = NULL;
		switch (arg->i) {
		case -1:
		case +1:
			sel = view_selections_primary_get(view);
			break;
		case INT_MIN:
			sel = view_selections(view);
			break;
		case INT_MAX:
			for (Selection *s = view_selections(view); s; s = view_selections_next(s)) {
				sel = s;
			}
			break;
		}

		if (!sel) {
			return keys;
		}

		size_t oldpos = view_cursors_pos(sel);
		if (arg->i > 0) {
			view_line_down(sel);
		} else if (arg->i < 0) {
			view_line_up(sel);
		}
		size_t newpos = view_cursors_pos(sel);
		view_cursors_to(sel, oldpos);
		Selection *sel_new = view_selections_new(view, newpos);
		if (!sel_new) {
			if (arg->i == -1) {
				sel_new = view_selections_prev(sel);
			} else if (arg->i == +1) {
				sel_new = view_selections_next(sel);
			}
		}
		if (sel_new) {
			view_selections_primary_set(sel_new);
			sel_new->anchored = anchored;
		}
	}
	vix->action.count = VIX_COUNT_UNKNOWN;
	return keys;
}

static KEY_ACTION_FN(ka_selections_align)
{
	View *view = vix_view(vix);
	Text *txt = vix_text(vix);
	int mincol = INT_MAX;
	for (Selection *s = view_selections(view); s; s = view_selections_next(s)) {
		size_t pos = view_cursors_pos(s);
		int col = text_line_width_get(txt, pos);
		if (col >= 0 && col < mincol) {
			mincol = col;
		}
	}
	for (Selection *s = view_selections(view); s; s = view_selections_next(s)) {
		size_t pos = view_cursors_pos(s);
		size_t newpos = text_line_width_set(txt, pos, mincol);
		view_cursors_to(s, newpos);
	}
	return keys;
}

static KEY_ACTION_FN(ka_selections_align_indent)
{
	View *view = vix_view(vix);
	Text *txt = vix_text(vix);
	bool left_align = arg->i < 0;
	int columns = view_selections_column_count(view);

	for (int i = 0; i < columns; i++) {
		int mincol = INT_MAX, maxcol = 0;
		for (Selection *s = view_selections_column(view, i); s; s = view_selections_column_next(s, i)) {
			Filerange sel = view_selections_get(s);
			size_t pos = left_align ? sel.start : sel.end;
			int col = text_line_width_get(txt, pos);
			if (col < mincol) {
				mincol = col;
			}
			if (col > maxcol) {
				maxcol = col;
			}
		}

		size_t len = maxcol - mincol;
		char *buf = malloc(len+1);
		if (!buf) {
			return keys;
		}
		memset(buf, ' ', len);

		for (Selection *s = view_selections_column(view, i); s; s = view_selections_column_next(s, i)) {
			Filerange sel = view_selections_get(s);
			size_t pos = left_align ? sel.start : sel.end;
			size_t ipos = sel.start;
			int col = text_line_width_get(txt, pos);
			if (col < maxcol) {
				size_t off = maxcol - col;
				if (off <= len) {
					text_insert(vix, txt, ipos, buf, off);
				}
			}
		}

		free(buf);
	}

	view_draw(view);
	return keys;
}

static KEY_ACTION_FN(ka_selections_clear)
{
	View *view = vix_view(vix);
	if (view->selection_count > 1) {
		view_selections_dispose_all(view);
	} else {
		view_selection_clear(view_selections_primary_get(view));
	}
	return keys;
}

static Selection *ka_selection_new(View *view, Filerange *r, bool isprimary) {
	Text *txt = view->text;
	size_t pos = text_char_prev(txt, r->end);
	Selection *s = view_selections_new(view, pos);
	if (!s) {
		return NULL;
	}
	view_selections_set(s, r);
	s->anchored = true;
	if (isprimary) {
		view_selections_primary_set(s);
	}
	return s;
}

static KEY_ACTION_FN(ka_selections_match_next)
{
	Text *txt = vix_text(vix);
	View *view = vix_view(vix);
	Selection *s = view_selections_primary_get(view);
	Filerange sel = view_selections_get(s);
	if (!text_range_valid(&sel)) {
		return keys;
	}

	static bool match_word;

	if (view->selection_count == 1) {
		Filerange word = text_object_word(txt, view_cursors_pos(s));
		match_word = text_range_equal(&sel, &word);
	}

	Filerange (*find_next)(Text *, size_t, const char *) = text_object_word_find_next;
	Filerange (*find_prev)(Text *, size_t, const char *) = text_object_word_find_prev;
	if (!match_word) {
		find_next = text_object_find_next;
		find_prev = text_object_find_prev;
	}

	char *buf = text_bytes_alloc0(txt, sel.start, text_range_size(&sel));
	if (!buf) {
		return keys;
	}

	bool match_all = arg->b;
	Filerange primary = sel;

	for (;;) {
		sel = find_next(txt, sel.end, buf);
		if (!text_range_valid(&sel)) {
			break;
		}
		if (ka_selection_new(view, &sel, !match_all) && !match_all) {
			goto out;
		}
	}

	sel = primary;

	for (;;) {
		sel = find_prev(txt, sel.start, buf);
		if (!text_range_valid(&sel)) {
			break;
		}
		if (ka_selection_new(view, &sel, !match_all) && !match_all) {
			break;
		}
	}

out:
	free(buf);
	return keys;
}

static KEY_ACTION_FN(ka_selections_match_skip)
{
	View *view = vix_view(vix);
	Selection *sel = view_selections_primary_get(view);
	keys = ka_selections_match_next(vix, keys, arg);
	if (sel != view_selections_primary_get(view)) {
		view_selections_dispose(sel);
	}
	return keys;
}

static KEY_ACTION_FN(ka_selections_remove)
{
	View *view = vix_view(vix);
	view_selections_dispose(view_selections_primary_get(view));
	view_cursors_to(view->selection, view_cursor_get(view));
	return keys;
}

static KEY_ACTION_FN(ka_selections_remove_column)
{
	View *view = vix_view(vix);
	int max = view_selections_column_count(view);
	int column = VIX_COUNT_DEFAULT(vix->action.count, arg->i) - 1;
	if (column >= max) {
		column = max - 1;
	}
	if (view->selection_count == 1) {
		vix_keys_feed(vix, "<Escape>");
		return keys;
	}

	for (Selection *s = view_selections_column(view, column), *next; s; s = next) {
		next = view_selections_column_next(s, column);
		view_selections_dispose(s);
	}

	vix->action.count = VIX_COUNT_UNKNOWN;
	return keys;
}

static KEY_ACTION_FN(ka_selections_remove_column_except)
{
	View *view = vix_view(vix);
	int max = view_selections_column_count(view);
	int column = VIX_COUNT_DEFAULT(vix->action.count, arg->i) - 1;
	if (column >= max) {
		column = max - 1;
	}
	if (view->selection_count == 1) {
		vix_redraw(vix);
		return keys;
	}

	Selection *sel = view_selections(view);
	Selection *col = view_selections_column(view, column);
	for (Selection *next; sel; sel = next) {
		next = view_selections_next(sel);
		if (sel == col) {
			col = view_selections_column_next(col, column);
		} else {
			view_selections_dispose(sel);
		}
	}

	vix->action.count = VIX_COUNT_UNKNOWN;
	return keys;
}

static KEY_ACTION_FN(ka_wscroll)
{
	View *view = vix_view(vix);
	int count = vix->action.count;
	switch (arg->i) {
	case -PAGE:
		view_scroll_page_up(view);
		break;
	case +PAGE:
		view_scroll_page_down(view);
		break;
	case -PAGE_HALF:
		view_scroll_halfpage_up(view);
		break;
	case +PAGE_HALF:
		view_scroll_halfpage_down(view);
		break;
	default:
		if (count == VIX_COUNT_UNKNOWN) {
			count = arg->i < 0 ? -arg->i : arg->i;
		}
		if (arg->i < 0) {
			view_scroll_up(view, count);
		} else {
			view_scroll_down(view, count);
		}
		break;
	}
	vix->action.count = VIX_COUNT_UNKNOWN;
	return keys;
}

static KEY_ACTION_FN(ka_wslide)
{
	View *view = vix_view(vix);
	int count = vix->action.count;
	if (count == VIX_COUNT_UNKNOWN) {
		count = arg->i < 0 ? -arg->i : arg->i;
	}
	if (arg->i >= 0) {
		view_slide_down(view, count);
	} else {
		view_slide_up(view, count);
	}
	vix->action.count = VIX_COUNT_UNKNOWN;
	return keys;
}

static KEY_ACTION_FN(ka_selections_navigate)
{
	View *view = vix_view(vix);
	if (view->selection_count == 1) {
		return ka_wscroll(vix, keys, arg);
	}
	Selection *s = view_selections_primary_get(view);
	VixCountIterator it = vix_count_iterator_get(vix, 1);
	while (vix_count_iterator_next(&it)) {
		if (arg->i > 0) {
			s = view_selections_next(s);
			if (!s) {
				s = view_selections(view);
			}
		} else {
			s = view_selections_prev(s);
			if (!s) {
				s = view_selections(view);
				for (Selection *n = s; n; n = view_selections_next(n)) {
					s = n;
				}
			}
		}
	}
	view_selections_primary_set(s);
	vix->action.count = VIX_COUNT_UNKNOWN;
	return keys;
}

static KEY_ACTION_FN(ka_selections_rotate)
{
	typedef struct {
		Selection *sel;
		char *data;
		size_t len;
	} Rotate;

	struct {
		Rotate     *data;
		VixDACount  count;
		VixDACount  capacity;
	} rotations[1] = {0};

	Text *txt = vix_text(vix);
	View *view = vix_view(vix);
	int columns = view_selections_column_count(view);
	int selections = columns == 1 ? view->selection_count : columns;
	int count = VIX_COUNT_DEFAULT(vix->action.count, 1);

	da_reserve(vix, rotations, selections);

	size_t line = 0;
	for (Selection *s = view_selections(view), *next; s; s = next) {
		next = view_selections_next(s);
		size_t line_next = 0;

		Filerange sel = view_selections_get(s);
		Rotate rot;
		rot.sel = s;
		rot.len = text_range_size(&sel);
		if ((rot.data = malloc(rot.len))) {
			rot.len = text_bytes_get(txt, sel.start, rot.len, rot.data);
		} else {
			rot.len = 0;
		}
		*da_push(vix, rotations) = rot;

		if (!line) {
			line = text_lineno_by_pos(vix, txt, view_cursors_pos(s));
		}
		if (next) {
			line_next = text_lineno_by_pos(vix, txt, view_cursors_pos(next));
		}
		if (!next || (columns > 1 && line != line_next)) {
			VixDACount len = rotations->count;
			size_t off = arg->i > 0 ? count % len : len - (count % len);
			for (VixDACount i = 0; i < rotations->count; i++) {
				VixDACount j = (i + off) % len;
				if (i == j) {
					continue;
				}
				Rotate *oldrot = rotations->data + i;
				Rotate *newrot = rotations->data + j;
				Filerange newsel = view_selections_get(newrot->sel);
				if (!text_range_valid(&newsel)) {
					continue;
				}
				if (!text_delete_range(txt, &newsel)) {
					continue;
				}
				if (!text_insert(vix, txt, newsel.start, oldrot->data, oldrot->len)) {
					continue;
				}
				newsel.end = newsel.start + oldrot->len;
				view_selections_set(newrot->sel, &newsel);
				free(oldrot->data);
			}
			rotations->count = 0;
		}
		line = line_next;
	}

	da_release(rotations);
	vix->action.count = VIX_COUNT_UNKNOWN;
	return keys;
}

static KEY_ACTION_FN(ka_selections_trim)
{
	Text *txt = vix_text(vix);
	View *view = vix_view(vix);
	for (Selection *s = view_selections(view), *next; s; s = next) {
		next = view_selections_next(s);
		Filerange sel = view_selections_get(s);
		if (!text_range_valid(&sel)) {
			continue;
		}
		for (char b; sel.start < sel.end && text_byte_get(txt, sel.end-1, &b)
			&& isspace((unsigned char)b); sel.end--);
		for (char b; sel.start <= sel.end && text_byte_get(txt, sel.start, &b)
			&& isspace((unsigned char)b); sel.start++);
		if (sel.start < sel.end) {
			view_selections_set(s, &sel);
		} else if (!view_selections_dispose(s)) {
			vix_mode_switch(vix, VIX_MODE_NORMAL);
		}
	}
	return keys;
}

static void selections_set(Vix *vix, View *view, FilerangeList sel)
{
	enum VixMode mode = vix->mode->id;
	bool anchored = mode == VIX_MODE_VISUAL || mode == VIX_MODE_VISUAL_LINE;
	view_selections_set_all(view, sel, anchored);
	if (!anchored) {
		view_selections_clear_all(view);
	}
}

static KEY_ACTION_FN(ka_selections_save)
{
	Win  *win  = vix->win;
	View *view = &win->view;
	enum VixMark mark = vix_mark_used(vix);
	FilerangeList sel = view_selections_get_all(vix, view);
	vix_mark_set(vix, win, mark, sel);
	da_release(&sel);
	vix_cancel(vix);
	return keys;
}

static KEY_ACTION_FN(ka_selections_restore)
{
	Win  *win  = vix->win;
	View *view = &win->view;
	enum VixMark mark = vix_mark_used(vix);
	FilerangeList sel = vix_mark_get(vix, win, mark);
	selections_set(vix, view, sel);
	da_release(&sel);
	vix_cancel(vix);
	return keys;
}

static KEY_ACTION_FN(ka_selections_union)
{
	Win  *win  = vix->win;
	View *view = &win->view;
	enum VixMark mark = vix_mark_used(vix);
	FilerangeList a   = vix_mark_get(vix, win, mark);
	FilerangeList b   = view_selections_get_all(vix, view);
	FilerangeList sel = {0};

	VixDACount i = 0, j = 0;
	Filerange *r1 = a.count > 0 ? a.data : 0, *r2 = b.count > 0 ? b.data : 0, cur = text_range_empty();
	while (r1 || r2) {
		if (r1 && text_range_overlap(r1, &cur)) {
			cur = text_range_union(r1, &cur);
			r1 = ++i < a.count ? a.data + i : 0;
		} else if (r2 && text_range_overlap(r2, &cur)) {
			cur = text_range_union(r2, &cur);
			r2 = ++j < b.count ? b.data + j : 0;
		} else {
			if (text_range_valid(&cur)) {
				*da_push(vix, &sel) = cur;
			}
			if (!r1) {
				cur = *r2;
				r2 = ++j < b.count ? b.data + j : 0;
			} else if (!r2) {
				cur = *r1;
				r1 = ++i < a.count ? a.data + i : 0;
			} else {
				if (r1->start < r2->start) {
					cur = *r1;
					r1 = ++i < a.count ? a.data + i : 0;
				} else {
					cur = *r2;
					r2 = ++j < b.count ? b.data + j : 0;
				}
			}
		}
	}

	if (text_range_valid(&cur)) {
		*da_push(vix, &sel) = cur;
	}

	selections_set(vix, view, sel);
	vix_cancel(vix);

	da_release(&a);
	da_release(&b);
	da_release(&sel);

	return keys;
}

static FilerangeList intersect(FilerangeList a, FilerangeList b)
{
	FilerangeList result = {0};
	for (VixDACount i = 0, j = 0; i < a.count && j < b.count;) {
		Filerange *r1 = a.data + i, *r2 = b.data + j;
		if (text_range_overlap(r1, r2)) {
			*da_push(vix, &result) = text_range_intersect(r1, r2);
		}
		if (r1->end < r2->end) {
			i++;
		} else {
			j++;
		}
	}
	return result;
}

static KEY_ACTION_FN(ka_selections_intersect)
{
	Win  *win  = vix->win;
	View *view = &win->view;
	enum VixMark mark = vix_mark_used(vix);
	FilerangeList a   = vix_mark_get(vix, win, mark);
	FilerangeList b   = view_selections_get_all(vix, view);

	FilerangeList sel = intersect(a, b);
	selections_set(vix, view, sel);
	vix_cancel(vix);

	da_release(&a);
	da_release(&b);
	da_release(&sel);

	return keys;
}

static FilerangeList complement(FilerangeList a, Filerange universe)
{
	FilerangeList result = {0};
	size_t pos = universe.start;
	for (VixDACount i = 0; i < a.count; i++) {
		Filerange r = a.data[i];
		if (pos < r.start) {
			*da_push(vix, &result) = text_range_new(pos, r.start);
		}
		pos = r.end;
	}
	if (pos < universe.end) {
		*da_push(vix, &result) = text_range_new(pos, universe.end);
	}

	return result;
}

static KEY_ACTION_FN(ka_selections_complement)
{
	Text *txt  = vix->win->file->text;
	View *view = &vix->win->view;

	FilerangeList a   = view_selections_get_all(vix, view);
	FilerangeList sel = complement(a, text_object_entire(txt, 0));

	selections_set(vix, view, sel);
	da_release(&a);
	da_release(&sel);

	return keys;
}

static KEY_ACTION_FN(ka_selections_minus)
{
	Win  *win  = vix->win;
	Text *txt  = win->file->text;
	View *view = &win->view;
	enum VixMark mark = vix_mark_used(vix);
	FilerangeList a   = view_selections_get_all(vix, view);
	FilerangeList b   = vix_mark_get(vix, win, mark);

	FilerangeList b_complement = complement(b, text_object_entire(txt, 0));
	FilerangeList sel          = intersect(a, b_complement);

	selections_set(vix, view, sel);
	vix_cancel(vix);

	da_release(&a);
	da_release(&b);
	da_release(&b_complement);
	da_release(&sel);

	return keys;
}

static KEY_ACTION_FN(ka_replace)
{
	if (!keys[0]) {
		vix_keymap_disable(vix);
		return NULL;
	}

	const char *next = vix_keys_next(vix, keys);
	if (!next) {
		return NULL;
	}

	char replacement[4+1];
	if (!vix_keys_utf8(vix, keys, replacement)) {
		return next;
	}

	if (replacement[0] == 0x1b) { /* <Escape> */
		return next;
	}

	vix_operator(vix, VIX_OP_REPLACE, replacement);
	if (vix->mode->id == VIX_MODE_OPERATOR_PENDING) {
		vix_motion(vix, VIX_MOVE_CHAR_NEXT);
	}
	return next;
}

static KEY_ACTION_FN(ka_count)
{
	int digit = keys[-1] - '0';
	int count = VIX_COUNT_DEFAULT(vix->action.count, 0);
	if (0 <= digit && digit <= 9) {
		if (digit == 0 && count == 0) {
			vix_motion(vix, VIX_MOVE_LINE_BEGIN);
		} else {
			vix->action.count = VIX_COUNT_NORMALIZE(count * 10 + digit);
		}
	}
	return keys;
}

static KEY_ACTION_FN(ka_gotoline)
{
	if (vix->action.count != VIX_COUNT_UNKNOWN) {
		vix_motion(vix, VIX_MOVE_LINE);
	} else if (arg->i < 0) {
		vix_motion(vix, VIX_MOVE_FILE_BEGIN);
	} else {
		vix_motion(vix, VIX_MOVE_FILE_END);
	}
	return keys;
}

static KEY_ACTION_FN(ka_operator)
{
	vix_operator(vix, arg->i);
	return keys;
}

static KEY_ACTION_FN(ka_movement_key)
{
	if (!keys[0]) {
		vix_keymap_disable(vix);
		return NULL;
	}

	const char *next = vix_keys_next(vix, keys);
	if (!next) {
		return NULL;
	}
	char utf8[4+1];
	if (vix_keys_utf8(vix, keys, utf8)) {
		vix_motion(vix, arg->i, utf8);
	}
	return next;
}

static KEY_ACTION_FN(ka_movement)
{
	vix_motion(vix, arg->i);
	return keys;
}

static KEY_ACTION_FN(ka_text_object)
{
	vix_textobject(vix, arg->i);
	return keys;
}

static KEY_ACTION_FN(ka_selection_end)
{
	for (Selection *s = view_selections(vix_view(vix)); s; s = view_selections_next(s)) {
		view_selections_flip(s);
	}
	return keys;
}

static KEY_ACTION_FN(ka_reg)
{
	if (!keys[0]) {
		return NULL;
	}
	const char *next = vix_keys_next(vix, keys);
	if (next - keys > 1) {
		return next;
	}
	enum VixRegister reg = vix_register_from(vix, keys[0]);
	vix_register(vix, reg);
	return keys+1;
}

static KEY_ACTION_FN(ka_mark)
{
	if (!keys[0]) {
		return NULL;
	}
	const char *next = vix_keys_next(vix, keys);
	if (next - keys > 1) {
		return next;
	}
	enum VixMark mark = vix_mark_from(vix, keys[0]);
	vix_mark(vix, mark);
	return keys+1;
}

static KEY_ACTION_FN(ka_undo)
{
	size_t pos = text_undo(vix_text(vix));
	if (pos != EPOS) {
		View *view = vix_view(vix);
		if (view->selection_count == 1) {
			view_cursors_to(view->selection, pos);
		}
		/* redraw all windows in case some display the same file */
		vix_draw(vix);
	}
	return keys;
}

static KEY_ACTION_FN(ka_redo)
{
	size_t pos = text_redo(vix_text(vix));
	if (pos != EPOS) {
		View *view = vix_view(vix);
		if (view->selection_count == 1) {
			view_cursors_to(view->selection, pos);
		}
		/* redraw all windows in case some display the same file */
		vix_draw(vix);
	}
	return keys;
}

static KEY_ACTION_FN(ka_earlier)
{
	size_t pos = EPOS;
	VixCountIterator it = vix_count_iterator_get(vix, 1);
	while (vix_count_iterator_next(&it)) {
		pos = text_earlier(vix_text(vix));
	}
	if (pos != EPOS) {
		view_cursors_to(vix_view(vix)->selection, pos);
		/* redraw all windows in case some display the same file */
		vix_draw(vix);
	}
	return keys;
}

static KEY_ACTION_FN(ka_later)
{
	size_t pos = EPOS;
	VixCountIterator it = vix_count_iterator_get(vix, 1);
	while (vix_count_iterator_next(&it)) {
		pos = text_later(vix_text(vix));
	}
	if (pos != EPOS) {
		view_cursors_to(vix_view(vix)->selection, pos);
		/* redraw all windows in case some display the same file */
		vix_draw(vix);
	}
	return keys;
}

static KEY_ACTION_FN(ka_delete)
{
	vix_operator(vix, VIX_OP_DELETE);
	vix_motion(vix, arg->i);
	return keys;
}

static KEY_ACTION_FN(ka_insert_register)
{
	if (!keys[0]) {
		return NULL;
	}
	const char *next = vix_keys_next(vix, keys);
	if (next - keys > 1) {
		return next;
	}
	enum VixRegister reg = vix_register_from(vix, keys[0]);
	if (reg != VIX_REG_INVALID) {
		vix_register(vix, reg);
		vix_operator(vix, VIX_OP_PUT_BEFORE_END);
	}
	return keys+1;
}

static KEY_ACTION_FN(ka_prompt_show)
{
	vix_prompt_show(vix, arg->s);
	return keys;
}

static KEY_ACTION_FN(ka_insert_verbatim)
{
	uint32_t rune = 0;
	unsigned char buf[4], type = keys[0];
	const char *data = NULL;
	int len = 0, count = 0, base = 0;
	switch (type) {
	case '\0':
		return NULL;
	case 'o':
	case 'O':
		count = 3;
		base = 8;
		break;
	case 'U':
		count = 4;
		/* fall through */
	case 'u':
		count += 4;
		base = 16;
		break;
	case 'x':
	case 'X':
		count = 2;
		base = 16;
		break;
	default:
		if ('0' <= type && type <= '9') {
			rune = type - '0';
			count = 2;
			base = 10;
		}
		break;
	}

	if (base) {
		for (keys++; keys[0] && count > 0; keys++, count--) {
			int v = 0;
			if (base == 8 && '0' <= keys[0] && keys[0] <= '7') {
				v = keys[0] - '0';
			} else if ((base == 10 || base == 16) && '0' <= keys[0] && keys[0] <= '9') {
				v = keys[0] - '0';
			} else if (base == 16 && 'a' <= keys[0] && keys[0] <= 'f') {
				v = 10 + keys[0] - 'a';
			} else if (base == 16 && 'A' <= keys[0] && keys[0] <= 'F') {
				v = 10 + keys[0] - 'A';
			} else {
				count = 0;
				break;
			}
			rune = rune * base + v;
		}

		if (count > 0) {
			return NULL;
		}
		if (type == 'u' || type == 'U') {
			len = utf8_encode(buf, rune);
		} else {
			buf[0] = rune;
			len = 1;
		}

		data = (char *)buf;
	} else {
		const char *next = vix_keys_next(vix, keys);
		if (!next) {
			return NULL;
		}
		if ((rune = vix_keys_codepoint(vix, keys)) != -1) {
			len = utf8_encode(buf, rune);
			if (buf[0] == '\n') {
				buf[0] = '\r';
			}
			data = (char *)buf;
		} else {
			vix_info_show(vix, "Unknown key");
		}
		keys = next;
	}

	if (len > 0) {
		vix_insert_key(vix, data, len);
	}
	return keys;
}

static KEY_ACTION_FN(ka_call)
{
	arg->f(vix);
	return keys;
}

static KEY_ACTION_FN(ka_window)
{
	arg->w(vix_view(vix));
	return keys;
}

static KEY_ACTION_FN(ka_openline)
{
	vix_operator(vix, VIX_OP_MODESWITCH, VIX_MODE_INSERT);
	if (arg->i > 0) {
		vix_motion(vix, VIX_MOVE_LINE_END);
		vix_keys_feed(vix, "<Enter>");
	} else {
		if (vix->autoindent) {
			vix_motion(vix, VIX_MOVE_LINE_START);
			vix_keys_feed(vix, "<vix-motion-line-start>");
		} else {
			vix_motion(vix, VIX_MOVE_LINE_BEGIN);
			vix_keys_feed(vix, "<vix-motion-line-begin>");
		}
		vix_keys_feed(vix, "<Enter><vix-motion-line-up>");
	}
	return keys;
}

static KEY_ACTION_FN(ka_join)
{
	bool normal = (vix->mode->id == VIX_MODE_NORMAL);
	vix_operator(vix, VIX_OP_JOIN, arg->s);
	if (normal) {
		vix->action.count = VIX_COUNT_DEFAULT(vix->action.count, 0);
		if (vix->action.count > 0) {
			vix->action.count -= 1;
		}
		vix_motion(vix, VIX_MOVE_LINE_NEXT);
	}
	return keys;
}

static KEY_ACTION_FN(ka_normalmode_escape)
{
	if (vix->action.count == VIX_COUNT_UNKNOWN) {
		ka_selections_clear(vix, keys, arg);
	} else {
		vix->action.count = VIX_COUNT_UNKNOWN;
	}
	return keys;
}

static KEY_ACTION_FN(ka_visualmode_escape)
{
	if (vix->action.count == VIX_COUNT_UNKNOWN) {
		vix_mode_switch(vix, VIX_MODE_NORMAL);
	} else {
		vix->action.count = VIX_COUNT_UNKNOWN;
	}
	return keys;
}

static KEY_ACTION_FN(ka_switchmode)
{
	vix_mode_switch(vix, arg->i);
	return keys;
}

static KEY_ACTION_FN(ka_insertmode)
{
	vix_operator(vix, VIX_OP_MODESWITCH, VIX_MODE_INSERT);
	vix_motion(vix, arg->i);
	return keys;
}

static KEY_ACTION_FN(ka_replacemode)
{
	vix_operator(vix, VIX_OP_MODESWITCH, VIX_MODE_REPLACE);
	vix_motion(vix, arg->i);
	return keys;
}

static KEY_ACTION_FN(ka_unicode_info)
{
	View *view = vix_view(vix);
	Text *txt = vix_text(vix);
	size_t start = view_cursor_get(view);
	size_t end = text_char_next(txt, start);
	char *grapheme = text_bytes_alloc0(txt, start, end-start), *codepoint = grapheme;
	if (!grapheme) {
		return keys;
	}
	Buffer info = {0};
	mbstate_t ps = {0};
	Iterator it = text_iterator_get(txt, start);
	for (size_t pos = start; it.pos < end; pos = it.pos) {
		if (!text_iterator_codepoint_next(&it, NULL)) {
			vix_info_show(vix, "Failed to parse code point");
			goto err;
		}
		size_t len = it.pos - pos;
		wchar_t wc = 0xFFFD;
		size_t res = mbrtowc(&wc, codepoint, len, &ps);
		bool combining = false;
		if (res != (size_t)-1 && res != (size_t)-2) {
			combining = (wc != L'\0' && wcwidth(wc) == 0);
		}
		unsigned char ch = *codepoint;
		if (ch < 128 && !isprint(ch)) {
			buffer_appendf(&info, "<^%c> ", ch == 127 ? '?' : ch + 64);
		} else {
			buffer_appendf(&info, "<%s%.*s> ", combining ? " " : "", (int)len, codepoint);
		}
		if (arg->i == VIX_ACTION_UNICODE_INFO) {
			buffer_appendf(&info, "U+%04"PRIX32" ", (uint32_t)wc);
		} else {
			for (size_t i = 0; i < len; i++) {
				buffer_appendf(&info, "%02x ", (uint8_t)codepoint[i]);
			}
		}
		codepoint += len;
	}
	vix_info_show(vix, "%s", buffer_content0(&info));
err:
	free(grapheme);
	buffer_release(&info);
	return keys;
}

static KEY_ACTION_FN(ka_percent)
{
	if (vix->action.count == VIX_COUNT_UNKNOWN) {
		vix_motion(vix, VIX_MOVE_BRACKET_MATCH);
	} else {
		vix_motion(vix, VIX_MOVE_PERCENT);
	}
	return keys;
}

static KEY_ACTION_FN(ka_jumplist)
{
	vix_jumplist(vix, arg->i);
	return keys;
}

static void signal_handler(int signum, siginfo_t *siginfo, void *context) {
	vix_signal_handler(vix, signum, siginfo, context);
}

#define KEY_ACTION_STRUCT(impl, _, arg, lua_name, help) {lua_name, VIX_HELP(help) impl, {arg}},
static const KeyAction vix_action[] = { KEY_ACTION_LIST(KEY_ACTION_STRUCT) };
#undef KEY_ACTION_STRUCT

#include "config.h"

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			continue;
		} else if (strcmp(argv[i], "-") == 0) {
			continue;
		} else if (strcmp(argv[i], "--") == 0) {
			break;
		} else if (strcmp(argv[i], "-v") == 0) {
			printf("vix %s%s%s%s%s%s%s\n", VERSION,
			       CONFIG_CURSES  ? " +curses"  : "",
			       CONFIG_LUA     ? " +lua"     : "",
			       CONFIG_LPEG    ? " +lpeg"    : "",
			       CONFIG_TRE     ? " +tre"     : "",
			       CONFIG_ACL     ? " +acl"     : "",
			       CONFIG_SELINUX ? " +selinux" : "");
			return 0;
		} else if (strcmp(argv[i], "-headless") == 0) {
			continue;
		} else {
			fprintf(stderr, "Unknown command option: %s\n", argv[i]);
			return 1;
		}
	}

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-headless") == 0) {
			vix->headless = true;
		}
	}

	if (!vix_init(vix)) {
		return EXIT_FAILURE;
	}

	/* install signal handlers etc.
	 * Do it before any external lua code is run by EVENT_INIT to prevent lost
	 * signals.
	 */
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = signal_handler;
	if (sigaction(SIGBUS, &sa, NULL) == -1 ||
	    sigaction(SIGINT, &sa, NULL) == -1 ||
	    sigaction(SIGCONT, &sa, NULL) == -1 ||
	    sigaction(SIGWINCH, &sa, NULL) == -1 ||
	    sigaction(SIGTERM, &sa, NULL) == -1 ||
	    sigaction(SIGHUP, &sa, NULL) == -1) {
		vix_die(vix, "Failed to set signal handler: %s\n", strerror(errno));
	}

	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL) == -1 || sigaction(SIGQUIT, &sa, NULL) == -1) {
		vix_die(vix, "Failed to ignore signals\n");
	}

	sigset_t blockset;
	sigemptyset(&blockset);
	sigaddset(&blockset, SIGBUS);
	sigaddset(&blockset, SIGCONT);
	sigaddset(&blockset, SIGWINCH);
	sigaddset(&blockset, SIGTERM);
	sigaddset(&blockset, SIGHUP);
	if (sigprocmask(SIG_BLOCK, &blockset, NULL) == -1) {
		vix_die(vix, "Failed to block signals\n");
	}

	vix_event_emit(vix, VIX_EVENT_INIT);

	for (int i = 0; i < LENGTH(vix_action); i++) {
		if (!vix_action_register(vix, vix_action + i)) {
			vix_die(vix, "Could not register action: %s\n", vix_action[i].name);
		}
	}

	for (int i = 0; i < LENGTH(default_bindings); i++) {
		for (const KeyBinding **binding = default_bindings[i]; binding && *binding; binding++) {
			for (const KeyBinding *kb = *binding; kb->key; kb++) {
				vix_mode_map(vix, i, false, kb->key, kb);
			}
		}
	}

	for (const char **k = keymaps; k[0]; k += 2) {
		vix_keymap_add(vix, k[0], k[1]);
	}

	bool end_of_options = false, win_created = false;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && !end_of_options) {
			if (strcmp(argv[i], "-") == 0) {
				if (!vix_window_new_fd(vix, STDOUT_FILENO)) {
					vix_die(vix, "Can not create empty buffer\n");
				}
				ssize_t len = 0;
				char buf[PIPE_BUF];
				Text *txt = vix_text(vix);
				while ((len = read(STDIN_FILENO, buf, sizeof buf)) > 0) {
					text_insert(vix, txt, text_size(txt), buf, len);
				}
				if (len == -1) {
					vix_die(vix, "Can not read from stdin\n");
				}
				text_snapshot(txt);
				int fd = open("/dev/tty", O_RDWR);
				if (fd == -1) {
					vix_die(vix, "Can not reopen stdin\n");
				}
				dup2(fd, STDIN_FILENO);
				close(fd);
			} else if (strcmp(argv[i], "--") == 0) {
				end_of_options = true;
				continue;
			}
		} else if (argv[i][0] == '+' && !end_of_options) {
			vix_prompt_cmd(vix, argv[i] + (argv[i][1] == '/' || argv[i][1] == '?'));
			continue;
		} else if (!vix_window_new(vix, argv[i])) {
			vix_die(vix, "Can not load '%s': %s\n", argv[i], strerror(errno));
		}
		win_created = true;
	}

	if (!vix->win && !win_created) {
		if (!vix_window_new(vix, NULL)) {
			vix_die(vix, "Can not create empty buffer\n");
		}
	}

	int status = EXIT_SUCCESS;
	if (vix->running) {
		status = vix_run(vix);
	}
	vix_cleanup(vix);
	return status;
}
