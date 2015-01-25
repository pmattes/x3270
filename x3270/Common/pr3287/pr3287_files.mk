# Source files common to 3287 emulators
PR3287_SOURCES = asprintf.c charset.c ctlr.c lazya.c pr3287.c proxy.c \
	resolver.c see.c sf.c tables.c telnet.c trace.c unicode.c \
	unicode_dbcs.c utf8.c util.c varbuf.c xtable.c

# Object files common to 3287 emulators
PR3287_OBJECTS = asprintf.$(OBJ) charset.$(OBJ) ctlr.$(OBJ) lazya.$(OBJ) \
	pr3287.$(OBJ) proxy.$(OBJ) resolver.$(OBJ) see.$(OBJ) sf.$(OBJ) \
	tables.$(OBJ) telnet.$(OBJ) trace.$(OBJ) unicode.$(OBJ) \
	unicode_dbcs.$(OBJ) utf8.$(OBJ) util.$(OBJ) varbuf.$(OBJ) \
	xtable.$(OBJ)

# Header files common to 3287 emulators
PR3287_HEADERS = arpa_telnet.h charsetc.h ctlrc.h globals.h localdefs.h \
	pr3287.h proxy.h proxyc.h sfc.h tables.h telnetc.h trace.h \
	unicode_dbcsc.h xtablec.h
