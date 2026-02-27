VIX-MENU(1) - General Commands Manual

# NAME

**vix-menu** - Interactively select an item from a list

# SYNOPSIS

**vix-menu**
\[**-i**]
\[**-t**&nbsp;|&nbsp;**-b**]
\[**-p**&nbsp;*prompt*]
\[**-l**&nbsp;*lines*]
\[*initial*]  
**vix-menu**
\[**-v**]

# DESCRIPTION

**vix-menu**
allows a user to interactively select one item from a list of options.
A newline-separated list of items is read from standard input,
then the list of items is drawn directly onto the terminal
so the user may select one.
Finally,
the selected item is printed to standard output.

For information on actually navigating the menu,
see
*USAGE*
below.

**-i**

> Use case-insensitive comparison when filtering items.

**-t** | **-b**

> Normally,
> the menu is displayed on the current line of the terminal.
> When
> **-t**
> is provided, the menu will always be drawn on the top line of the terminal.
> When
> **-b**
> is provided, the menu will always be drawn on the bottom line.

**-p** *prompt*

> Display
> *prompt*
> before the list of items.

**-l** *lines*

> Normally,
> the list is displayed with all the items side-by-side on a single line,
> which is space-efficient
> but does not show many items at a time,
> especially if some of them are long.
> When
> **-l**
> is provided,
> the list is displayed with each item on its own line,
> *lines*
> lines high.
> If there are more than
> *lines*
> items in the list,
> the user can scroll through them with the arrow keys,
> just like in the regular horizontal mode.

*initial*

> The user can type into a text field
> to filter the list of items
> as well as scrolling through them.
> If supplied,
> *initial*
> is used as the initial content of the text field.

**-v**

> Instead of displaying an interactive menu,
> **vix-menu**
> prints its version number to standard output and exits.

# USAGE

**vix-menu**
displays the prompt (if any),
a text field,
and a list of items.
Normally these are presented side-by-side in a single line,
but if the
**-l**
flag is given,
the prompt and typing area will be on the first line,
and list items on the following lines.

The following commands are available:

**Enter**

> selects the currently-highlighted list item and exits.

**Control-&#92;**
or
**Control-]**

> selects the current contents of the text field
> (even if it does not appear in the list)
> and exits.

**ESC ESC**
or
**Control-C**

> exit without selecting any item.

**Down**
or
**Control-N**

> scroll forward through the available list items.

**Up**
or
**Control-P**

> scroll backward through the available list items.

**Right**
or
**Control-F**

> move the cursor forward through the typed text,
> and scroll through the available list items.

**Left**
or
**Control-B**

> move the cursor backward through the typed text,
> and scroll through the available list items.

**PageUp**
or
**Control-V**

> scrolls to show the previous page of list items.

**PageDown**
or
**Meta-v**

> scrolls to show the next page of list items.

**Home**
or
**Control-A**

> move the cursor to the beginning of the text field
> or scroll to the first item in the list.

**End**
or
**Control-E**

> move the cursor to the end of the text field
> or scroll to the last item in the list.

**Meta-b**

> moves the cursor to the beginning of the current word in the text field.

**Meta-f**

> moves the cursor past the end of the current word in the text field.

**Tab**

> copies the content of the selected list item into the text field.
> This is almost, but not quite, like tab completion.

**Delete**
or
**Control-D**

> delete the character in the text field under the cursor.

**Backspace**

> deletes the character in the text field to the left of the cursor.

**Meta-d**

> deletes the characters in the text field
> from the character under the cursor
> to the next space.

**Control-K**

> deletes the characters in the text field
> from the character under the cursor to the end.

**Control-U**

> deletes the characters in the text field
> from the beginning up to
> (but not including)
> the character under the cursor.

**Control-W**

> deletes the characters in the text field
> from the previous space up to
> (but not including)
> the character under the cursor.

All other non-control characters will be inserted into the text field
at the current cursor position.

When there is text in the text field,
only list items that include the given text will be shown.
If the text contains one or more spaces,
each space-delimited string is a separate filter
and only items matching every filter will be shown.

If the user filters out all the items from the list,
then hits Enter to select the
"currently highlighted"
item,
the text they typed will be returned instead.

# EXAMPLES

Here's a shell-script that allows the user to choose a number from one to 10:

	NUMBER=$(seq 1 10 | vix-menu -p "Choose a number")
	if [ $? -eq 0 ]; then
		echo "You chose: $NUMBER"
	else
		echo "You refused to choose a number, or an error occurred."
	fi

# DIAGNOSTICS

The
**vix-menu**
utility exits 0 if the user successfully selected an item from the list,
and 1 if the user cancelled.

If an internal error occurs,
the
**vix-menu**
utility prints a message to standard error and terminates with an exit
status greater than 1.
Potential error conditions include
being unable to allocate memory,
being unable to read from standard input,
or being run without a controlling terminal.

# SEE ALSO

dmenu(1),
slmenu(1),
vix(1)

# HISTORY

The original model for a single line menu reading items from standard input was
dmenu(1)
which implements the idea for X11.
**dmenu**
is available from
`http://tools.suckless.org/dmenu/`

The code was subsequently re-worked for ANSI terminal output as
slmenu(1)
which is available from
`https://bitbucket.org/rafaelgg/slmenu/`

Since
**slmenu**
did not appear to be maintained,
it was forked to become
**vix-menu**
to be distributed with
vix(1).

Vix VERSION - February 27, 2026
