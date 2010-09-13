XX_SH(Menus)
XX_FB(x3270) has a menu bar with three pull-down menus (File, Options, and
Connect) and a button to turn the keypad on and off.
The pull-down menus are also available as pop-up menus by using the "Ctrl"
key and the left, middle and right mouse buttons, respectively.
XX_PP
The menu bar can be turned off by setting the "x3270.menuBar" resource to
XX_FB(false.)
XX_PP
Many sections of the File and Options menus are toggles,
options that may be either on or off.
The entries under the File menu are as follows:
XX_TPS()dnl
XX_TP(XX_FB(File Transfer))
Initiates transferring a file between the IBM host and the local workstation,
using the XX_FB(IND$FILE) protocol.
A pop-up menu allows specifying the file names and other attributes of the
transfer.
XX_BR()
The IND$FILE program must be installed on the IBM host, and the 3270 cursor
must be located in a field that will accept a TSO or VM/CMS command.
XX_TP(XX_FB(Printer Session))
Starts or stops a printer session.
XX_TP(XX_FB(Trace Data Stream))
If set, network traffic (both a hexadecimal representation and its
interpretation) are logged to the file
XX_FB(/tmp/x3trc.)`'XX_FI(process_id),
and a window is popped up to monitor the data.
The file name is confirmed with a pop-up; the default directory name for the
trace file can be changed with the "x3270.traceDir" resource.
XX_TP(XX_FB(Trace Keyboard/Mouse Events))
If set, information about keyboard and mouse events and the actions that
XX_FB(x3270) takes in response are logged to a file.
This is the same file as used for tracing the data stream, above.
Event tracing is useful for creating and debugging custom keymaps, macros
and scripts.
For example, it will tell you precisely what action was taken in response to
pressing a particular key.
If the key is not mapped, it will tell you the keysym name and keycode so you
can add it to a custom keymap.
XX_TP(XX_FB(Save Screen(s) in File))
If set, saves an XX_SM(ASCII) representation of the current screen image in
the file XX_FB(/tmp/x3scr.)`'XX_FI(process_id).
A pop-up allows the file name to be changed; the default directory name can be
changed with the "x3270.traceDir" resource.
The pop-up also has buttons to choose between saving just the current image,
or continuously saving it as it is redrawn.
XX_TP(XX_FB(Print Screen Text))
Pipes an XX_SM(ASCII) representation of the screen contents to a command for
printing.
The default command is XX_FB(lpr).
XX_TP(XX_FB(Save Screen Text in File))
Appends a text representation of the screen contents to a file.
The data can be saved in plain XX_SM(ASCII) or XX_SM(HTML).
XX_TP(XX_FB(Print Window Bitmap))
Uses the XX_FB(xwd) and XX_FB(xpr) commands to print a copy of the graphical
screen contents.
XX_TP(XX_FB(Save Changed Options in File))
Saves into a file the values of all options that have been changed since
XX_FB(x3270) was started.
A pop-up allows the file name to be changed; the default file
is XX_FB(.x3270pro) in the XX_POSESSIVE(user) home directory.
If the file already exists, it is appended to.
XX_FB(x3270) will read the contents of this file the next time it starts up.
The options settings in the file override any resources defined
with XX_FB(xrdb) or in the XX_POSESSIVE(user) XX_FB(.Xdefaults)
file; command-line switches override the file.
A different options file can be specified by the XX_FB(X3270PRO)
environment variable.
If the environment variable XX_FB(NOX3270PRO)
is set, no options file will be read.
XX_TP(XX_FB(Execute an Action))
Allows an action name and parameters to be entered from the keyboard.
This allows experimentation with actions without having to edit keymaps
and repeatedly restart XX_FB(x3270).
XX_LP
The toggles under the Options menu are as follows:
XX_TPS()dnl
XX_TP(XX_FB(Monocase))
If set, XX_FB(x3270) operates in uppercase-only mode.
XX_TP(XX_FB(Blinking Cursor))
If set, the cursor blinks once per second.
XX_TP(XX_FB(Blank Fill))
If set, XX_FB(x3270) behaves in some un-3270-like ways.
First, when a character is typed into a field, all nulls in the field to the
left of that character are changed to blanks.
This eliminates a common 3270 data-entry surprise.
Second, in insert mode, trailing blanks in a field are treated like nulls,
eliminating the annoying XX_DQUOTED(lock-up) that often occurs when inserting into an
field with (apparent) space at the end.
XX_TP(XX_FB(Show Timing))
If set, the time taken by the host to process an XX_SM(AID) is displayed on
the status line.
XX_TP(XX_FB(Track Cursor))
If set, the cursor position is displayed on the status line.
XX_TP(XX_FB(Scrollbar))
If set, the scrollbar appears.
XX_TP(XX_FB(Wraparound))
If set, the XX_SM(ANSI) terminal emulator automatically assumes a XX_SM(NEWLINE)
character when it reaches the end of a line.
XX_TP(XX_FB(Paste with Left Margin))
If set, puts restrictions on how pasted text is placed on the screen.
The position of the cursor at the time the `paste' operation is begun is
used as a left margin.
No pasted text will fill any area of the screen to the left of that
position.
This option is useful for pasting into certain XX_SM(IBM) editors that use the
left side of the screen for control information.
XX_TP(XX_FB(Select by Rectangles))
If set, XX_FB(x3270) will always select rectangular areas of the screen.
Otherwise, XX_FB(x3270)
selects by rectangles when in 3270 mode, but in XX_SM(ANSI) mode it selects
continuous regions of the screen like XX_FI(xterm)(1).
XX_TP(XX_FB(Crosshair Cursor))
If set, XX_FB(x3270) will display a crosshair over the cursor: lines extending
the full width and height of the screen, centered over the cursor position.
This makes locating the cursor on the screen much easier.
XX_TPE()dnl
XX_TPE()dnl
XX_LP
