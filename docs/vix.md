VIX(1) - General Commands Manual

# NAME

**vix** - a highly efficient text editor

# SYNOPSIS

**vix**
\[**-v**]
\[**-headless**]
\[**+**&zwnj;*command*]
\[**--**]
\[*files&nbsp;...*]

# DESCRIPTION

**vix**
is a highly efficient screen-oriented text editor combining the strengths of
both
**vi(m)**
and
**sam**.
This manual page is intended for users already familiar with
**vi**/**sam**.
Anyone else should almost certainly read a good tutorial on either editor
before this manual page.
The following options are available:

**-v**

> Print version information and exit.

**-headless**

> Run in non-interactive mode. Useful for scripting and automated testing.
> No UI is initialized.

**+**&zwnj;*command*

> Execute
> *command*
> after loading file.

**--**

> Denotes the end of the options.
> Arguments after this will be handled as a
> file name.

The special file
**-**
instructs
**vix**
to read from standard input in which case
**:wq**
will write to standard output, thereby enabling usage as an interactive filter.

If standard input is redirected and all input is consumed,
**vix**
will open
*/dev/tty*
to gather further commands.
Failure to do so results in program termination.

## Selections

**vix**
uses selections as core editing primitives.
A selection is a non-empty, directed range with two endpoints called
*cursor*
and
*anchor*.
A selection can be anchored in which case the anchor remains fixed while
only the position of the cursor is adjusted.
For non-anchored selections both endpoints are updated.
A singleton selection covers one character on which both cursor and
anchor reside.
There always exists a primary selection which remains vixible
(i.e. changes to its position will adjust the viewport).

## Modes

**vix**
employs the same
*modal*
editing approach as
**vi**.
It supports a
'normal',
'operator pending',
'insert',
'replace',
'window'
and
'visual'
(in both line and character wise variants) mode.
The visual block and ex modes are deliberately not implemented,
instead
**vix**
has built in support for multiple selections and an
*interactive*
variant of the structural regular expression based command language of
**sam**.

In normal mode all selections are non-anchored and reduced to a single character.

## Undo/Redo

**vix**
uses an undo tree to keep track of text revixions.
The
**u**
(undo)
and
&lt;**C-r**&gt;
(redo)
commands can be used to traverse the tree along the main branch.
**g+**
and
**g-**
traverse the history in chronological order.
The
**:earlier**
and
**:later**
commands provide means to restore the text to an arbitrary state.

## Marks

A mark associates a symbolic name to a set of selections.
A stored selection becomes invalid when its delimiting boundaries change
in the underlying buffer.
If said changes are later undone the mark becomes valid again.
**m**
sets a mark,
**M**
restores it.
For example,
**'**&zwnj;*a*&zwnj;**m**
sets the mark
*a*
while
**'**&zwnj;*a*&zwnj;**M**
restores it.
Use of
**m**
or
**M**
without specifying a mark uses the default mark.

Available marks are:

**''**

> default mark

**'^**

> active selections when leaving visual mode

**'a**&#8211;**'z**

> general purpose marks

No marks across files are supported.
Marks are not preserved over editing sessions.

## Jump list

A per window, fixed sized file local jump list exists which stores marks
(i.e. set of selections).

**g&lt;**

> jump backward

**g&gt;**

> jump forward

**gs**

> save currently active selections

## Tab Pages

**vix**
supports multiple tab pages, each containing its own window layout.

**gt**

> go to the next tab page

**gT**

> go to the previous tab page

## Registers

Registers are named lists of text.
Uninitialized register slots default to the empty string.
Available registers are:

**&#34;&#34;**

> default register

**&#34;a**&#8211;**&#34;z**

> general purpose registers

**&#34;A**&#8211;**&#34;Z**

> append to corresponding general purpose register

**&#34;\*&zwnj;**, **&#34;+**

> system clipboard integration via shell script
> vix-clipboard(1)

**&#34;0**

> yank register, most recently yanked range

**&#34;1**&#8211;**&#34;9**

**&#34;&**

> sub expression matches of most recent
> **x**
> or
> **y**
> command

**&#34;/**

> search register, most recently used search pattern

**&#34;:**

> command register, most recently executed command

**&#34;\_**

> black hole
> (*/dev/null*)
> register, ignore content is always empty

**&#34;#**

> selection number (readonly)

If no explicit register is specified the default register is used.

## Macros

The general purpose registers
**&#34;a**&#8211;**&#34;z**
can be used to record macros.
Use one of
**&#34;A**&#8211;**&#34;Z**
to append to an existing macro.
**q**
starts a recording,
**@**
plays it back.
**@@**
refers to the most recently recorded macro.
**@:**
repeats the last
**:**-command.
**@/**
is equivalent to
**n**
in normal mode.
These operations always use the first register slot.

## Encoding, Tab and Newline handling

**vix**
always assumes the input file to be UTF-8 encoded with
`\n`
line endings.
If you wish to edit files with legacy encodings or non-Unix line endings,
use
iconv(1)
and
dos2unix(1)
to convert them as needed.
&lt;**Tab**&gt;
can optionally be expanded to a configurable number of spaces (see
*SET OPTIONS*).

## Mouse support

The mouse is currently not used at all.

# SAM COMMANDS

**vix**
supports an interactive variant of the structural regular expression based
command language introduced by
sam(1).

## Regular expressions

**vix**
currently defers regular expression matching to the underlying C library.
It uses what POSIX refers to as
"Extended Regular Expressions"
as described in
regex(7).
The anchors
**^**
and
**$**
match the beginning / end of the range they are applied to.
Additionally
`\n`
and
`\t`
may be used to refer to newlines and tabs,
respectively.
The
**.**
atom matches any character except newline.
The empty regular expression stands for the last complete expression
encountered.

## Addresses

An address identifies a substring (or range) in a file.
In the following
"character
*n*"
means the null string after the
*n*-th
character in the file, with 1 the first character in the file.
"Line
*n*"
means the
*n*-th
match, starting at the beginning of the file, of the regular expression
"`.*\n?`".

All windows always have at least one current substring which is the default
address.
In sam this is referred to as
**dot**.
In
**vix**
multiple
"dots"
(or selections) can exist at the same time.

## Simple addresses

**#**&zwnj;*n*

> The empty string after character
> *n*;
> **#0**
> is the beginning of the file.

*n*

> Line
> *n*.

**/**&zwnj;*regexp*&zwnj;**/**

**?**&zwnj;*regexp*&zwnj;**?**

> The substring that matches the regular expression, found by looking
> towards the end
> (**/**)
> or beginning
> (**?**)
> of the file.
> The search does not wrap around when hitting the end
> (start)
> of the file.

**0**

> The string before the first full line.
> This is not necessarily the null string; see
> **+**
> and
> **-**
> below.

**$**

> The null string at the end of the file.

**.**

> Dot, the current range.

**'**&zwnj;*m*

> The mark
> *m*
> in the file.

## Compound addresses

In the following,
*a1*
and
*a2*
are addresses.

*a1*&zwnj;**+**&zwnj;*a2*

> The address
> *a2*
> evaluated starting at the end of
> *a1*.

*a1*&zwnj;**-**&zwnj;*a2*

> The address
> *a2*
> evaluated looking the reverse direction starting at the beginning of
> *a1*.

*a1*&zwnj;**,**&zwnj;*a2*

> The substring from the beginning of
> *a1*
> to the end of
> *a2*.
> If
> *a1*
> is missing,
> **0**
> is substituted.
> If
> *a2*
> is missing,
> **$**
> is substituted.

*a1*&zwnj;**;**&zwnj;*a2*

> Like
> *a1*&zwnj;**,**&zwnj;*a2*
> but with
> *a2*
> evaluated at the end of, and range set to,
> *a1*.

The operators
**+**
and
**-**
are high precedence, while
**,**
and
**;**
are low precedence.

