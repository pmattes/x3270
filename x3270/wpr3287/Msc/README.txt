This directory contains files needed to build wpr3287 with command-line
Microsoft tools:

    Makefile
	Makefile for nmake.  To use this Makefile, cd to the directory above
	this one and run:
		nmake /f Msc\Makefile

    mkversion.c
	C-language replacement for mkversion.sh and mkwversion.sh.

Official releases of wpr3287 are built and tested with MinGW, not the Microsoft
tools.  These files are provided as-is, with no guarantee that the resulting
executables and DLLs will function properly.
