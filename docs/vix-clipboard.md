VIX-CLIPBOARD(1) - General Commands Manual

# NAME

**vix-clipboard** - Read from or write to the system clipboard

# SYNOPSIS

**vix-clipboard**
**--usable**

**vix-clipboard**
**--copy**
\[**--selection**&nbsp;*selection*]

**vix-clipboard**
**--paste**
\[**--selection**&nbsp;*selection*]

# DESCRIPTION

**vix-clipboard**
wraps various system-specific tools for interacting with a system clipboard,
like
xsel(1)
for X11,
pbcopy(1)
for Mac OS X,
and
*/dev/clipboard*
on Cygwin.

**vix-clipboard**
can run in three different ways,
depending on the flag given on the command-line.

**--usable**

> In this mode,
> **vix-clipboard**
> looks for a way to interface with the system clipboard.
> If it finds one,
> it terminates with exit code 0.
> If no interface to the system clipboard is available,
> it terminates with exit code 1.

**--copy**

> In this mode,
> **vix-clipboard**
> reads the content of standard input,
> and stores it in the system clipboard.

**--paste**

> In this mode,
> **vix-clipboard**
> reads the content of the system clipboard,
> and writes it to standard output.

**--selection** *selection*

> specify which selection to use, options are "primary" or
> "clipboard". Silently ignored on platforms with a single clipboard.

# ENVIRONMENT

The following environment variables affect the operation of
**vix-clipboard**:

`DISPLAY`

> If non-empty,
> **vix-clipboard**
> will prefer to access the X11 clipboard even if other options are available.

# EXIT STATUS

The **vix-clipboard** utility exits&#160;0 on success, and&#160;&gt;0 if an error occurs.
When run with the
**--usable**
flag,
an exit status of 0 means that it found a supported system-specific tool,
while 1 means that clipboard access is not available.

# EXAMPLES

Test whether clipboard access is available:

	if vix-clipboard --usable; then
		echo "Clipboard access available"
	else
		echo "No clipboard"
	fi

Copy a friendly greeting to the clipboard:

	echo "Hello, World" | vix-clipboard --copy

Send the current contents of the system clipboard to be recorded and analyzed:

	vix-clipboard --paste | curl -d - https://www.nsa.gov/

# SEE ALSO

pbcopy(1),
pbpaste(1),
vix(1),
xclip(1),
xsel(1)

Vix VERSION - February 27, 2026
