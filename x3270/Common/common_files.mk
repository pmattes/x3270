# Source files common to all 3270 emulators
COMMON_SOURCES = charset.c ctlr.c host.c kybd.c nvt.c print_screen.c proxy.c \
	resources.c telnet.c trace.c unicode.c unicode_dbcs.c util.c xio.c

# Object files common to all 3270 emulators
COMMON_OBJECTS = charset.$(OBJ) ctlr.$(OBJ) host.$(OBJ) kybd.$(OBJ) \
	nvt.$(OBJ) print_screen.$(OBJ) proxy.$(OBJ) resources.$(OBJ) \
	telnet.$(OBJ) trace.$(OBJ) unicode.$(OBJ) unicode_dbcs.$(OBJ) \
	util.$(OBJ) xio.$(OBJ)

# Header files common to all 3270 emulators
COMMON_HEADERS = fallbacksc.h localdefs.h
