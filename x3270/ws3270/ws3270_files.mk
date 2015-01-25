# ws3270-specific source files
WS3270_SOURCES = XtGlue.c gdi_print.c glue.c macros.c readres.c s3270.c \
	strtok_r.c windirs.c winvers.c

WS3270_OBJECTS = XtGlue.$(OBJ) gdi_print.$(OBJ) glue.$(OBJ) macros.$(OBJ) \
	readres.$(OBJ) s3270.$(OBJ) strtok_r.$(OBJ) windirs.$(OBJ) \
	winvers.$(OBJ)

# ws3270-specific header files
WS3270_HEADERS = X11/keysym.h conf.h gdi_printc.h gluec.h readresc.h \
	windirsc.h winversc.h
