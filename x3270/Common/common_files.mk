# Source files common to all 3270 emulators
COMMON_SOURCES = host.c kybd.c nvt.c print_screen.c resources.c telnet.c \
	trace.c util.c xio.c

# Object files common to all 3270 emulators
COMMON_OBJECTS = host.$(OBJ) kybd.$(OBJ) nvt.$(OBJ) print_screen.$(OBJ) \
	resources.$(OBJ) telnet.$(OBJ) trace.$(OBJ) util.$(OBJ) xio.$(OBJ)

# Header files common to all 3270 emulators
COMMON_HEADERS = fallbacksc.h localdefs.h
