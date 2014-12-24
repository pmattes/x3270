# Source files common to all 3270 emulators
COMMON_SOURCES = actions.c apl.c asprintf.c charset.c ctlr.c fprint_screen.c \
	ft.c ft_cut.c ft_dft.c host.c idle.c kybd.c linemode.c nvt.c print.c \
	proxy.c resolver.c resources.c rpq.c see.c sf.c tables.c telnet.c \
	toggles.c trace_ds.c unicode.c unicode_dbcs.c utf8.c util.c varbuf.c \
	xio.c

# HTTPD source files
HTTPD_SOURCES = bind-opt.c favicon.c httpd-core.c httpd-io.c httpd-nodes.c

# Object files common to all 3270 emulators
COMMON_OBJECTS = actions.o apl.o asprintf.o charset.o ctlr.o fprint_screen.o \
	ft.o ft_cut.o ft_dft.o host.o idle.o kybd.o linemode.o nvt.o print.o \
	proxy.o resolver.o resources.o rpq.o see.o sf.o tables.o telnet.o \
	toggles.o trace_ds.o unicode.o unicode_dbcs.o utf8.o util.o varbuf.o \
	xio.o

# HTTPD object files
HTTPD_OBJECTS = bind-opt.o favicon.o httpd-core.o httpd-io.o httpd-nodes.o

# Header files common to all 3270 emulators
COMMON_HEADERS = 3270ds.h actionsc.h aplc.h appres.h arpa_telnet.h \
	asprintfc.h cg.h charsetc.h childc.h ctlr.h ctlrc.h dialogc.h \
	fallbacksc.h fprint_screenc.h ft_cut_ds.h ft_cutc.h ft_dft_ds.h \
	ft_dftc.h ft_guic.h ft_private.h ftc.h globals.h hostc.h idlec.h \
	keymapc.h keypadc.h kybdc.h linemodec.h localdefs.h macrosc.h \
	menubarc.h nvt_guic.h nvtc.h objects.h popupsc.h print_guic.h \
	printc.h proxy.h proxyc.h resolverc.h resources.h rpqc.h savec.h \
	screen.h screenc.h scrollc.h seec.h selectc.h sfc.h statusc.h \
	tablesc.h telnetc.h tn3270e.h togglesc.h trace_dsc.h unicode_dbcsc.h \
	unicodec.h utf8c.h utilc.h varbufc.h w3miscc.h xioc.h xl.h

# HTTPD header files
HTTPD_HEADERS = bind-optc.h httpd-corec.h httpd-ioc.h httpd-nodesc.h 
