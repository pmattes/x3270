ALL_BUT_WEBPAGE = c3270-dir pr3287-dir s3270-dir tcl3270-dir wc3270-dir \
	wpr3287-dir ws3270-dir x3270-dir
ALL = $(ALL_BUT_WEBPAGE) Webpage-dir

all: $(ALL)

.fake: $(ALL)

$(ALL_BUT_WEBPAGE): Webpage-dir

Webpage-dir: Shared-dir

Shared-dir: lib-dir

lib-dir:
	cd lib && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

Shared-dir:
	cd Shared && $(MAKE)

Webpage-dir:
	cd Webpage && $(MAKE)

c3270-dir:
	cd c3270 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

pr3287-dir:
	cd pr3287 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

s3270-dir:
	cd s3270 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

tcl3270-dir:
	cd tcl3270 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

wc3270-dir:
	cd wc3270 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

wpr3287-dir:
	cd wpr3287 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

ws3270-dir:
	cd ws3270 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

x3270-dir:
	cd x3270 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

depend:
	cd lib && $(MAKE) $@
	cd c3270 && $(MAKE) $@
	cd pr3287 && $(MAKE) $@
	cd s3270 && $(MAKE) $@
	cd tcl3270 && $(MAKE) $@
	cd wc3270 && $(MAKE) $@
	cd wpr3287 && $(MAKE) $@
	cd ws3270 && $(MAKE) $@
	cd x3270 && $(MAKE) $@

clean:
	cd lib && $(MAKE) $@
	cd c3270 && $(MAKE) $@
	cd pr3287 && $(MAKE) $@
	cd s3270 && $(MAKE) $@
	cd tcl3270 && $(MAKE) $@
	cd wc3270 && $(MAKE) $@
	cd wpr3287 && $(MAKE) $@
	cd ws3270 && $(MAKE) $@
	cd x3270 && $(MAKE) $@
	cd Webpage && $(MAKE) $@
