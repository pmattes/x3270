# Source files for lib3270.
LIB3270_SOURCES = Malloc.c XtGlue.c actions.c apl.c asprintf.c bind-opt.c \
	charset.c ctlr.c fprint_screen.c ft.c ft_cut.c ft_dft.c host.c \
	httpd-core.c httpd-io.c httpd-nodes.c idle.c kybd.c lazya.c \
	linemode.c macros.c nvt.c print_screen.c proxy.c readres.c resolver.c \
	resources.c rpq.c see.c sf.c tables.c telnet.c toggles.c trace.c \
	unicode.c unicode_dbcs.c utf8.c util.c varbuf.c xio.c

# Object files for lib3270.
LIB3270_OBJECTS = Malloc.$(OBJ) XtGlue.$(OBJ) actions.$(OBJ) apl.$(OBJ) \
	asprintf.$(OBJ) bind-opt.$(OBJ) charset.$(OBJ) ctlr.$(OBJ) \
	favicon.$(OBJ) fprint_screen.$(OBJ) ft.$(OBJ) ft_cut.$(OBJ) \
	ft_dft.$(OBJ) host.$(OBJ) httpd-core.$(OBJ) httpd-io.$(OBJ) \
	httpd-nodes.$(OBJ) idle.$(OBJ) kybd.$(OBJ) lazya.$(OBJ) \
	linemode.$(OBJ) macros.$(OBJ) nvt.$(OBJ) print_screen.$(OBJ) \
	proxy.$(OBJ) readres.$(OBJ) resolver.$(OBJ) resources.$(OBJ) \
	rpq.$(OBJ) see.$(OBJ) sf.$(OBJ) tables.$(OBJ) telnet.$(OBJ) \
	toggles.$(OBJ) trace.$(OBJ) unicode.$(OBJ) unicode_dbcs.$(OBJ) \
	utf8.$(OBJ) util.$(OBJ) varbuf.$(OBJ) xio.$(OBJ)

# Header files for lib3270.
LIB3270_HEADERS = conf.h localdefs.h
