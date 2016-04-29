XX_SH(Keymaps)
The XX_FB(XX_DASHED(keymap)) option
or the XX_FB(XX_PRODUCT`'.keymap) resource
allow a custom keymap to be specified.
If the option XX_FB(XX_DASHED(keymap)) XX_FI(xxx) is given (or the
XX_FB(XX_PRODUCT`'.keymap) resource has the value XX_FI(xxx)),
XX_FB(XX_PRODUCT)
will look for a resource named XX_FB(XX_PRODUCT.keymap.)`'XX_FI(xxx).
If no resource definition is found, it will look for a file named
ifelse(XX_PRODUCT,c3270,`XX_FI(xxx)',XX_PRODUCT,wc3270,`XX_FI(xxx)XX_FB(.wc3270km)').
XX_LP
Multiple keymaps may be specified be separating their names with commas.
Definitions in later keymaps supercede those in earlier keymaps.
XX_LP
In addition, separate keymaps may be defined that apply only in 3270 mode or
XX_SM(NVT) mode.
For example, the resource definition
XX_FB(XX_PRODUCT.keymap.)`'XX_FI(xxx)`'XX_FB(.nvt)
or the file
ifelse(XX_PRODUCT,c3270,`'XX_FI(xxx)`'XX_FB(.nvt),
XX_PRODUCT,wc3270,`XX_FI(xxx)XX_FB(.nvt.wc3270km)')
will augment the definition
of keymap XX_FI(xxx) in XX_SM(NVT) mode.
Similarly, the resource definition
XX_FB(XX_PRODUCT.keymap.)`'XX_FI(xxx)`'XX_FB(.3270)
or the file
ifelse(XX_PRODUCT,c3270,`'XX_FI(xxx)`'XX_FB(.3270),
XX_PRODUCT,wc3270,`XX_FI(xxx)XX_FB(.3270.wc3270km)')
will augment the definition
of keymap XX_FI(xxx) in 3270 mode.
XX_LP
Temporary keymaps can also be added or removed while XX_PRODUCT is running
with the XX_FB(Keymap) action.
See XX_LINK(XX_X3270-script.html,XX_FI(XX_X3270-script)(1)) for details.
XX_LP
Each line (rule) in a keymap specifies actions to perform when a
particular key or sequence of keys is pressed.
Keymap rules have the following syntax:
XX_LP
ifelse(XX_PRODUCT,c3270,
`XX_RS(`[XX_FB(Meta)][XX_FB(Ctrl)]XX_FB(XX_KEY())`'XX_FI(key)...: XX_FI(action)[(XX_FI(param)[,...])] ...')
',
`XX_RS(`[XX_FI(modifier)...]XX_FB(XX_KEY())`'XX_FI(key)...: XX_FI(action)[(XX_FI(param)[,...])] ...')
')dnl
XX_LP
Here is a sample keymap definition from a file:
XX_LP
XX_RS(`! Lines beginning with ! are ignored and can
XX_BR()
! occur anywhere.
XX_BR()
ifelse(XX_PRODUCT,wc3270,`! The line below will be displayed
XX_BR()
! by the New Session Wizard.
XX_BR
!description: An example.
XX_BR
')dnl
! Definition of keymap xxx
XX_BR()
! XX_NBSP()When Alt-c is pressed, clear the screen.
XX_BR()
XX_KEY(Alt,c): Clear()
XX_BR()
! XX_NBSP()When PageUp is pressed, send PF7 to the host.
XX_BR()
ifelse(XX_PRODUCT,c3270,`XX_KEY(,PPAGE): PF(7)',
`XX_KEY(,PRIOR): PF(7)')
XX_BR()
! XX_NBSP()When Ctrl-a is pressed, then F1, send PF13
XX_BR()
! XX_NBSP()to the host.
XX_BR()
XX_KEY(Ctrl,a) XX_KEY(,F1): PF(13)')
XX_LP
Here is the same definition as a resource:
XX_LP
XX_RS(`! Lines beginning with ! are ignored, but NOT
XX_BR()
! within a definition.
XX_BR()
! Note that the XX_BACKSLASH() is required at the end of the
XX_BR()
! first line, and XX_BACKSLASH(n)`'XX_BACKSLASH() is
XX_BR()
! required at the end of every other line except
XX_BR()
! the last.
XX_BR()
! Definition of keymap xxx
XX_BR()
XX_PRODUCT.keymap.xxx: XX_BACKSLASH()
XX_BR()
XX_NBSP()XX_KEY(Alt,c): Clear() XX_BACKSLASH(n)`'XX_BACKSLASH()
XX_BR()
XX_NBSP()ifelse(XX_PRODUCT,c3270,`XX_KEY(,PPAGE): PF(7) XX_BACKSLASH(n)`'XX_BACKSLASH()',
`XX_KEY(,PRIOR): PF(7) \n\')
XX_BR()
XX_NBSP()XX_KEY(Ctrl,A) XX_KEY(,F1): PF(13)')
XX_LP
The optional
ifelse(XX_PRODUCT,wc3270,`XX_FB(Shift), ')dnl
XX_FB(Alt) or XX_FB(Ctrl) modifiers specify that
the
ifelse(XX_PRODUCT,wc3270,`XX_FB(Shift), ')dnl
XX_FB(Alt) and XX_FB(Ctrl) keys are pressed along with the
specified XX_FI(key), respectively.
ifelse(XX_PRODUCT,wc3270,
`The XX_FB(LeftCtrl), XX_FB(RightCtrl), XX_FB(LeftAlt), and XX_FB(RightAlt)
modifiers specifify a particular XX_FB(Ctrl) or XX_FB(Alt) key.
')dnl
ifelse(XX_PRODUCT,wc3270,`The XX_FB(Enhanced) modifier is also available;
XX_FB(Enhanced XX_LT()Key>ENTER) is the keypad XX_FB(Enter) key.
')dnl
XX_FI(Key) is either an XX_SM(ISO) 8859-1 symbol name, such as XX_FB(equal)
for XX_DQUOTED(=) and XX_FB(a) for XX_DQUOTED(a), or a symbolic
ifelse(XX_PRODUCT,c3270,`XX_FB(ncurses)',XX_PRODUCT,wc3270,`Windows') key name,
such as XX_FB(UP).
More than one XX_FI(key) can be specified, indicating that a sequence of
keys must be pressed in order for the rule to be matched.
The XX_FI(action) is an action from the XX_LINK(#Actions,XX_SM(ACTIONS)) list
above.
More than one XX_FI(action) may be specified; they will be executed in order.
XX_LP
Keymap entries are case-sensitive and modifier-specific.
This means that a keymap for the XX_FB(b) key will match only a
lowercase XX_FB(b).
Actions for uppercase XX_FB(B), or for XX_FB(Alt-B),
must be specified separately.
ifelse(XX_PRODUCT,wc3270,
`XX_LP
Available symbolic key names are:
XX_FB(ADD),
XX_FB(ALT),
XX_FB(APPS),
XX_FB(BACK) (BackSpace),
XX_FB(BackSpace) (alias for BACK),
XX_FB(CLEAR),
XX_FB(CTRL),
XX_FB(DECIMAL),
XX_FB(DELETE),
XX_FB(DIVIDE),
XX_FB(DOWN),
XX_FB(END),
XX_FB(Enter) (alias for RETURN),
XX_FB(ESCAPE),
XX_FB(Esc) (alias for ESCAPE),
XX_FB(EXECUTE),
XX_FB(F1),
XX_FB(F2),
XX_FB(F3),
XX_FB(F4),
XX_FB(F5),
XX_FB(F6),
XX_FB(F7),
XX_FB(F8),
XX_FB(F9),
XX_FB(F10),
XX_FB(F11),
XX_FB(F12),
XX_FB(F13),
XX_FB(F14),
XX_FB(F15),
XX_FB(F16),
XX_FB(F17),
XX_FB(F18),
XX_FB(F19),
XX_FB(F20),
XX_FB(F21),
XX_FB(F22),
XX_FB(F23),
XX_FB(F24),
XX_FB(HELP),
XX_FB(HOME),
XX_FB(INSERT),
XX_FB(LEFT),
XX_FB(LMENU),
XX_FB(LWIN) (Left Windows key),
XX_FB(MULTIPLY),
XX_FB(NEXT) (Page Down),
XX_FB(NUMLOCK),
XX_FB(NUMPAD0),
XX_FB(NUMPAD1),
XX_FB(NUMPAD2),
XX_FB(NUMPAD3),
XX_FB(NUMPAD4),
XX_FB(NUMPAD5),
XX_FB(NUMPAD6),
XX_FB(NUMPAD7),
XX_FB(NUMPAD8),
XX_FB(NUMPAD9),
XX_FB(PageUp) (alias for PRIOR),
XX_FB(PageDown) (alias for Next),
XX_FB(PAUSE),
XX_FB(PRINT),
XX_FB(PRIOR) (Page Up),
XX_FB(RETURN) (Enter),
XX_FB(RIGHT),
XX_FB(RMENU),
XX_FB(RWIN) (Right Windows key),
XX_FB(SCROLL),
XX_FB(SELECT),
XX_FB(SEPARATOR),
XX_FB(SHIFT),
XX_FB(SLEEP),
XX_FB(SNAPSHOT),
XX_FB(SUBTRACT),
XX_FB(TAB)
and
XX_FB(UP).
In addition, any Windows VKey code can be specified in hexadecimal with the
syntax XX_FB(VKEY-0x)XX_FI(nn).
')dnl
XX_LP
The base keymap is:
XX_LP
XX_TS(2,l l.)
XX_TR(XX_TD(Key)	XX_TD(Action))
XX_T_()
ifelse(XX_PRODUCT,c3270,
`XX_TR(XX_TD(XX_KEY(Ctrl,`]'))	XX_TD(Escape))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(Ctrl,a))	XX_TD(`Key(0x01)'))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(Ctrl,`]'))	XX_TD(`Key(0x1d)'))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(,Tab))	XX_TD(BackTab))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(,c))	XX_TD(Clear))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(,e))	XX_TD(Escape))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(,r))	XX_TD(Reset))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(,k))	XX_TD(Keypad))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(,l))	XX_TD(Redraw))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(,m))	XX_TD(Compose))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(,n))	XX_TD(Menu))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(,p))	XX_TD(PrintText))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(,^))	XX_TD(Key(notsign)))
XX_TR(XX_TD(XX_KEY(Ctrl,k))	XX_TD(Keypad))
XX_TR(XX_TD(XX_KEY(Ctrl,n))	XX_TD(Menu))
XX_TR(XX_TD(XX_KEY(,UP))	XX_TD(Up))
XX_TR(XX_TD(XX_KEY(,DOWN))	XX_TD(Down))
XX_TR(XX_TD(XX_KEY(,LEFT))	XX_TD(Left))
XX_TR(XX_TD(XX_KEY(,RIGHT))	XX_TD(Right))
XX_TR(XX_TD(XX_KEY(,`F(XX_FI(n))'))	XX_TD(`PF(XX_FI(n))'))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(,`F(XX_FI(n))'))	XX_TD(`PF(XX_FI(n)+12)'))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(,1))	XX_TD(`PA(1)'))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(,2))	XX_TD(`PA(2)'))
XX_TR(XX_TD(XX_KEY(Ctrl,a) XX_KEY(,3))	XX_TD(`PA(3)'))
XX_TR(XX_TD(XX_KEY(,PPAGE))	XX_TD(`Scroll(Backward)'))
XX_TR(XX_TD(XX_KEY(,NPAGE))	XX_TD(`Scroll(Forward)'))',
XX_PRODUCT,wc3270,
`XX_TR(XX_TD(XX_KEY(Alt,1))	XX_TD(`PA(1)'))
XX_TR(XX_TD(XX_KEY(Alt,2))	XX_TD(`PA(2)'))
XX_TR(XX_TD(XX_KEY(Alt,3))	XX_TD(`PA(3)'))
XX_TR(XX_TD(XX_KEY(Alt Ctrl,`]'))	XX_TD(`Key(0x1d)'))
XX_TR(XX_TD(XX_KEY(Ctrl,`]'))	XX_TD(Escape))
XX_TR(XX_TD(XX_KEY(Alt,^))	XX_TD(Key(notsign)))
XX_TR(XX_TD(XX_KEY(Ctrl,c))	XX_TD(Copy))
XX_TR(XX_TD(XX_KEY(Alt,k))	XX_TD(Keymap))
XX_TR(XX_TD(XX_KEY(Alt,l))	XX_TD(Redraw))
XX_TR(XX_TD(XX_KEY(Alt,m))	XX_TD(Compose))
XX_TR(XX_TD(XX_KEY(Alt,n))	XX_TD(Menu))
XX_TR(XX_TD(XX_KEY(Ctrl,p))	XX_TD(Paste))
XX_TR(XX_TD(XX_KEY(Alt,p))	XX_TD(PrintText))
XX_TR(XX_TD(XX_KEY(Ctrl,x))	XX_TD(Cut))
XX_TR(XX_TD(XX_KEY(,PRIOR))	XX_TD(`Scroll(Backward)'))
XX_TR(XX_TD(XX_KEY(,NEXT))	XX_TD(`Scroll(Forward)'))
XX_TR(XX_TD(XX_KEY(Shift,`F`'XX_FI(n)'))	XX_TD(`PF(XX_FI(n)+12)'))')
XX_TE()
XX_LP
The base 3270-mode keymap adds:
XX_LP
XX_TS(2,l l.)
XX_TR(XX_TD(Key)	XX_TD(Action))
XX_T_()
ifelse(XX_PRODUCT,c3270,
`XX_TR(XX_TD(XX_KEY(Ctrl,c))	XX_TD(Clear))
XX_TR(XX_TD(XX_KEY(Ctrl,d))	XX_TD(Dup))
XX_TR(XX_TD(XX_KEY(Ctrl,f))	XX_TD(FieldMark))
XX_TR(XX_TD(XX_KEY(Ctrl,i))	XX_TD(Tab))
XX_TR(XX_TD(XX_KEY(Ctrl,l))	XX_TD(Redraw))
XX_TR(XX_TD(XX_KEY(Ctrl,r))	XX_TD(Reset))
XX_TR(XX_TD(XX_KEY(Ctrl,u))	XX_TD(DeleteField))
XX_TR(XX_TD(XX_KEY(,BackSpace))	XX_TD(BackSpace))
XX_TR(XX_TD(XX_KEY(,Return))	XX_TD(Enter))
XX_TR(XX_TD(XX_KEY(,Tab))	XX_TD(Tab))
XX_TR(XX_TD(XX_KEY(,Linefeed))	XX_TD(Newline))
XX_TR(XX_TD(XX_KEY(,BACKSPACE))	XX_TD(BackSpace))
XX_TR(XX_TD(XX_KEY(,DC))	XX_TD(Delete))
XX_TR(XX_TD(XX_KEY(,HOME))	XX_TD(Home))
XX_TR(XX_TD(XX_KEY(,IC))	XX_TD(ToggleInsert))',
XX_PRODUCT,wc3270,
`XX_TR(XX_TD(XX_KEY(Ctrl,a))	XX_TD(Attn))
XX_TR(XX_TD(XX_KEY(Alt,a))	XX_TD(Attn))
XX_TR(XX_TD(XX_KEY(Alt,c))	XX_TD(Clear))
XX_TR(XX_TD(XX_KEY(Ctrl,d))	XX_TD(Dup))
XX_TR(XX_TD(XX_KEY(Alt,d))	XX_TD(Dup))
XX_TR(XX_TD(XX_KEY(Ctrl,f))	XX_TD(FieldMark))
XX_TR(XX_TD(XX_KEY(Alt,f))	XX_TD(FieldMark))
XX_TR(XX_TD(XX_KEY(Ctrl,h))	XX_TD(Erase))
XX_TR(XX_TD(XX_KEY(Alt,i))	XX_TD(Insert))
XX_TR(XX_TD(XX_KEY(Shift Ctrl,i))	XX_TD(BackTab))
XX_TR(XX_TD(XX_KEY(Ctrl,i))	XX_TD(Tab))
XX_TR(XX_TD(XX_KEY(Ctrl,j))	XX_TD(Newline))
XX_TR(XX_TD(XX_KEY(Ctrl,l))	XX_TD(Redraw))
XX_TR(XX_TD(XX_KEY(Ctrl,m))	XX_TD(Enter))
XX_TR(XX_TD(XX_KEY(Ctrl,r))	XX_TD(Reset))
XX_TR(XX_TD(XX_KEY(Alt,r))	XX_TD(Reset))
XX_TR(XX_TD(XX_KEY(Ctrl,u))	XX_TD(DeleteField))
XX_TR(XX_TD(XX_KEY(,INSERT))	XX_TD(ToggleInsert))
XX_TR(XX_TD(XX_KEY(Shift,TAB))	XX_TD(BackTab))
XX_TR(XX_TD(XX_KEY(,BACK))	XX_TD(Erase))
XX_TR(XX_TD(XX_KEY(Shift,END))	XX_TD(EraseEOF))
XX_TR(XX_TD(XX_KEY(,END))	XX_TD(FieldEnd))
XX_TR(XX_TD(XX_KEY(Ctrl,LEFT))	XX_TD(PreviousWord))
XX_TR(XX_TD(XX_KEY(Ctrl,RIGHT))	XX_TD(NextWord))
XX_TR(XX_TD(XX_KEY(Shift,LEFT))	XX_TD(SelectLeft))
XX_TR(XX_TD(XX_KEY(Shift,RIGHT))	XX_TD(SelectRight))
XX_TR(XX_TD(XX_KEY(Shift,UP))	XX_TD(SelectUp))
XX_TR(XX_TD(XX_KEY(Shift,DOWN))	XX_TD(SelectDown))
XX_TR(XX_TD(XX_KEY(,PRIOR))	XX_TD(`PF(7)'))
XX_TR(XX_TD(XX_KEY(,NEXT))	XX_TD(`PF(8)'))')
XX_TE()
ifelse(XX_PRODUCT,c3270,
`XX_SH(The Meta or Alt Key)
Some keyboards do not have a XX_FB(Meta) key.
Instead, they have an XX_FB(Alt) key.
Sometimes this key acts as a proper
XX_FB(Meta) key, that is, it is a modifier key that sets the high-order bit
(0x80) in the code that is transmitted for each key.
Other keyboards send a two-character sequence when
the XX_FB(Alt) key is pressed with another key: the Escape character (0x1b),
followed by the code for the other key.
XX_LP
The resource XX_FB(XX_PRODUCT.metaEscape) and the termcap XX_FB(km) attribute
control how XX_FB(XX_PRODUCT) will interpret these sequences.
When XX_FB(XX_PRODUCT.metaEscape) is set to XX_FB(true), or when
XX_FB(XX_PRODUCT.metaEscape) is set to XX_FB(auto) and the termcap XX_FB(km)
attribute is set, the keyboard is assumed to have a separate XX_FB(Meta) key.
The Escape key can be used as an ordinary data key and has no special meaning.
XX_LP
When XX_FB(XX_PRODUCT.metaEscape) is set to XX_FB(true), or when
XX_FB(XX_PRODUCT.metaEscape) is set to XX_FB(auto) and the termcap XX_FB(km)
attribute is not set, the keyboard is assumed to use the Escape character
as a prefix to indicate that the following character is supposed to have the
high-order bit set.
When XX_FB(XX_PRODUCT) sees an Escape character from the keyboard, it sets a short
timeout.
If another character arrives before the timeout expires, then XX_FB(XX_PRODUCT)
will combine the two characters, setting the high-order bit of the second.
In an event trace file, the combined character is listed as XX_FI(derived).
In a keymap, only the combined character or the XX_FB(Meta) prefix may be used.
The Escape key can still be used by itself, but only if there is a short pause
before pressing another key.
XX_LP
The default value for XX_FB(XX_PRODUCT.metaEscape) is XX_FB(auto).')dnl
