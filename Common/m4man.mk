# Set up variables to build man pages.
MANDEP = m4man man.m4 html.m4 m4man Makefile.aux version.txt
MKMAN =  -t man  -I$(VPATH) -p $(PRODUCT) -v version.txt
MKHTML = -t html -I$(VPATH) -p $(PRODUCT) -v version.txt
