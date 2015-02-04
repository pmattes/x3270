# ws3270-specific source files
WS3270_SOURCES = glue.c s3270.c strtok_r.c windirs.c

WS3270_OBJECTS = glue.$(OBJ) s3270.$(OBJ) strtok_r.$(OBJ) windirs.$(OBJ)

# ws3270-specific header files
WS3270_HEADERS = conf.h windirsc.h
