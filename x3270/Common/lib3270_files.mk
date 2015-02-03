# Source files for lib3270.
LIB3270_SOURCES = Malloc.c actions.c apl.c asprintf.c bind-opt.c charset.c \
	ctlr.c fprint_screen.c ft.c ft_cut.c ft_dft.c httpd-core.c httpd-io.c \
	httpd-nodes.c idle.c kybd.c lazya.c linemode.c print_screen.c proxy.c \
	readres.c resolver.c rpq.c see.c sf.c tables.c telnet.c toggles.c \
	unicode.c unicode_dbcs.c utf8.c varbuf.c

# Object files for lib3270.
LIB3270_OBJECTS = Malloc.$(OBJ) actions.$(OBJ) apl.$(OBJ) asprintf.$(OBJ) \
	bind-opt.$(OBJ) charset.$(OBJ) ctlr.$(OBJ) favicon.$(OBJ) \
	fprint_screen.$(OBJ) ft.$(OBJ) ft_cut.$(OBJ) ft_dft.$(OBJ) \
	httpd-core.$(OBJ) httpd-io.$(OBJ) httpd-nodes.$(OBJ) idle.$(OBJ) \
	kybd.$(OBJ) lazya.$(OBJ) linemode.$(OBJ) print_screen.$(OBJ) \
	proxy.$(OBJ) readres.$(OBJ) resolver.$(OBJ) rpq.$(OBJ) see.$(OBJ) \
	sf.$(OBJ) tables.$(OBJ) telnet.$(OBJ) toggles.$(OBJ) unicode.$(OBJ) \
	unicode_dbcs.$(OBJ) utf8.$(OBJ) varbuf.$(OBJ)

# Header files for lib3270.
LIB3270_HEADERS = conf.h localdefs.h
