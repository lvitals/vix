VIX-DIGRAPH(1) - General Commands Manual

# NAME

**vix-digraph** - print Unicode character using mnemonics

# SYNOPSIS

**vix-digraph**
*digraph&nbsp;...*  
**vix-digraph**
**-**

# DESCRIPTION

**vix-digraph**
read a digraph from command line argument or standard input and print
a corresponding Unicode character to standard output, encoded in current
locale.

*digraph*

> A set of two (or more) characters that get replaced by a single Unicode
> character.

**-**

> Read digraph from standard input.

Without argument,
**vix-digraph**
displays all available digraphs along with a description.

# ENVIRONMENT

`LC_CTYPE`

> Locale definition, setting the encoding format of the Unicode character printed out.
> See
> locale(1)
> for more information.

# EXIT STATUS

0

> Digraph correctly read recognised and printed out.

1

> Digraph not recognised.

2

> Digraph is the beginning of an existing digraph, but does not correspond to a full digraph.

3

> An error occurred and digraph could not be read or printed.

# SEE ALSO

locale(1),
vix(1)

# STANDARDS

**vix-digraph**
follows the digraph format from
[RFC 1345](http://tools.ietf.org/rfc/rfc1345.txt)

Vix VERSION - February 27, 2026
