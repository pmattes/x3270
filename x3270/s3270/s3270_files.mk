# s3270-specific source files
S3270_SOURCES = XtGlue.c glue.c macros.c readres.c s3270.c

# s3270-specific object files
S3270_OBJECTS = XtGlue.$(OBJ) glue.$(OBJ) macros.$(OBJ) readres.$(OBJ) \
	s3270.$(OBJ)

# s3270-specific header files
S3270_HEADERS = gluec.h readresc.h
