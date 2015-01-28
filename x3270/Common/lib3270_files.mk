# Source files for lib3270.
LIB3270_SOURCES = Malloc.c actions.c apl.c asprintf.c bind-opt.c httpd-core.c \
	httpd-io.c httpd-nodes.c lazya.c resolver.c see.c utf8.c varbuf.c

# Object files for lib3270.
LIB3270_OBJECTS = Malloc.$(OBJ) actions.$(OBJ) apl.$(OBJ) asprintf.$(OBJ) \
	bind-opt.$(OBJ) httpd-core.$(OBJ) httpd-io.$(OBJ) httpd-nodes.$(OBJ) \
	lazya.$(OBJ) resolver.$(OBJ) see.$(OBJ) utf8.$(OBJ) varbuf.$(OBJ)

# Header files for lib3270.
LIB3270_HEADERS = conf.h localdefs.h
