# tcl3270-specific source files
TCL3270_SOURCES = XtGlue.c glue.c menubar_stubs.c readres.c tcl3270.c

# tcl3270-specific object files
TCL3270_OBJECTS = XtGlue.$(OBJ) glue.$(OBJ) menubar_stubs.$(OBJ) \
	readres.$(OBJ) tcl3270.$(OBJ)

# tcl3270-specific header files
TCL3270_HEADERS = X11/keysym.h gluec.h readresc.h
