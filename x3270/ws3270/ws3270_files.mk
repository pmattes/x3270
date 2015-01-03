# ws3270-specific source files
WS3270_SOURCES = XtGlue.c glue.c macros.c readres.c s3270.c snprintf.c \
	ssl_dll.c strtok_r.c w3misc.c windirs.c winprint.c winvers.c

WS3270_OBJECTS = XtGlue.$(OBJ) glue.$(OBJ) macros.$(OBJ) readres.$(OBJ) \
	s3270.$(OBJ) snprintf.$(OBJ) ssl_dll.$(OBJ) strtok_r.$(OBJ) \
	w3misc.$(OBJ) windirs.$(OBJ) winprint.$(OBJ) winvers.$(OBJ)

# ws3270-specific header files
WS3270_HEADERS = X11/keysym.h conf.h gluec.h readresc.h ssl_dll.h wincmn.h \
	windirsc.h winprintc.h winversc.h
