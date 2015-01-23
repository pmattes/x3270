all: unix windows
unix:
	cd 3270 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)
	cd 3270stubs && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)
windows:
	cd w3270 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)
	cd w3270stubs && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

clean: unix-clean windows-clean
unix-clean:
	cd 3270 && $(MAKE) clean
	cd 3270stubs && $(MAKE) clean
windows-clean:
	cd w3270 && $(MAKE) clean
	cd w3270stubs && $(MAKE) clean

clobber: unix-clobber windows-clobber
unix-clobber:
	cd 3270 && $(MAKE) clobber
	cd 3270stubs && $(MAKE) clobber
windows-clobber:
	cd w3270 && $(MAKE) clobber
	cd w3270stubs && $(MAKE) clobber

depend: unix-depend windows-depend
unix-depend:
	cd 3270 && $(MAKE) depend
	cd 3270stubs && $(MAKE) depend
windows-depend:
	cd w3270 && $(MAKE) depend
	cd w3270stubs && $(MAKE) depend
