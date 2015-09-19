# Source files for lib32xx.
LIB32XX_SOURCES = asprintf.c lazya.c proxy.c resolver.c see.c tables.c \
	unicode.c unicode_dbcs.c utf8.c varbuf.c xs_buffer.c

# Object files for lib32xx.
LIB32XX_OBJECTS = asprintf.$(OBJ) lazya.$(OBJ) proxy.$(OBJ) resolver.$(OBJ) \
	see.$(OBJ) tables.$(OBJ) unicode.$(OBJ) unicode_dbcs.$(OBJ) \
	utf8.$(OBJ) varbuf.$(OBJ) xs_buffer.$(OBJ)

# Header files for lib32xx.
LIB32XX_HEADERS = conf.h localdefs.h
