# wc3270-specific source files
WC3270_SOURCES = c3270.c ft_gui.c help.c icmd.c keymap.c keypad.c menubar.c \
	nvt_gui.c relink.c screen.c select.c shortcut.c ssl_passwd_gui.c

# wc3270-specific object files
WC3270_OBJECTS = c3270.$(OBJ) ft_gui.$(OBJ) help.$(OBJ) icmd.$(OBJ) \
	keymap.$(OBJ) keypad.$(OBJ) menubar.$(OBJ) nvt_gui.$(OBJ) \
	relink.$(OBJ) screen.$(OBJ) select.$(OBJ) shortcut.$(OBJ) \
	ssl_passwd_gui.$(OBJ)

# wc3270-specific header files
WC3270_HEADERS = ckeypad.h cmenubar.h conf.h cscreen.h cstatus.h help.h \
	icmdc.h keymap.h relinkc.h shortcutc.h wc3270.h wselectc.h xscroll.h
