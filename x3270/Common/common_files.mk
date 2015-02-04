# Source files common to all 3270 emulators
COMMON_SOURCES = nvt.c resources.c

# Object files common to all 3270 emulators
COMMON_OBJECTS = nvt.$(OBJ) resources.$(OBJ)

# Header files common to all 3270 emulators
COMMON_HEADERS = fallbacksc.h localdefs.h
