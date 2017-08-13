# wb3270-specific source files
WB3270_SOURCES = async.c b3270.c b_ft.c b_popups.c b_status.c screen.c \
	ui_stream.c

WB3270_OBJECTS = async.$(OBJ) b3270.$(OBJ) b_ft.$(OBJ) b_popups.$(OBJ) \
	b_status.$(OBJ) screen.$(OBJ) ui_stream.$(OBJ)

# wb3270-specific header files
WB3270_HEADERS = conf.h
