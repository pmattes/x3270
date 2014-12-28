# c3270-specific source files
C3270_SOURCES = XtGlue.c c3270.c child.c ft_gui.c glue.c help.c icmd.c \
	keymap.c keypad.c macros.c menubar.c printer.c readres.c screen.c \
	scroll.c ssl_passwd_gui.c

# c3270-specific object files
C3270_OBJECTS = XtGlue.o c3270.o child.o ft_gui.o glue.o help.o icmd.o \
	keymap.o keypad.o macros.o menubar.o printer.o readres.o screen.o \
	scroll.o ssl_passwd_gui.o

# c3270-specific header files
C3270_HEADERS = X11/keysym.h conf.h gluec.h icmdc.h printerc.h readresc.h
