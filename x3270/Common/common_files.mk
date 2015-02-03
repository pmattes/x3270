# Source files common to all 3270 emulators
COMMON_SOURCES = host.c nvt.c resources.c trace.c util.c xio.c

# Object files common to all 3270 emulators
COMMON_OBJECTS = host.$(OBJ) nvt.$(OBJ) resources.$(OBJ) trace.$(OBJ) \
	util.$(OBJ) xio.$(OBJ)

# Header files common to all 3270 emulators
COMMON_HEADERS = fallbacksc.h localdefs.h
