# Source files common to 3287 emulators
PR3287_SOURCES = charset.c ctlr.c pr3287.c sf.c telnet.c trace.c xtable.c

# Object files common to 3287 emulators
PR3287_OBJECTS = charset.$(OBJ) ctlr.$(OBJ) pr3287.$(OBJ) sf.$(OBJ) \
	telnet.$(OBJ) trace.$(OBJ) xtable.$(OBJ)

# Header files common to 3287 emulators
PR3287_HEADERS = ctlrc.h globals.h localdefs.h pr3287.h pr_telnet.h trace.h \
	xtablec.h
