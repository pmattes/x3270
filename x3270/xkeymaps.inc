dnl Copyright (c) 1993-2009, Paul Mattes.
dnl All rights reserved.
dnl 
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions
dnl are met:
dnl     * Redistributions of source code must retain the above copyright
dnl       notice, this list of conditions and the following disclaimer.
dnl     * Redistributions in binary form must reproduce the above copyright
dnl       notice, this list of conditions and the following disclaimer in the
dnl       documentation and/or other materials provided with the distribution.
dnl     * Neither the name of Paul Mattes nor his contributors may be used
dnl       to endorse or promote products derived from this software without
dnl       specific prior written permission.
dnl 
dnl THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
dnl OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
dnl WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
dnl DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
dnl INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
dnl (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
dnl SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
dnl HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
dnl STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
dnl IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
dnl POSSIBILITY OF SUCH DAMAGE.
XX_SH(Keymaps)
XX_PP
The type of keyboard may be specified with the XX_FB(XX_DASHED(keymap))
switch or using either the XX_SM(KEYMAP) or XX_SM(KEYBD) environment variables.
The types of supported keyboards `include'
XX_FB(sun_k3),
XX_FB(sun_k4),
XX_FB(sun_k5),
XX_FB(`hp'XX_DASH()k1),
XX_FB(`hp'XX_DASH()pc)
and
XX_FB(ncd).
XX_PP
The keymap may also be specified as a comma-separated list of names.
Later definitions override earlier ones.
This is used to specify both a primary keyboard type and a set of modifiers.
The modifiers defined `include':
XX_TPS()dnl
XX_TP(XX_FB(ow))
(OpenWindows) Swaps the middle and right mouse button definitions, so the
middle button performs the "Extend" function and the right-hand button
performs the "Paste" function.
Also changes the cut and `paste' actions to use the OpenWindows XX_SM(CLIPBOARD).
XX_TP(XX_FB(apl))
Allows entry of XX_SM(APL) characters (see XX_LINK(#APL-Support,XX_SM(APL SUPPORT))
below).
XX_TP(XX_FB(finnish7))
Replaces the bracket, brace and bar keys with common Finnish characters.
XX_TP(XX_FB(norwegian7))
Replaces the bracket, brace and bar keys with common Norwegian characters.
XX_TPE()dnl
XX_PP
A temporary keymap can also be specified while XX_FB(x3270)
is running with the XX_FB(Keymap) action.
When the action XX_FB(Keymap)(XX_FI(n)) is executed, temporary keymap
XX_FI(n)
is added to or deleted from the current keymap.
Multiple temporary keymaps can be active simultaneously.
The action XX_FB(Keymap(None)) restores the original keymap.
XX_FB(Note:)
When Keymap() is specified as part of a list of multiple actions in a keymap,
it must be the last action in the list.
XX_PP
The temporary keymap XX_FB(hebrew)
is provided to allow entry of Hebrew characters.
XX_PP
The X Toolkit translation mechanism is used to provide keyboard emulation.
It maps XX_FB(events) into XX_FB(actions.)
The best documentation can be found with X toolkit documents, but the
following should suffice for simple customization.
XX_PP
An Xt event consists of (at least) four fields.  The first is called a
XX_FB(modifier).
It may be any combination of XX_FB(Meta), XX_FB(Shift) and XX_FB(Ctrl).  If it
is prefaced by XX_FB(!),
it means those modifiers only.  The second field is the specific event,
in XX_FB(x3270)
usually just XX_KEY.
The third field is the detail field, which gives the actual key.
The name of the key may be determined using the XX_FI(xev)
program or with the "Trace X Events" menu option.
The last field is the action, which is the internal emulator function.
A complete list of actions may be found later in the manual.
XX_PP
There are three levels of translation tables in XX_FB(x3270).
The first is a defined by the resource
XX_FB(x3270.keymap.base).
It defines alphabetic, numeric,
function keys, and such basic functions as Enter and Delete.
It allows a minimal useful functionality.
It is generally compiled in XX_FB(x3270), but can be overridden.
XX_PP
The second level is a keyboard specific table, which is selected by the
XX_FB(x3270.keymap) resource, and defined by the
XX_FB(x3270.keymap.)`'XX_FI(name) resource (where XX_FI(name) is the value
of the XX_FB(x3270.keymap) resource).
This keymap defines actions for such things as keypad
keys, and keys unique to certain keyboards.
Several predefined keymaps are included with XX_FB(x3270).
XX_PP
The third level is a user customizable table which may be used to augment or
override key definitions.
This keymap is defined by the XX_FB(x3270.keymap.)`'XX_FI(name)`'XX_FB(.user)
resource.
XX_PP
In addition, keymaps may be defined for use in 3270 mode or XX_SM(NVT) mode
only.
These keymaps use the suffixes XX_FB(.3270) and XX_FB(.nvt) in their names,
respectively.
If a keymap XX_FB(x3270.keymap.)`'XX_FI(name)`.'XX_FI(mode) is defined, it
will augment the keymap XX_FB(x3270.keymap.)`'XX_FI(name) when XX_FB(x3270)
is in the given XX_FI(mode).
If a keymap XX_FB(x3270.keymap.)`'XX_FI(name)`.'XX_FB(user)`.'XX_FI(mode) is
defined, it will augment the
keymap XX_FB(x3270.keymap)`.'XX_FI(name)`.'XX_FB(user) when
XX_FB(x3270)
is in the given XX_FI(mode).
XX_PP
XX_TARGET(default-keymap)dnl
The default translation table XX_FB(x3270.keymap.base) is:
XX_TS(2,l l .)
XX_TR(XX_TD(XX_KEY(,Multi_key))	XX_TD(Compose()))
XX_TR(XX_TD(XX_KEY(Shift,Left))	XX_TD(KybdSelect(Left,PRIMARY)))
XX_TR(XX_TD(XX_KEY(,Left))	XX_TD(Left()))
XX_TR(XX_TD(XX_KEY(Meta,Right))	XX_TD(NextWord()))
XX_TR(XX_TD(XX_KEY(Shift,Right))	XX_TD(KybdSelect(Right,PRIMARY)))
XX_TR(XX_TD(XX_KEY(,Right))	XX_TD(Right()))
XX_TR(XX_TD(XX_KEY(Shift,Up))	XX_TD(KybdSelect(Up,PRIMARY)))
XX_TR(XX_TD(XX_KEY(,Up))	XX_TD(Up()))
XX_TR(XX_TD(XX_KEY(Shift,Down))	XX_TD(KybdSelect(Down,PRIMARY)))
XX_TR(XX_TD(XX_KEY(,Down))	XX_TD(Down()))
XX_TR(XX_TD(XX_BTN(Ctrl,1Down))	XX_TD(HandleMenu(quitMenu)))
XX_TR(XX_TD(XX_BTN(Ctrl,2Down))	XX_TD(HandleMenu(optionsMenu)))
XX_TR(XX_TD(XX_BTN(Ctrl,3Down))	XX_TD(HandleMenu(hostMenu)))
XX_TR(XX_TD(XX_BTN(Shift,1Down))	XX_TD(MoveCursor()))
XX_TR(XX_TD(XX_BTN(,1Down))	XX_TD(`select'XX_DASH()start()))
XX_TR(XX_TD(XX_BTN(,1Motion))	XX_TD(`select'XX_DASH()extend()))
XX_TR(XX_TD(XX_BTN(,2Down))	XX_TD(ignore()))
XX_TR(XX_TD(XX_BTN(,2Motion))	XX_TD(ignore()))
XX_TR(XX_TD(XX_BTN(,2Up))	XX_TD(`insert'XX_DASH()selection(PRIMARY)))
XX_TR(XX_TD(XX_BTN(,3Down))	XX_TD(`start'XX_DASH()extend()))
XX_TR(XX_TD(XX_BTN(,3Motion))	XX_TD(`select'XX_DASH()extend()))
XX_TR(XX_TD(XX_BTN(,Up))	XX_TD(`select'XX_DASH()end(PRIMARY)))
XX_TR(XX_TD(XX_KEY(Meta,F1))	XX_TD(PF(13)))
XX_TR(XX_TD(XX_KEY(Shift,F1))	XX_TD(PF(13)))
XX_TR(XX_TD(XX_KEY(Meta,F2))	XX_TD(PF(14)))
XX_TR(XX_TD(XX_KEY(Shift,F2))	XX_TD(PF(14)))
XX_TR(XX_TD(XX_KEY(Meta,F3))	XX_TD(PF(15)))
XX_TR(XX_TD(XX_KEY(Shift,F3))	XX_TD(PF(15)))
XX_TR(XX_TD(XX_KEY(Meta,F4))	XX_TD(PF(16)))
XX_TR(XX_TD(XX_KEY(Shift,F4))	XX_TD(PF(16)))
XX_TR(XX_TD(XX_KEY(Meta,F5))	XX_TD(PF(17)))
XX_TR(XX_TD(XX_KEY(Shift,F5))	XX_TD(PF(17)))
XX_TR(XX_TD(XX_KEY(Meta,F6))	XX_TD(PF(18)))
XX_TR(XX_TD(XX_KEY(Shift,F6))	XX_TD(PF(18)))
XX_TR(XX_TD(XX_KEY(Meta,F7))	XX_TD(PF(19)))
XX_TR(XX_TD(XX_KEY(Shift,F7))	XX_TD(PF(19)))
XX_TR(XX_TD(XX_KEY(Meta,F8))	XX_TD(PF(20)))
XX_TR(XX_TD(XX_KEY(Shift,F8))	XX_TD(PF(20)))
XX_TR(XX_TD(XX_KEY(Meta,F9))	XX_TD(PF(21)))
XX_TR(XX_TD(XX_KEY(Shift,F9))	XX_TD(PF(21)))
XX_TR(XX_TD(XX_KEY(Meta,F10))	XX_TD(PF(22)))
XX_TR(XX_TD(XX_KEY(Shift,F10))	XX_TD(PF(22)))
XX_TR(XX_TD(XX_KEY(Meta,F11))	XX_TD(PF(23)))
XX_TR(XX_TD(XX_KEY(Shift,F11))	XX_TD(PF(23)))
XX_TR(XX_TD(XX_KEY(Meta,F12))	XX_TD(PF(24)))
XX_TR(XX_TD(XX_KEY(Shift,F12))	XX_TD(PF(24)))
XX_TR(XX_TD(XX_KEY(,F1))	XX_TD(PF(1)))
XX_TR(XX_TD(XX_KEY(,F2))	XX_TD(PF(2)))
XX_TR(XX_TD(XX_KEY(,F3))	XX_TD(PF(3)))
XX_TR(XX_TD(XX_KEY(,F4))	XX_TD(PF(4)))
XX_TR(XX_TD(XX_KEY(,F5))	XX_TD(PF(5)))
XX_TR(XX_TD(XX_KEY(,F6))	XX_TD(PF(6)))
XX_TR(XX_TD(XX_KEY(,F7))	XX_TD(PF(7)))
XX_TR(XX_TD(XX_KEY(,F8))	XX_TD(PF(8)))
XX_TR(XX_TD(XX_KEY(,F9))	XX_TD(PF(9)))
XX_TR(XX_TD(XX_KEY(,F10))	XX_TD(PF(10)))
XX_TR(XX_TD(XX_KEY(,F11))	XX_TD(PF(11)))
XX_TR(XX_TD(XX_KEY(,F12))	XX_TD(PF(12)))
XX_TR(XX_TD(XX_KEY(Alt,q))	XX_TD(Quit()))
XX_TR(XX_TD(XX_KEY(,Prior))	XX_TD(`Scroll(Backward)'))
XX_TR(XX_TD(XX_KEY(,Next))	XX_TD(`Scroll(Forward)'))
XX_TR(XX_TD(:XX_KEY(,))	XX_TD(Default()))
XX_TE()
XX_PP
The default 3270-mode table XX_FB(x3270.keymap.base.3270) adds the
following definitions:
XX_TS(2,l l .)
XX_TR(XX_TD(XX_KEY(Shift,Return))	XX_TD(Newline()))
XX_TR(XX_TD(XX_KEY(,Return))	XX_TD(Enter()))
XX_TR(XX_TD(XX_KEY(,Linefeed))	XX_TD(Newline()))
XX_TR(XX_TD(XX_KEY(Shift,Tab))	XX_TD(BackTab()))
XX_TR(XX_TD(XX_KEY(,Tab))	XX_TD(Tab()))
XX_TR(XX_TD(XX_KEY(,Home))	XX_TD(Home()))
XX_TR(XX_TD(XX_KEY(Meta,Left))	XX_TD(PreviousWord()))
XX_TR(XX_TD(XX_KEY(Meta,Right))	XX_TD(NextWord()))
XX_TR(XX_TD(XX_KEY(,Insert))	XX_TD(Insert()))
XX_TR(XX_TD(XX_KEY(,Delete))	XX_TD(Delete()))
XX_TR(XX_TD(XX_KEY(,BackSpace))	XX_TD(BackSpace()))
XX_TR(XX_TD(XX_BTN(Ctrl Shift,1Down))	XX_TD(MouseSelect()))
XX_TR(XX_TD(XX_BTN(Shift,1Down))	XX_TD(MoveCursor()))
XX_TR(XX_TD(XX_KEY(Meta,1))	XX_TD(PA(1)))
XX_TR(XX_TD(XX_KEY(Meta,2))	XX_TD(PA(2)))
XX_TR(XX_TD(XX_KEY(Meta,3))	XX_TD(PA(3)))
XX_TR(XX_TD(XX_KEY(Ctrl,a))	XX_TD(SelectAll(PRIMARY)))
XX_TR(XX_TD(XX_KEY(Meta,a))	XX_TD(Attn()))
XX_TR(XX_TD(XX_KEY(Meta,b))	XX_TD(PrintWindow()))
XX_TR(XX_TD(XX_KEY(Ctrl,c))	XX_TD(set-select(CLIPBOARD)))
XX_TR(XX_TD(XX_KEY(Meta,c))	XX_TD(Clear()))
XX_TR(XX_TD(XX_KEY(Meta,d))	XX_TD(Delete()))
XX_TR(XX_TD(XX_KEY(Meta,e))	XX_TD(EraseEOF()))
XX_TR(XX_TD(XX_KEY(Meta,f))	XX_TD(Flip()))
XX_TR(XX_TD(XX_KEY(Meta,h))	XX_TD(Home()))
XX_TR(XX_TD(XX_KEY(Meta,i))	XX_TD(Insert()))
XX_TR(XX_TD(XX_KEY(Meta,l))	XX_TD(Redraw()))
XX_TR(XX_TD(XX_KEY(Meta,p))	XX_TD(PrintText()))
XX_TR(XX_TD(XX_KEY(Meta,r))	XX_TD(Reset()))
XX_TR(XX_TD(XX_KEY(Meta,u))	XX_TD(Unselect()))
XX_TR(XX_TD(XX_KEY(Ctrl,u))	XX_TD(DeleteField()))
XX_TR(XX_TD(XX_KEY(Ctrl,v))	XX_TD(insert-selection(CLIPBOARD)))
XX_TR(XX_TD(XX_KEY(Meta,v))	XX_TD(ToggleReverse()))
XX_TR(XX_TD(XX_KEY(Ctrl,w))	XX_TD(DeleteWord()))
XX_TR(XX_TD(XX_KEY(Ctrl,x))	XX_TD(Cut(CLIPBOARD)))
XX_TE()
XX_PP
Meta is the diamond shaped key on a sun_k4, "Alt" on an XX_SM(NCD),
"Extend Char" on an XX_SM(HP).
The following
XX_FI(xmodmap)
command must be used on the XX_SM(NCD) to allow use the the "Alt"
key:
XX_PP
XX_RS(xmodmap XX_DASHED(e) "keysym Alt_L = Meta_L")
XX_PP
The left mouse button may be used to make a selection.
Clicking once unselects the current selection.
Clicking twice selects the word under the mouse cursor.
Clicking three times selects the line under the mouse cursor.
Clicking and dragging selects a rectangular area of the display.
XX_PP
The middle mouse button may be used to `paste' a selection.
XX_PP
The right mouse button may also be used for selections, selecting the
rectangular area between the current position and where the left button was
last pressed.
XX_PP
XX_PP
On color X displays, the "x3270.selectBackground" resource is used to
distinguish the selected text from the rest of the screen.
On monochrome X displays, selected text is in reverse video.
(It can be distinguished from a block cursor because the block cursor covers
slightly less than an entire character position on the screen.)
XX_PP
The left mouse button, when pressed with the "Shift" key held down, moves the
3270 cursor to the where the mouse cursor is pointing.
XX_PP
This is the complete list of keymap-callable actions.
Other actions are defined for use by scripts and are documented in
XX_FI(x3270-script)(1);
still others are defined for internal use by XX_FB(x3270)
and are not documented here.
Note that when an action with no parameters is used in a keymap, the
parentheses and empty argument list are still required.
