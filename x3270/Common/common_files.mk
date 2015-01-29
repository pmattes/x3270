# Source files common to all 3270 emulators
COMMON_SOURCES = charset.c ctlr.c host.c idle.c kybd.c nvt.c print_screen.c \
	proxy.c resources.c rpq.c sf.c telnet.c trace.c unicode.c \
	unicode_dbcs.c util.c xio.c

# HTTPD source files
HTTPD_SOURCES = favicon.c httpd-core.c httpd-io.c httpd-nodes.c

# Object files common to all 3270 emulators
COMMON_OBJECTS = charset.$(OBJ) ctlr.$(OBJ) host.$(OBJ) idle.$(OBJ) \
	kybd.$(OBJ) nvt.$(OBJ) print_screen.$(OBJ) proxy.$(OBJ) \
	resources.$(OBJ) rpq.$(OBJ) sf.$(OBJ) telnet.$(OBJ) trace.$(OBJ) \
	unicode.$(OBJ) unicode_dbcs.$(OBJ) util.$(OBJ) xio.$(OBJ)

# HTTPD object files
HTTPD_OBJECTS = favicon.$(OBJ) httpd-core.$(OBJ) httpd-io.$(OBJ) \
	httpd-nodes.$(OBJ)

# Header files common to all 3270 emulators
COMMON_HEADERS = fallbacksc.h localdefs.h

# HTTPD header files
HTTPD_HEADERS = 
