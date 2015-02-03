# Source files for lib3270.
LIB3270_SOURCES = Malloc.c actions.c apl.c asprintf.c bind-opt.c \
	fprint_screen.c ft.c ft_cut.c ft_dft.c httpd-core.c httpd-io.c \
	httpd-nodes.c idle.c lazya.c linemode.c resolver.c rpq.c see.c \
	tables.c toggles.c utf8.c varbuf.c

# Object files for lib3270.
LIB3270_OBJECTS = Malloc.$(OBJ) actions.$(OBJ) apl.$(OBJ) asprintf.$(OBJ) \
	bind-opt.$(OBJ) favicon.$(OBJ) fprint_screen.$(OBJ) ft.$(OBJ) \
	ft_cut.$(OBJ) ft_dft.$(OBJ) httpd-core.$(OBJ) httpd-io.$(OBJ) \
	httpd-nodes.$(OBJ) idle.$(OBJ) lazya.$(OBJ) linemode.$(OBJ) \
	resolver.$(OBJ) rpq.$(OBJ) see.$(OBJ) tables.$(OBJ) toggles.$(OBJ) \
	utf8.$(OBJ) varbuf.$(OBJ)

# Header files for lib3270.
LIB3270_HEADERS = conf.h localdefs.h
