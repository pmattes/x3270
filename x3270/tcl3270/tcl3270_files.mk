# tcl3270-specific source files
TCL3270_SOURCES = XtGlue.c glue.c idle_stubs.c readres.c tcl3270.c

# tcl3270-specific object files
TCL3270_OBJECTS = XtGlue.$(OBJ) glue.$(OBJ) idle_stubs.$(OBJ) readres.$(OBJ) \
	tcl3270.$(OBJ)

# tcl3270-specific header files
TCL3270_HEADERS = gluec.h readresc.h
