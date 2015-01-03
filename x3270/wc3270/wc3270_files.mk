# wc3270-specific source files
WC3270_SOURCES = XtGlue.c c3270.c ft_gui.c gdi_print.c glue.c help.c icmd.c \
	keymap.c keypad.c macros.c menubar.c nvt_gui.c pr3287_session.c \
	readres.c relink.c screen.c scroll.c select.c shortcut.c snprintf.c \
	ssl_dll.c ssl_passwd_gui.c w3misc.c windirs.c winprint.c winvers.c

# wc3270-specific object files
WC3270_OBJECTS = XtGlue.o c3270.o ft_gui.o gdi_print.o glue.o help.o icmd.o \
	keymap.o keypad.o macros.o menubar.o nvt_gui.o pr3287_session.o \
	readres.o relink.o screen.o scroll.o select.o shortcut.o snprintf.o \
	ssl_dll.o ssl_passwd_gui.o w3misc.o windirs.o winprint.o winvers.o

# wc3270-specific header files
WC3270_HEADERS = X11/keysym.h conf.h gdi_printc.h gluec.h help.h icmdc.h \
	pr3287_session.h readresc.h relinkc.h shlobj_missing.h shortcutc.h \
	ssl_dll.h wincmn.h windirsc.h winprintc.h winversc.h
