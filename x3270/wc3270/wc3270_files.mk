# wc3270-specific source files
WC3270_SOURCES = c3270.c ft_gui.c glue.c help.c icmd.c keymap.c keypad.c \
	menubar.c nvt_gui.c pr3287_session.c relink.c screen.c scroll.c \
	select.c shortcut.c ssl_passwd_gui.c

# wc3270-specific object files
WC3270_OBJECTS = c3270.$(OBJ) ft_gui.$(OBJ) glue.$(OBJ) help.$(OBJ) \
	icmd.$(OBJ) keymap.$(OBJ) keypad.$(OBJ) menubar.$(OBJ) nvt_gui.$(OBJ) \
	pr3287_session.$(OBJ) relink.$(OBJ) screen.$(OBJ) scroll.$(OBJ) \
	select.$(OBJ) shortcut.$(OBJ) ssl_passwd_gui.$(OBJ)

# wc3270-specific header files
WC3270_HEADERS = ckeypad.h cmenubar.h conf.h cscreen.h cstatus.h help.h \
	icmdc.h keymap.h relinkc.h shlobj_missing.h shortcutc.h wc3270.h \
	wselectc.h xscroll.h
