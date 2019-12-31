# Root of the library object directory. Assumes $(TOP) and $(HOST) are set.
LIBOBJ = $(TOP)/obj/$(HOST)

# For Unix, include generated library dependencies.
ifndef WIN32_FLAGS
include $(TOP)/lib/32xx/32xx-deplibs.mk
endif

# Library depend macros.
DEP3270 = $(LIBOBJ)/lib3270/lib3270.a
DEP3270I = $(LIBOBJ)/lib3270i/lib3270i.a
DEP32XX = $(LIBOBJ)/lib32xx/lib32xx.a
DEP3270STUBS = $(LIBOBJ)/lib3270stubs/lib3270stubs.a
DEPLIBEXPAT = $(LIBOBJ)/libexpat/libexpat.a

# Library reference macros.
LD3270 = -L$(LIBOBJ)/lib3270 -l3270
LD3270I = -L$(LIBOBJ)/lib3270i -l3270i
LD32XX = -L$(LIBOBJ)/lib32xx -l32xx $(LIB32XX_DEPLIBS)
LD3270STUBS = -L$(LIBOBJ)/lib3270stubs -l3270stubs
LDLIBEXPAT = -L$(LIBOBJ)/libexpat -lexpat

# Library build rules.
$(DEP3270):
	cd $(TOP)/lib/$(W)3270 && $(MAKE) all
$(DEP3270I):
	cd $(TOP)/lib/$(W)3270i && $(MAKE) all
$(DEP32XX):
	cd $(TOP)/lib/$(W)32xx && $(MAKE) all
$(DEP3270STUBS):
	cd $(TOP)/lib/$(W)3270stubs && $(MAKE) all
$(DEPLIBEXPAT):
	cd $(TOP)/lib/libexpat && $(MAKE) all
