# c3270-specific source files
C3270_SOURCES = c3270.c ft_gui.c glue.c help.c icmd.c keymap.c keypad.c \
	menubar.c pr3287_session.c screen.c scroll.c ssl_passwd_gui.c

# c3270-specific object files
C3270_OBJECTS = c3270.$(OBJ) ft_gui.$(OBJ) glue.$(OBJ) help.$(OBJ) \
	icmd.$(OBJ) keymap.$(OBJ) keypad.$(OBJ) menubar.$(OBJ) \
	pr3287_session.$(OBJ) screen.$(OBJ) scroll.$(OBJ) \
	ssl_passwd_gui.$(OBJ)

# c3270-specific header files
C3270_HEADERS = ckeypad.h cmenubar.h conf.h cscreen.h cstatus.h help.h \
	icmdc.h keymap.h xscroll.h
