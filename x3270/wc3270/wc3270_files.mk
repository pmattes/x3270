# wc3270-specific source files
WC3270_SOURCES = XtGlue.c c3270.c ft_gui.c gdi_print.c glue.c help.c icmd.c \
	keymap.c keypad.c macros.c menubar.c nvt_gui.c pr3287_session.c \
	readres.c relink.c screen.c scroll.c select.c shortcut.c snprintf.c \
	ssl_dll.c ssl_passwd_gui.c w3misc.c windirs.c winprint.c winvers.c

# wc3270-specific object files
WC3270_OBJECTS = XtGlue.$(OBJ) c3270.$(OBJ) ft_gui.$(OBJ) gdi_print.$(OBJ) \
	glue.$(OBJ) help.$(OBJ) icmd.$(OBJ) keymap.$(OBJ) keypad.$(OBJ) \
	macros.$(OBJ) menubar.$(OBJ) nvt_gui.$(OBJ) pr3287_session.$(OBJ) \
	readres.$(OBJ) relink.$(OBJ) screen.$(OBJ) scroll.$(OBJ) \
	select.$(OBJ) shortcut.$(OBJ) snprintf.$(OBJ) ssl_dll.$(OBJ) \
	ssl_passwd_gui.$(OBJ) w3misc.$(OBJ) windirs.$(OBJ) winprint.$(OBJ) \
	winvers.$(OBJ)

# wc3270-specific header files
WC3270_HEADERS = X11/keysym.h cmenubar.h conf.h cscreen.h gdi_printc.h \
	gluec.h help.h icmdc.h pr3287_session.h readresc.h relinkc.h \
	shlobj_missing.h shortcutc.h ssl_dll.h wincmn.h windirsc.h winprint.h \
	winversc.h xscroll.h
