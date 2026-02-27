VIX-OPEN(1) - General Commands Manual

# NAME

**vix-open** - Interactively select a file to open

# SYNOPSIS

**vix-open**
\[**-p**&nbsp;*prompt*]
\[**-f**]
\[*--*]
\[*files*]

**vix-open**
**-h**

# DESCRIPTION

**vix-open**
takes a list of filenames and directories on the command-line
and displays them in a menu for the user to select one.
If the user selects a directory
(including
`..`),
the directory contents are displayed as a fresh menu.
Once the user has selected a filename,
its absolute path is printed to standard output.

**vix-open**
uses
vix-menu(1)
as its user-interface,
so see that page for more details.

**-p** *prompt*

> Display
> *prompt*
> before the list of items.
> This is passed straight through to
> vix-menu(1).

**-f**

> Normally,
> if
> **vix-open**
> is provided with a single filename or directory argument,
> it will automatically select it
> (printing the filename to standard output,
> or presenting a new menu with the contents of the directory).
> If
> **-f**
> is provided,
> **vix-open**
> will always present the arguments it's given,
> even if there's only one.

**--**

> If this token is encountered before the first non-option argument,
> all following arguments will be treated as menu-items,
> even if they would otherwise be valid command-line options.

> If encountered after the first non-option argument,
> or after a previous instance of
> `--`
> it is treated as a menu-item.

*files*

> File and directory names to be presented to the user.
> If a name does not exist on the filesystem
> and the user selects it,
> it is treated as a file.

**-h**

> If present,
> **vix-open**
> prints a usage summary and exits,
> ignoring any other flag and arguments.

# EXIT STATUS

The **vix-open** utility exits&#160;0 on success, and&#160;&gt;0 if an error occurs.

In particular,
like
vix-menu(1),
**vix-open**
prints nothing and sets its exit status to 1
if the user refused to select a file.

# EXAMPLES

	CHOICE=$(vix-open -p "Select a file to stat")
	if [ $? -gt 0 ]; then
		echo "No selection was made, or an error occurred"
	else
		stat "$CHOICE"
	fi

# SEE ALSO

vix(1),
vix-menu(1)

# BUGS

Because
**vix-open**
uses
ls(1)
to obtain the contents of a directory,
weird things might happen if you have control-characters in your filenames.

Vix VERSION - February 27, 2026
