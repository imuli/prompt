# prompt

Consistent line editing for terminals.

# newline

	newline [options] [n]

`newline` interactively edits `n` lines (one at a time) and copies them stdout. `n` defaults to **1** and may be **inf** to copy an arbitrary number of lines. `newline` supports Unicode (including characters beyond the BMP and combining characters), history, kill and yank, and unambiguous display of inline control characters. All options are ignored at this time.

`newline` makes a number of assumptions:
* All input and output is UTF-8.
* Terminals understand these three escape sequences:
	* `ESC [ Ps C`: Move cursor forward `Ps` times.
	* `ESC [ Ps D`: Move cursor backward `Ps` times.
	* `ESC [ 0 K`: Erase everything to the right of cursor.
* Terminals understand or ignore `ESC [ Pm (; Pm)* m`: Set character attributes (color, intensity, and normal).

### Keyboard Bindings:

#### Movement:

* Move backward one character. `C-b` `Left`
* Move forward one character. `C-f` `Right`
* Move backward one word. `M-b` `C-Left`
* Move forward one word. `M-f` `C-Right`
* Move backward the whole line.	`C-a` `Home`
* Move forward the whole line.	`C-e` `End`

#### Delete, Kill, and Yank:

Delete and Kill both remove characters from the line, however characters removed with a Kill can be returned in a different place with a Yank. Multiple Kills in a row will accumulate and may be Yanked all togother.

* Delete backward one character. `Backspace`
* Delete forward one character. `Delete`
* Kill backward one character. `C-h`
* Kill backward one word. `C-w`
* Kill backward the whole line. `C-u`
* Kill forward the whole line. `C-k`
* Yank. `C-y`

#### Sending text:
* Send with Newline. `C-m` `Enter`
* Send with End of Text. `C-d`
* Send empty line with interrupt. `C-c`

#### History:
* Previous line in history. `C-p` `Up`
* Next line in history. `C-n` `Down`
* Undo all changes to this line. `C-z`

#### Miscellaneous:
* Enable line echo: `C-q`
* Disable line echo: `C-S`
* Insert literal character: `C-v`

## Copying

Public domain
