VIX-COMPLETE(1) - General Commands Manual

# NAME

**vix-complete** - Interactively complete file or word

# SYNOPSIS

**vix-complete**
\[**--file**&nbsp;|&nbsp;**--word**]
\[*--*]
*pattern*

**vix-complete**
**-h**&nbsp;|&nbsp;**--help**

# DESCRIPTION

**vix-complete**
takes a pattern on the command-line and completes file or word and displays
them in a menu for the user to select one.
Once the user has selected a completion, the completion (excluding the
pattern) is printed to standard output.

**vix-complete**
uses
vix-menu(1)
as its user-interface,
so see that page for more details.

**--file**

> This passes
> *pattern*
> to
> **find**
> to obtain a list of matching file names
> (this is the default).

**--word**

> This reads standard input to obtain a list of lines matching
> *pattern*.

**--**

> An argument following this token will be treated as pattern,
> even if it would otherwise be a valid command-line option.

> If encountered after a previous instance of
> `--`
> it is treated as a pattern.

*pattern*

> The pattern to be completed by file or word.

**-h** | **--help**

> If present,
> **vix-complete**
> prints a usage summary and exits,
> ignoring any other flag and arguments.

# EXIT STATUS

The **vix-complete** utility exits&#160;0 on success, and&#160;&gt;0 if an error occurs.

In particular,
like
vix-menu(1),
**vix-complete**
prints nothing and sets its exit status to 1
if the user refused to select a file.

# SEE ALSO

vix(1),
vix-menu(1)

# BUGS

Because
**vix-complete**
uses
find(1)
to obtain the list of files, weird things might happen if you have
control-characters in your filenames.

Vix VERSION - February 27, 2026
