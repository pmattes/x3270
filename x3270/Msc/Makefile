# Top-level nmake Makefile for suite3270

all: wc3270 ws3270 wpr3287

wc3270 ws3270 wpr3287: libw3270 libw3270stubs

libw3270:
	cd lib\w3270
	$(MAKE) /fMsc\Makefile
	cd ..\..

libw3270stubs:
	cd lib\w3270stubs
	$(MAKE) /fMsc\Makefile
	cd ..\..

wc3270: FORCE
	cd wc3270
	$(MAKE) /fMsc\Makefile
	cd ..

ws3270: FORCE
	cd ws3270
	$(MAKE) /fMsc\Makefile
	cd ..

clean:
	cd lib\w3270
	$(MAKE) /fMsc\Makefile $@
	cd ..\w3270stubs
	$(MAKE) /fMsc\Makefile $@
	cd ..\..\wc3270
	$(MAKE) /fMsc\Makefile $@
	cd ..\ws3270
	$(MAKE) /fMsc\Makefile $@
	cd ..\wpr3287
	$(MAKE) /fMsc\Makefile $@
	cd ..

clobber: clean
	cd lib\w3270
	$(MAKE) /fMsc\Makefile $@
	cd ..\w3270stubs
	$(MAKE) /fMsc\Makefile $@
	cd ..\..\wc3270
	$(MAKE) /fMsc\Makefile $@
	cd ..\ws3270
	$(MAKE) /fMsc\Makefile $@
	cd ..\wpr3287
	$(MAKE) /fMsc\Makefile $@
	cd ..

FORCE:
