# Source files specific to wpr3287
WPR3287_SOURCES = snprintf.c ssl_dll.c w3misc.c windirs.c winvers.c ws.c

# Object files specific to wpr3287
WPR3287_OBJECTS = snprintf.$(OBJ) ssl_dll.$(OBJ) w3misc.$(OBJ) windirs.$(OBJ) \
	winvers.$(OBJ) ws.$(OBJ)

# Header files specific to wpr3287
WPR3287_HEADERS = ssl_dll.h wincmn.h windirsc.h winversc.h wsc.h
