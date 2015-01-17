# c3270-specific source files
C3270_SOURCES = XtGlue.c c3270.c child.c ft_gui.c glue.c help.c icmd.c \
	keymap.c keypad.c macros.c menubar.c pr3287_session.c readres.c \
	screen.c scroll.c ssl_passwd_gui.c

# c3270-specific object files
C3270_OBJECTS = XtGlue.$(OBJ) c3270.$(OBJ) child.$(OBJ) ft_gui.$(OBJ) \
	glue.$(OBJ) help.$(OBJ) icmd.$(OBJ) keymap.$(OBJ) keypad.$(OBJ) \
	macros.$(OBJ) menubar.$(OBJ) pr3287_session.$(OBJ) readres.$(OBJ) \
	screen.$(OBJ) scroll.$(OBJ) ssl_passwd_gui.$(OBJ)

# c3270-specific header files
C3270_HEADERS = X11/keysym.h cmenubar.h conf.h gluec.h help.h icmdc.h \
	pr3287_session.h readresc.h xscroll.h
