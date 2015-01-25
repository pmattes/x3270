# Source files common to all 3270 emulators
COMMON_SOURCES = apl.c charset.c ctlr.c fprint_screen.c ft.c ft_cut.c \
	ft_dft.c host.c idle.c kybd.c linemode.c nvt.c print_screen.c proxy.c \
	resources.c rpq.c sf.c tables.c telnet.c toggles.c trace.c unicode.c \
	unicode_dbcs.c util.c xio.c

# HTTPD source files
HTTPD_SOURCES = favicon.c httpd-core.c httpd-io.c httpd-nodes.c

# Object files common to all 3270 emulators
COMMON_OBJECTS = apl.$(OBJ) charset.$(OBJ) ctlr.$(OBJ) fprint_screen.$(OBJ) \
	ft.$(OBJ) ft_cut.$(OBJ) ft_dft.$(OBJ) host.$(OBJ) idle.$(OBJ) \
	kybd.$(OBJ) linemode.$(OBJ) nvt.$(OBJ) print_screen.$(OBJ) \
	proxy.$(OBJ) resources.$(OBJ) rpq.$(OBJ) sf.$(OBJ) tables.$(OBJ) \
	telnet.$(OBJ) toggles.$(OBJ) trace.$(OBJ) unicode.$(OBJ) \
	unicode_dbcs.$(OBJ) util.$(OBJ) xio.$(OBJ)

# HTTPD object files
HTTPD_OBJECTS = favicon.$(OBJ) httpd-core.$(OBJ) \
	httpd-io.$(OBJ) httpd-nodes.$(OBJ)

# Header files common to all 3270 emulators
COMMON_HEADERS = charsetc.h dialogc.h fallbacksc.h fprint_screenc.h keymap.h \
	kybdc.h localdefs.h menubar.h save.h telnetc.h

# HTTPD header files
HTTPD_HEADERS = 
