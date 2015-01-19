# Source files for lib3270.
LIB3270_SOURCES = Malloc.c asprintf.c lazya.c see.c varbuf.c

# Object files for lib3270.
LIB3270_OBJECTS = Malloc.$(OBJ) asprintf.$(OBJ) lazya.$(OBJ) see.$(OBJ) \
	varbuf.$(OBJ)

# Header files for lib3270.
LIB3270_HEADERS = appres.h conf.h globals.h localdefs.h trace.h unicodec.h \
	utf8c.h utilc.h
