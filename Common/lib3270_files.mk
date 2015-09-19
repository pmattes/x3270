# Source files for lib3270.
LIB3270_SOURCES = Malloc.c XtGlue.c actions.c apl.c b8.c bind-opt.c charset.c \
	ctlr.c event.c fprint_screen.c ft.c ft_cut.c ft_dft.c glue.c host.c \
	httpd-core.c httpd-io.c httpd-nodes.c idle.c kybd.c linemode.c \
	llist.c macros.c nvt.c print_screen.c readres.c resources.c rpq.c \
	sf.c telnet.c toggles.c trace.c util.c xio.c

# Object files for lib3270.
LIB3270_OBJECTS = Malloc.$(OBJ) XtGlue.$(OBJ) actions.$(OBJ) apl.$(OBJ) \
	b8.$(OBJ) bind-opt.$(OBJ) charset.$(OBJ) ctlr.$(OBJ) event.$(OBJ) \
	favicon.$(OBJ) fprint_screen.$(OBJ) ft.$(OBJ) ft_cut.$(OBJ) \
	ft_dft.$(OBJ) glue.$(OBJ) host.$(OBJ) httpd-core.$(OBJ) \
	httpd-io.$(OBJ) httpd-nodes.$(OBJ) idle.$(OBJ) kybd.$(OBJ) \
	linemode.$(OBJ) llist.$(OBJ) macros.$(OBJ) nvt.$(OBJ) \
	print_screen.$(OBJ) readres.$(OBJ) resources.$(OBJ) rpq.$(OBJ) \
	sf.$(OBJ) telnet.$(OBJ) toggles.$(OBJ) trace.$(OBJ) util.$(OBJ) \
	xio.$(OBJ)

# Header files for lib3270.
LIB3270_HEADERS =
