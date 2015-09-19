# c3270-specific source files
C3270_SOURCES = c3270.c ft_gui.c help.c icmd.c keymap.c keypad.c menubar.c \
	screen.c ssl_passwd_gui.c

# c3270-specific object files
C3270_OBJECTS = c3270.$(OBJ) ft_gui.$(OBJ) help.$(OBJ) icmd.$(OBJ) \
	keymap.$(OBJ) keypad.$(OBJ) menubar.$(OBJ) screen.$(OBJ) \
	ssl_passwd_gui.$(OBJ)

# c3270-specific header files
C3270_HEADERS = ckeypad.h cmenubar.h conf.h cscreen.h cstatus.h help.h \
	icmdc.h keymap.h xscroll.h
