# Source files for lib3270stubs.
LIB3270STUBS_SOURCES = ft_gui_stubs.c menubar_stubs.c nvt_gui_stubs.c \
	popups_stubs.c print_gui_stubs.c screen_stubs1.c screen_stubs2.c \
	screen_stubs3.c ssl_passwd_gui_stubs.c trace_gui_stubs.c

# Object files for lib3270stubs.
LIB3270STUBS_OBJECTS = ft_gui_stubs.$(OBJ) menubar_stubs.$(OBJ) \
	nvt_gui_stubs.$(OBJ) popups_stubs.$(OBJ) print_gui_stubs.$(OBJ) \
	screen_stubs1.$(OBJ) screen_stubs2.$(OBJ) screen_stubs3.$(OBJ) \
	ssl_passwd_gui_stubs.$(OBJ) trace_gui_stubs.$(OBJ)

# Header files for lib3270stubs.
LIB3270STUBS_HEADERS = conf.h globals.h localdefs.h menubar.h popupsc.h
