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
XX_TP(XX_FB(Trace Data and Events))
If set, network traffic (both a hexadecimal representation and its
interpretation) is logged to the file
XX_FB(/tmp/x3trc.)`'XX_FI(pid),
and a window is popped up to monitor the data.
The file name is confirmed with a pop-up; the default directory name for the
trace file can be changed with the "x3270.traceDir" resource.
Keyboard and mouse events are also traced.
This is useful for creating and debugging custom keymaps, macros
and scripts.
For example, it will tell you precisely what action was taken in response to
pressing a particular key.
If the key is not mapped, it will tell you the keysym name and keycode so you
can add it to a custom keymap.
XX_TP(XX_FB(Save Screen Contents))
Allows the contents of the screen to be saved.
A dialog allows selection between continuous saving (a new image added each
time the screen changes) or just a snapshot.
It also allows switching beteen saving to a file and writing directly to the
printer.
If the screens are saved to a file, the dialog allows a choice between
text, HTML or RichText output.
By default, the file name is XX_FB(/tmp/x3scr.)`'XX_FI(pid)XX_FB(.)`'XX_FI(type)
in /tmp. The default directory name can be changed with the "x3270.traceDir"
resource.
The dialog also allows the default print command (XX_FB(lpr)) to be changed.
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
XX_TP(XX_FB(Toggles))
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
If set, the XX_SM(NVT) terminal emulator automatically assumes a XX_SM(NEWLINE)
character when it reaches the end of a line.
XX_TP(XX_FB(Paste with Left Margin))
If set, puts restrictions on how pasted text is placed on the screen.
The position of the cursor at the time the `paste' operation is begun is
used as a left margin.
No pasted text will fill any area of the screen to the left of that
position.
This option is useful for pasting into certain XX_SM(IBM) editors that use the
left side of the screen for control information.
XX_TP(XX_FB(Overlay Paste))
If set, pasting over a protected field will simply increment the cursor
position instead of locking the keyboard.
This allows forms to be copied and pasted with the protected fields
included.
Setting this toggle also implicitly sets the XX_FB(Paste with Left Margin)
toggle.
XX_TP(XX_FB(Select by Rectangles))
If set, XX_FB(x3270) will always select rectangular areas of the screen.
Otherwise, XX_FB(x3270)
selects by rectangles when in 3270 mode, but in XX_SM(NVT) mode it selects
continuous regions of the screen like XX_FI(xterm)(1).
XX_TP(XX_FB(Crosshair Cursor))
If set, XX_FB(x3270) will display a crosshair over the cursor: lines extending
the full width and height of the screen, centered over the cursor position.
This makes locating the cursor on the screen much easier.
XX_TP(XX_FB(Visible Control Chars))
If set, XX_FB(x3270) will display control characters as special underscored
symbols instead of blanks.
Field attribute characters are displayed in underscored yellow.
Other control characters are displayed according to their field colors and
attributes.
XX_LP
XX_TS(2,c l.)
XX_TR(XX_TD(Symbol)	XX_TD(Meaning))
XX_TR(XX_TD(Period)	XX_TD(NUL (X'00')))
XX_TR(XX_TD(<)	XX_TD(SO (Start of DBCS subfield)))
XX_TR(XX_TD(>)	XX_TD(SI (End of DBCS subfield)))
XX_TR(XX_TD(0-9 A-V)	XX_TD(Field attribute))
XX_TE()
The mappings for field attribute characters are included in the XX_FB(x3270)
Resource documentation for the XX_FB(visibleControl) resource.
XX_TPE()dnl
XX_TPE()dnl
XX_LP
The names of the toggles for use with the XX_FB(XX_DASHED(set))
and XX_FB(XX_DASHED(clear))
options are as follows:
XX_LP
XX_TS(2,center;
l l .)
XX_TR(XX_TD(Menu Option)	XX_TD(Name))
XX_T_()
XX_TR(XX_TD(Monocase)	XX_TD(monoCase))
XX_TR(XX_TD(Blinking Cursor)	XX_TD(cursorBlink))
XX_TR(XX_TD(Blank Fill)	XX_TD(blankFill))
XX_TR(XX_TD(Show Timing)	XX_TD(showTiming))
XX_TR(XX_TD(Track Cursor)	XX_TD(cursorPos))
XX_TR(XX_TD(Trace Data and Events)	XX_TD(trace))
XX_TR(XX_TD(Save Screen(s) in File)	XX_TD(screenTrace))
XX_TR(XX_TD(Scrollbar)	XX_TD(scrollBar))
XX_TR(XX_TD(Wraparound)	XX_TD(lineWrap))
XX_TR(XX_TD(Paste with Left Margin)	XX_TD(marginedPaste))
XX_TR(XX_TD(Select by Rectangles)	XX_TD(rectangleSelect))
XX_TR(XX_TD(Crosshair Cursor)	XX_TD(crosshair))
XX_TR(XX_TD(Visible Control Chars)	XX_TD(visibleControl))
XX_TE()
XX_LP
In addition, the toggle XX_FB(altCursor) can be used to select the cursor type.
If set, an underline cursor will be used.
If clear, the normal block cursor will be used.
The toggle XX_FB(aidWait) can also be used to change how actions that send an
AID to the host behave: when clear, they no longer block until the host
unlocks the keyboard.
XX_LP
These names also represent resources that can be set in your .Xdefaults
or .x3270pro file.
For example, if you always want to have the scrollbar off, you can add
the following to your .Xdefaults or .x3270pro:
XX_BR
XX_RS(XX_PRODUCT.scrollBar:	false)
XX_BR
XX_LP
These names are also used as the first parameter to the XX_FB(Toggle) action.
