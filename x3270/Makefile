ALL_BUT_WEBPAGE = c3270-dir pr3287-dir s3270-dir tcl3270-dir wc3270-dir \
	wpr3287-dir ws3270-dir x3270-dir
ALL = $(ALL_BUT_WEBPAGE) Webpage-dir

all: $(ALL)

.fake: $(ALL)

$(ALL_BUT_WEBPAGE): Webpage-dir

c3270-dir:
	cd c3270 && make -f Makefile.aux bootstrap && mk

pr3287-dir:
	cd pr3287 && make -f Makefile.aux bootstrap && mk

s3270-dir:
	cd s3270 && make -f Makefile.aux bootstrap && mk

tcl3270-dir:
	cd tcl3270 && make -f Makefile.aux bootstrap && mk

wc3270-dir:
	cd wc3270 && make -f Makefile.aux bootstrap && mk

wpr3287-dir:
	cd wpr3287 && make -f Makefile.aux bootstrap && mk

ws3270-dir:
	cd ws3270 && make -f Makefile.aux bootstrap && mk

x3270-dir:
	cd x3270 && make -f Makefile.aux bootstrap && mk

Webpage-dir:
	cd Webpage && make
