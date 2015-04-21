prompt(1) -- consistant line editing
====================================

## SYNOPSIS

`prompt` [_command_...]

## DESCRIPTION

`prompt` sits between your terminal and your shell session and notices when you are being prompted to enter a line. Instead of the kernel canonical terminal mode, you edit the line with newline(1).

## OPTIONS

No options may be specified, executes _command_.

The line editing program may be overridden by placing a custom `newline` early in your PATH.

## BUGS

Lots. This is pre-release software.

## COPYRIGHT

Public Domain

## SEE ALSO

newline(1)

newline(1) -- read edited lines
===============================

## SYNOPSIS

`newline` [options] [_n_]

## DESCRIPTION

`newline` interactively edits _n_ lines (one at a time) and copies them stdout. It supports Unicode (including characters beyond the BMP and combining characters), history, kill and yank, and unambiguous display of inline control characters.

The number of lines to copy, _n_, must come last. It defaults to **1** and may be **inf** to copy an arbitrary number of lines.

## OPTIONS

* `--pty`
	Output a stream suitable for feeding to a pty master. (Don't act on `C-d` or `C-c`.)

## COMMANDS

Below, `C-x` indicates Control-x, likewise `M-x` for Alt-x (Or more preciesly, `ESC x`).

### Movement:

* Move backward one character. `C-b`, `Left`
* Move forward one character. `C-f`, `Right`
* Move backward one word. `M-b`, `C-Left`
* Move forward one word. `M-f`, `C-Right`
* Move backward the whole line.	`C-a`, `Home`
* Move forward the whole line.	`C-e`, `End`

### Delete, Kill, and Yank:

Delete and Kill both remove characters from the line, however characters removed with a Kill can be returned in a different place with a Yank. Multiple Kills in a row will accumulate and may be Yanked all togother.

* Delete backward one character. `Backspace`
* Delete forward one character. `Delete`
* Kill backward one character. `C-h`
* Kill backward one word. `C-w`
* Kill backward the whole line. `C-u`
* Kill forward the whole line. `C-k`
* Yank, insert previously killed text. `C-y`

### Sending text:
* Send with Newline. `C-m`, `Enter`
* Send with End of Text. `C-d`
* Send empty line with interrupt. `C-c`

### History:
* Previous line in history. `C-p`, `Up`
* Next line in history. `C-n`, `Down`
* Undo all changes to this line. `C-z`

### Miscellaneous:
* Enable line echo: `C-q`
* Disable line echo: `C-S`
* Insert literal character: `C-v`

## BUGS

`newline` makes a number of assumptions:

* All input and output is UTF-8.
* Terminals understand `ESC [ Ps C`: Move cursor forward `Ps` times.
* Terminals understand `ESC [ Ps D`: Move cursor backward `Ps` times.
* Terminals understand `ESC [ 0 K`: Erase everything to the right of cursor.
* Terminals understand or ignore `ESC [ Pm (; Pm)* m`: Set character attributes (color, intensity, and normal).

## COPYRIGHT

Public Domain

## SEE ALSO

prompt(1), readline(3), editline(3), [linenoise](https://github.com/antires/linenoise)

