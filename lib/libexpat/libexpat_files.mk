# Source files specific to libexpat.
LIBEXPAT_SOURCES = xmlparse.c xmlrole.c xmltok.c xmltok_impl.c xmltok_ns.c

# Object files specific to libexpat.
LIBEXPAT_OBJECTS = xmlparse.$(OBJ) xmlrole.$(OBJ) xmltok.$(OBJ) \
	xmltok_impl.$(OBJ) xmltok_ns.$(OBJ)

# Header files specific to libexpat.
LIBEXPAT_HEADERS = 