In both
**+**
and
**-**
forms, if
*a2*
is a line or character address with a missing number, the number defaults to 1.
If
*a1*
is missing,
**.**
is substituted.
If both
*a1*
and
*a2*
are present and distinguishable,
**+**
may be elided.
*a2*
may be a regular expression; if it is delimited by
`?`
characters, the effect of the
**+**
or
**-**
is reversed.
The
**%**
sign is an alias for
**,**
and hence
**0,$**.
It is an error for a compound address to represent a malformed substring.

## Commands

In the following, text demarcated by slashes represents text delimited
by any printable ASCII character except alphanumerics.
Any number of trailing delimiters may be elided, with multiple elisions then
representing null strings, but the first delimiter must always be present.
In any delimited text, newline may not appear literally;
`\n`
and
`\t`
may be typed for newline and tab;
`\/`
quotes the delimiter, here
`/`.
An ampersand
`&`
and
`\`*n*,
where
*n*
is a digit (1&#8211;9) are replaced by the corresponding register.
Backslash is otherwise interpreted literally.

Most commands may be prefixed with an address to indicate their range of
operation.
If a command takes an address and none is supplied, a default address is used.
In normal mode this equates to the character the selection is currently over.
If only one selection exists
**x**
and
**y**
default to the whole file
**0,$**.
In normal mode the write commands
**w**
and
**wq**
always apply to the whole file.
Commands are executed once for every selection.
In visual mode the commands are applied to every selection
as if an implicit
**x**
command, matching the existing selections, was present.

In the description,
"range"
is used to represent whatever address is supplied.

Many commands create new selections as a side effect when issued from a visual
mode.
If so, it is always to the &#8220;result&#8221; of the change: the new text for an
insertion, the empty string for a deletion, the command output of a filter etc.
If after a successful command execution no selections remain,
the editor will switch to normal mode, otherwise it remains in
visual mode.
This allows
*interactive*
refinements of ranges.

## Text commands

**a**\[*count*]/*text*&zwnj;**/**

> Insert the
> *text*
> *count*
> times into the file after the range.

> May also be written as

> >  a
> >  lines
> >  of
> >  text
> >  .

**c** or **i**

> Same as
> **a**,
> but
> **c**
> replaces the text, while
> **i**
> inserts
> *before*
> the range.

**d**

> Delete the text in range.

## Display commands

**p**

> Create a new selection for the range.

## I/O commands

**e**\[**!**] \[*file name*]

> Replace the file by the contents of the named external file.
> If no file name is given, reload file from disk.

**r** *file name*

> Replace the text in the range by the contents of the named external file.

**w**\[**!**] \[*file name*]

> Write the range
> (default
> **0,$**)
> to the named external file.

**wq**\[**!**] \[*file name*]

> Same as
> **w**,
> but close file afterwards.

If the file name argument is absent from any of these, the current file name is
used.
**e**
always sets the file name,
**w**
will do so if the file has no name.
Forcing the
**e**
command with
**!**
will discard any unsaved changes.
Forcing
**w**
will overwrite the file on disk even if it has been externally modified
since loading it.
Write commands with a non-default addresses and no file name are destructive
and need always to be forced.

**&lt;** *shell command*

> Replace the range by the standard output of the shell command.

**&gt;** *shell command*

> Sends the range to the standard input of the shell command.

**|** *shell command*

> Send the range to the standard input, and replace it by the standard output, of
> the shell command.

**!** *shell command*

> Run interactive shell command, redirect keyboard input to it.

**cd** *directory*

> Change working directory.
> If no directory is specified,
> `$HOME`
> is used.

In any of
**&lt;**,
**&gt;**,
**|**,
or
**!**,
if the shell command is omitted, the last shell command
(of any type)
is substituted.
Unless the file being edited is unnamed, all these external commands
can refer to its absolute path and file name through the
`vix_filepath`
and
`vix_filename`
environment variables.

## Loops and conditionals

**x/**&zwnj;*regexp*&zwnj;**/** \[*command*]

> For each match of the regular expression in the range, run the command with
> range set to the match.
> If the regular expression and its slashes are omitted,
> */.\*&#92;n/*
> is assumed.
> Null string matches potentially occur before every character of the range and
> at the end of the range.

> The
> **&#34;1**&#8211;**&#34;9**
> and
> **&#34;&**
> registers are updated with the (sub) expression matches of the pattern.

**y/**&zwnj;*regexp*&zwnj;**/** \[*command*]

> Like
> **x**,
> but run the command for each substring that lies before, between, or after the
> matches that would be generated by
> **x**.
> There is no default behavior.
> Null substrings potentially occur before every character in the range.

**X/**&zwnj;*regexp*&zwnj;**/** *command*

> For each file whose file name matches the regular expression, make that the
> current file and run the command.
> If the expression is omitted, the command is run in every file.

**Y/**&zwnj;*regexp*&zwnj;**/** *command*

> Same as
> **X**,
> but for files that do not match the regular expression, and the expression is
> required.

**g**\[*count*]\[*/regexp/*] *command*

**v**\[*count*]\[*/regexp/*] *command*

> If the
> *count*
> range contains
> (**g**)
> or does not contain
> (**v**)
> a match for the expression, run command on the range.

> The
> *count*
> specifier has the following format, where
> *n*
> and
> *m*
> are integers denoting the ranges.

> *n*&zwnj;**,**&zwnj;*m*

> > The closed interval from
> > *n*
> > to
> > *m*.
> > If
> > *n*
> > is missing,
> > **1**
> > is substituted.
> > If
> > *m*
> > is missing,
> > **&#8734;**
> > is substituted.
> > Negative values are interpreted relative to the last range.

> **%**&zwnj;*n*

> > Matches every
> > *n*-th
> > range.

These may be nested arbitrarily deeply.
An empty command in an
**x**
or
**y**
defaults to
**p**.
**X**,
**Y**,
**g**
and
**v**
do not have defaults.

## Grouping and multiple changes

Commands may be grouped by enclosing them in curly braces.
Semantically, the opening brace is like a command: it takes an
(optional)
address and runs each sub-command on the range.
Commands within the braces are executed sequentially, but changes
made by one command are not vixible to other commands.

When a command makes a number of changes to a file, as in
**x/**&zwnj;*re*&zwnj;**/** **c/**&zwnj;*text*&zwnj;**/**,
the addresses of all changes are computed based on the initial state.
If the changes are non-overlapping, they are applied in the specified
order.
Conflicting changes are rejected.

Braces may be nested arbitrarily.

# VI(M) KEY BINDINGS

In the following sections angle brackets are used to denote special keys.
The prefixes
**C-**,
**S-**,
and
**M-**
are used to refer to the
&lt;Ctrl&gt;,
&lt;Shift&gt;
and
&lt;Alt&gt;
modifiers, respectively.

All active key bindings can be listed at runtime using the
**:help**
command.

## Operators

Operators perform a certain operation on a text range indicated by either a
motion, a text object or an existing selection.

When used in normal mode, the following operators wait for a motion, putting
vix into operator pending mode.

**c**

> change, delete range and enter insert mode

**d**

> delete, cut range to register

**&lt;**

> shift-left, decrease indent

**&gt;**

> shift-right, increase indent

**y**

> yank, copy range to register

When used in normal mode, the following actions take effect immediately.

**=**

> format, filter range through
> fmt(1)

**gu**

> make lowercase

**gU**

> make uppercase

**g~**

> swap case

**J**

> join lines, insert spaces in between

**gJ**

> join lines remove any delimiting white spaces

**p**

> put register content after cursor

**P**

> put register content before cursor

## Motions

Motions take an initial file position and transform it to a destination file
position,
thereby defining a range.

**0**

> start of line

**b**

> previous start of a word

**B**

> previous start of a WORD

**$**

> end of line

**e**

> next end of a word

**E**

> next end of a WORD

**F**&lt;*char*&gt;

> to next occurrence of
> &lt;*char*&gt;
> to the left

**f**&lt;*char*&gt;

> to next occurrence of
> &lt;*char*&gt;
> to the right

**^**

> first non-blank of line

**g0**

> begin of display line

**g$**

> end of display line

**ge**

> previous end of a word

**gE**

> previous end of a WORD

**gg**

> begin of file

**G**

> goto line or end of file

**gj**

> display line down

**gk**

> display line up

**gh**

> codepoint left

**gl**

> codepoint right

**gH**

> byte left

**gL**

> byte right

**g\_**

> last non-blank of line

**gm**

> middle of display line

**gt**

> next tab page

**gT**

> previous tab page

**g|**

> goto column

**h**

> char left

**H**

> goto top/home line of window

**j**

> line down

**k**

> line up

**l**

> char right

**L**

> goto bottom/last line of window

**%**

> match bracket, quote or backtick

**}**

> next paragraph

**)**

> next sentence

**N**

> repeat last search backwards

**n**

> repeat last search forward

**\[{**

> previous start of block

**]}**

> next start of block

**\[(**

> previous start of parentheses pair

**])**

> next start of parentheses pair

**{**

> previous paragraph

**(**

> previous sentence

**;**

> repeat last to/till movement

**,**

> repeat last to/till movement but in opposite direction

**#**

> search word under selection backwards

**\*&zwnj;**

> search word under selection forwards

**T**&lt;*char*&gt;

> till before next occurrence of
> &lt;*char*&gt;
> to the left

**t**&lt;*char*&gt;

> till before next occurrence of
> &lt;*char*&gt;
> to the right

**?**&zwnj;*pattern*

> to next match of
> *pattern*
> in backward direction

**/**&zwnj;*pattern*

> to next match of
> *pattern*
> in forward direction

**w**

> next start of a word

**W**

> next start of a WORD

## Text objects

Text objects take an initial file position and transform it to a range
where the former does not necessarily have to be contained in the latter.
All of the following text objects are implemented in an inner variant
(prefixed with
**i**)
where the surrounding white space or delimiting characters are not part
of the resulting range and a normal variant (prefixed with
**a**)
where they are.

**w**

> word

**W**

> WORD

**s**

> sentence

**p**

> paragraph

**\[, ], (, ), {, }, &lt;, &gt;, &#34;, ', \`**

> block enclosed by these symbols

Further available text objects include:

**gn**

> matches the last used search term in forward direction

**gN**

> matches the last used search term in backward direction

**al**

> current line

**il**

> current line without leading and trailing white spaces

**i**&lt;**Tab**&gt;

> inner indentation level

**a**&lt;**Tab**&gt;

> indentation level with surrounding lines

**ii**

> inner lexer token or item

**ai**

> outer lexer token or item

## Multiple Selections

**vix**
supports multiple selections with immediate visual feedback.
There always exists one primary selection located within the current
view port.
Additional selections can be created as needed.
If more than one selection exists, the primary one is styled differently.

To manipulate selections use in normal mode:

&lt;**C-k**&gt;

> create count new selections on the lines above

&lt;**C-M-k**&gt;

> create count new selections on the lines above the first selection

&lt;**C-j**&gt;

> create count new selections on the lines below

&lt;**C-M-j**&gt;

> create count new selections on the lines below the last selection

&lt;**C-p**&gt;

> remove primary selection

&lt;**C-n**&gt;

> select word the selection is currently over, switch to visual mode

&lt;**C-u**&gt;

> make the count previous selection primary

&lt;**C-d**&gt;

> make the count next selection primary

&lt;**C-c**&gt;

> remove the count selection column

&lt;**C-l**&gt;

> remove all but the count selection column

&lt;**Tab**&gt;

> try to align all selections on the same column

&lt;**Escape**&gt;

> dispose all but the primary selection

The visual modes were enhanced to recognize:

**I**

> create a selection at the start of every selected line

**A**

> create a selection at the end of every selected line

&lt;**Tab**&gt;

> left align selections by inserting spaces

&lt;**S-Tab**&gt;

> right align selections by inserting spaces

&lt;**C-a**&gt;

> create new selections everywhere matching current word or selection

&lt;**C-n**&gt;

> create new selection and select next word matching current selection

&lt;**C-x**&gt;

> clear (skip) current selection, but select next matching word

&lt;**C-p**&gt;

> remove primary selection

&lt;**C-u**&gt; &lt;**C-k**&gt;

> make the count previous selection primary

&lt;**C-d**&gt; &lt;**C-j**&gt;

> make the count next selection primary

&lt;**C-c**&gt;

> remove the count selection column

&lt;**C-l**&gt;

> remove all but the count selection column

**+**

> rotate selections rightwards count times

**-**

> rotate selections leftwards count times

**\_**

> trim selections, remove leading and trailing white space

**o**

> flip selection direction, swap cursor and anchor

&lt;**Escape**&gt;

> clear all selections, switch to normal mode

In insert and replace mode:

&lt;**S-Tab**&gt;

> align all selections by inserting spaces

Selections can be manipulated using set operations.
The first operand is the currently active selections while the second
can be specified as a mark.
For example,
**'**&zwnj;*a*&zwnj;**|**
produces the union of the mark
*a*
and the current selection.
Use of set operations without specifying a mark use the default mark as
the first operand.

**|**

> set union

**&**

> set intersection

**&#92;**

> set minus

**~**

> set complement

## Window Mode

Entering window mode via
&lt;**C-w**&gt;
allows for persistent window management using single key commands.
The following keys are recognized:

**+**, **&gt;**

> increase focused window size

**-**, **&lt;**

> decrease focused window size

**=**

> reset all windows to equal size

**h**, **k**

> focus previous window

**j**, **l**

> focus next window

**s**

> switch global layout to horizontal (rows)

**v**

> switch global layout to vertical (columns)

**S**

> split focused window horizontally

**V**

> split focused window vertically

**n**

> open a new empty window

**c**

> close focused window

**o**

> close all other windows

**t**

> toggle tabview mode (maximized window viewing)

**T**

> move current window to a new tab page

**w**, **x**

> focus next window

&lt;**Escape**, **q**&gt;

> return to normal mode

# VI(M) COMMANDS

Any unique prefix can be used to abbreviate a command.

## File and Window management

A file must be opened in at least one window.
If the last window displaying a certain file is closed all unsaved changes are
discarded.
Windows can be displayed in rows (horizontally) or in columns (vertically).
The orientation is a global setting.
Windows can be proportionally resized.
The
&lt;**C-w**&gt;
key mapping enters a dedicated
'window'
mode for window management and resizing.

**:new**

> open an empty window, arranged as a new row (horizontally)

**:vnew**

> open an empty window, arranged as a new column (vertically)

**:open**\[**!**] \[*file name*]

> open a new window, displaying file name if given

**:split** \[*file name*]

> split window into rows (horizontally)

**:vsplit** \[*file name*]

> split window into columns (vertically)

**:q**\[**!**] \[*exit code*]

> close currently focused window

**:qall**\[**!**] \[*exit code*]

> close all windows, terminate editor with exit code (defaults to 0)

**:sh**

> open an interactive shell

**:shell**

> alias for
> **:sh**

Commands taking a file name will invoke the
vix-open(1)
utility, if given a file pattern or directory.

## Runtime key mappings

**vix**
supports global as well as window local run time key mappings which are
always evaluated recursively.

**:map**\[**!**] *mode* *lhs* *rhs*

> add a global key mapping

**:map-window**\[**!**] *mode* *lhs* *rhs*

> add a window local key mapping

**:unmap** *mode* *lhs*

> remove a global key mapping

**:unmap-window** *mode* *lhs*

> remove a window local key mapping

In the above
*mode*
refers to one of
'`normal`',
'`insert`',
'`replace`',
'`window`',
'`visual`',
'`visual-line`'
or
'`operator-pending`';
*lhs*
refers to the key to map and
*rhs*
is a key action or alias.
An existing mapping may be overridden by forcing the map command by specifying
**!**.

Because key mappings are always recursive, doing something like:

	:map! normal j 2j

will not work because it would enter an endless loop.
Instead,
**vix**
uses pseudo keys referred to as key actions which can be used to invoke
a set of available editor functions.
**:help**
lists all currently active key bindings as well as all available symbolic
keys.

## Keyboard Layout Specific Mappings

In order to facilitate usage of non-latin keyboard layouts,
**vix**
allows one to map locale specific keys to their latin equivalents by means of the

	**:langmap** *locale-keys* *latin-keys*

command.
As an example, the following maps the movement keys in Russian layout:

	:langmap <?><?><?><?> hjkl

If the key sequences have not the same length, the remainder of the longer
sequence will be discarded.

The defined mappings take effect
in all non-input modes, i.e. everywhere except in insert and replace mode.

## Undo/Redo

**:earlier** [*count*]

	revert to older text state

**:later** [*count*]

	revert to newer text state

If count is suffixed by either of
**d**
(days),
**h**
(hours),
**m**
(minutes)
or
**s**
(seconds)
it is interpreted as an offset from the current system time and the closest
available text state is restored.

# SET OPTIONS

There are a small number of options that may be set
(or unset)
to change the editor's behavior using the
**:set**
command.
This section describes the options, their abbreviations and their
default values.
Boolean options can be toggled by appending
**!**
to the option name.

In each entry below, the first part of the tag line is the full name
of the option, followed by any equivalent abbreviations.
The part in square brackets is the default value of the option.

**shell** ["*/bin/sh*"]

	User shell to use for external commands, overrides
	`SHELL`
	and shell field of password database
	*/etc/passwd*

**escdelay** [*50*]

	Milliseconds to wait before deciding whether an escape sequence should
	be treated as an
	<**Escape**>
	key.

**tabwidth**, **tw** [*8*]

	Display width of a tab and number of spaces to use if
	**expandtab**
	is enabled.

**autoindent**, **ai** [**off**]

	Automatically indent new lines by copying white space from previous line.

**opentab** [**off**]

	Whether to open new files in a new tab when using
	**:edit**
	or
	**:open**.

**expandtab**, **et** [**off**]

	Whether
	<**Tab**>
	should be expanded to
	**tabwidth**
	spaces.

**number**, **nu** [**off**]

	Display absolute line numbers.

**relativenumbers**, **rnu** [**off**]

	Display relative line numbers.

**cursorline**, **cul** [**off**]

	Highlight line primary cursor resides on.

**colorcolumn**, **cc** [*0*]

	Highlight a fixed column.

**horizon** [*32768*]

	How many bytes back the lexer will look to synchronize parsing.

**theme** ["default-16" or "default-256"]

	Color theme to use, name without file extension.
	Loaded from a
	*themes/*
	sub directory of the paths listed in the
	*FILES*
	section.

**syntax** [**auto**]

	Syntax highlighting lexer to use, name without file extension.

**showtabs** [**off**]

	Whether to display replacement symbol instead of tabs.

**shownewlines** [**off**]

	Whether to display replacement symbol instead of newlines.

**showspaces** [**off**]

	Whether to display replacement symbol instead of blank cells.

**showeof** [**on**]

	Whether to display replacement symbol for lines after the end of the file.

**savemethod** [*auto*]

	How the current file should be saved,
	*atomic*
	which uses
	rename(2)
	to atomically replace the file,
	*inplace*
	which truncates the file and then rewrites it or
	*auto*
	which tries the former before falling back to the latter.
	The rename method fails for symlinks, hardlinks, in case of insufficient
	directory permissions or when either the file owner, group, POSIX ACL or
	SELinux labels can not be restored.

**loadmethod** [*auto*]

	How existing files should be loaded,
	*read*
	which copies the file content to an independent in-memory buffer,
	*mmap*
	which memory maps the file from disk and uses OS capabilities as
	caching layer or
	*auto*
	which tries the former for files smaller than 8Mb and the latter for
	larger ones.
	WARNING: modifying a memory mapped file in-place will cause data loss.

**layout** ["v" or "h"]

	Whether to use vertical or horizontal layout.

**ignorecase**, **ic** [**off**]

	Whether to ignore case when searching.

**wrapcolumn**, **wc** [*0*]

	Wrap lines at minimum of window width and wrapcolumn.

**breakat**, **brk** ["**"]

	Characters which might cause a word wrap.

# COMMAND and SEARCH PROMPT

The command and search prompt as opened by
**:**,
**/**,
or
**?**
is implemented as a single line height window, displaying a regular file
whose editing starts in insert mode.
<**Escape**>
switches to normal mode, a second
<**Escape**>
cancels the prompt.
<**Up**>
enlarges the window, giving access to the command history.
<**C-v**>
<**Enter**>
inserts a literal new line thus enabling multiline commands.
<**Enter**>
executes the visual selection if present, or else everything in the
region spawned by the selection position and the delimiting prompt symbols
at the start of adjacent lines.

# CONFIGURATION

**vix**
uses Lua for configuration and scripting purposes.
During startup
*vixrc.lua*
(see the
*FILES*
section) is sourced which can be used to set personal configuration options.
As an example the following will enable the display of line numbers:

	vix:command('set number')

# ENVIRONMENT

`VIX_PATH`

	The default path to use to load Lua support files.

`HOME`

	The home directory used for the
	**cd**
	command if no argument is given.

`TERM`

	The terminal type to use to initialize the curses interface, defaults to
	**xterm**
	if unset.

`SHELL`

	The command shell to use for I/O related commands like
	**!**,
	**>**,
	**<**
	and
	**|**.

`XDG_CONFIG_HOME`

	The configuration directory to use, defaults to
	*$HOME/.config*
	if unset.

# ASYNCHRONOUS EVENTS

`SIGSTOP`

	Suspend editor.

`SIGCONT`

	Resume editor.

`SIGBUS`

	An
	mmap(2)
	ed file got truncated, unsaved file contents will be lost.

`SIGHUP`

`SIGTERM`

	Restore initial terminal state.
	Unsaved file contents will be lost.

`SIGINT`

	When an interrupt occurs while an external command is being run it is terminated.

`SIGWINCH`

	The screen is resized.

# FILES

Upon startup
**vix**
will source the first
*vixrc.lua*
configuration file found from these locations.
All actively used paths can be listed at runtime using the
**:help**
command.

*	*$VIX_PATH*

*	The location of the
	**vix**
	binary (on systems where
	*/proc/self/exe*
	is available).

*	*$XDG_CONFIG_HOME/vix*
	where
	`XDG_CONFIG_HOME`
	refers to
	*$HOME/.config*
	if unset.

*	*/etc/vix*
	for a system-wide configuration provided by administrator.

*	*/usr/local/share/vix*
	or
	*/usr/share/vix*
	depending on the build configuration.  
	When creating a new
	*vixrc.lua*
	be sure to copy the structure from here.

# EXIT STATUS

The **vix** utility exits0 on success, and>0 if an error occurs.

# EXAMPLES

Use
**vix**
as an interactive filter.

	$ { echo Pick your number; seq 1 10; } | vix - > choice

Use the
vix-open(1)
based file browser to list all C language source files:

	:e *.c

Spawn background process and pipe range to its standard input:

	:> { plumber <&3 3<&- & } 3<&0 1>&- 2>&-

# SEE ALSO

sam(1),
vi(1),
vix-clipboard(1),
vix-complete(1),
vix-digraph(1),
vix-menu(1),
vix-open(1)

[A Tutorial for the Sam Command Language](http://doc.cat-v.org/bell_labs/sam_lang_tutorial/sam_tut.pdf)
by
Rob Pike

[The Text Editor sam](http://doc.cat-v.org/plan_9/4th_edition/papers/sam/)
by
Rob Pike

[Plan 9 manual page for sam(1)](http://man.cat-v.org/plan_9/1/sam)

[Structural Regular Expressions](http://doc.cat-v.org/bell_labs/structural_regexps/se.pdf)
by
Rob Pike

[vi - screen-oriented (visual) display editor](http://pubs.opengroup.org/onlinepubs/9699919799/utilities/vi.html)
IEEE Std 1003.1 ("POSIX.1")

# STANDARDS

**vix**
does not strive to be
IEEE Std 1003.1 ("POSIX.1")
compatible, but shares obvious similarities with the
**vi**
utility.

# AUTHORS

**vix**
is written by
Leandro V. Catarin <leavitals@gmail.com>

# BUGS

On some systems there already exists a
**vix**
binary, thus causing a name conflict.

Vix VERSION - March 2, 2026
