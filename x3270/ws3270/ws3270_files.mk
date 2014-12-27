# ws3270-specific source files
WS3270_SOURCES = XtGlue.c glue.c macros.c printer.c readres.c s3270.c \
	snprintf.c ssl_dll.c strtok_r.c w3misc.c windirs.c winprint.c \
	winvers.c

WS3270_OBJECTS = XtGlue.o glue.o macros.o printer.o readres.o s3270.o \
	snprintf.o ssl_dll.o strtok_r.o w3misc.o windirs.o winprint.o \
	winvers.o

# ws3270-specific header files
WS3270_HEADERS = X11/keysym.h conf.h gluec.h readresc.h ssl_dll.h wincmn.h \
	windirsc.h winprintc.h winversc.h
