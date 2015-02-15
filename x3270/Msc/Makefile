# Top-level nmake Makefile for suite3270

all: wc3270 ws3270 wpr3287

wc3270 ws3270 wpr3287: libw3270 libw3270stubs

libw3270:
	cd lib\w3270
	$(MAKE) /nologo /fMsc\Makefile
	cd ..\..

libw3270stubs:
	cd lib\w3270stubs
	$(MAKE) /nologo /fMsc\Makefile
	cd ..\..

wc3270: FORCE
	cd wc3270
	$(MAKE) /nologo /fMsc\Makefile
	cd ..

ws3270: FORCE
	cd ws3270
	$(MAKE) /nologo /fMsc\Makefile
	cd ..

clean: wc3270-clean ws3270-clean wpr3287-clean
wc3270-clean ws3270-clean wpr3287-clean: lib-clean
lib-clean:
	cd lib\w3270
	$(MAKE) /nologo /fMsc\Makefile clean
	cd ..\w3270stubs
	$(MAKE) /nologo /fMsc\Makefile clean
	cd ..\..
wc3270-clean:
	cd wc3270
	$(MAKE) /nologo /fMsc\Makefile clean
	cd ..
ws3270-clean:
	cd ws3270
	$(MAKE) /nologo /fMsc\Makefile clean
	cd ..
wpr3287-clean:
	cd wpr3287
	$(MAKE) /nologo /fMsc\Makefile clean
	cd ..

clobber: wc3270-clobber ws3270-clobber wpr3287-clobber
wc3270-clobber ws3270-clobber wpr3287-clobber: lib-clobber
lib-clobber: lib-clean
	cd lib\w3270
	$(MAKE) /nologo /fMsc\Makefile clobber
	cd ..\w3270stubs
	$(MAKE) /nologo /fMsc\Makefile clobber
	cd ..\..
wc3270-clobber: wc3270-clean
	cd wc3270
	$(MAKE) /nologo /fMsc\Makefile clobber
	cd ..
ws3270-clobber: ws3270-clean
	cd ws3270
	$(MAKE) /nologo /fMsc\Makefile clobber
	cd ..
wpr3287-clobber:
	cd wpr3287
	$(MAKE) /nologo /fMsc\Makefile clobber
	cd ..

FORCE:
