# Source files for lib3270.
LIB3270_SOURCES = Malloc.c actions.c asprintf.c bind-opt.c lazya.c resolver.c \
	see.c utf8.c varbuf.c

# Object files for lib3270.
LIB3270_OBJECTS = Malloc.$(OBJ) actions.$(OBJ) asprintf.$(OBJ) \
	bind-opt.$(OBJ) lazya.$(OBJ) resolver.$(OBJ) see.$(OBJ) utf8.$(OBJ) \
	varbuf.$(OBJ)

# Header files for lib3270.
LIB3270_HEADERS = conf.h localdefs.h
