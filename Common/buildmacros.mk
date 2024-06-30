# Default Settings for BUILD_CC, BUILD_LDFLAGS, BUILD_LIBS and BUILD_CFLAGS.

ifeq ($(strip $(BUILD_CC)),)
 # BUILD_CC is not set. Default it to CC.
 BUILD_CC=$(CC)

 # If they are not set, default BUILD_LDFLAGS to LDFLAGS, BUILD_LIBS to LIBS and BUILD_CFLAGS to CFLAGS.
 ifeq ($(strip $(BUILD_LDFLAGS)),)
  BUILD_LDFLAGS = $(LDFLAGS)
 endif
 ifeq ($(strip $(BUILD_LIBS)),)
  BUILD_LIBS = $(LIBS)
 endif
 ifeq ($(strip $(BUILD_CFLAGS)),)
  BUILD_CFLAGS = $(CFLAGS)
 endif
endif
# Otherwise (BUILD_CC is not set), do not default them.
