# Source files common to all 3270 emulators
COMMON_SOURCES = actions.c apl.c asprintf.c charset.c ctlr.c fprint_screen.c \
	ft.c ft_cut.c ft_dft.c host.c idle.c kybd.c lazya.c linemode.c nvt.c \
	print_screen.c proxy.c resolver.c resources.c rpq.c see.c sf.c \
	tables.c telnet.c toggles.c trace.c unicode.c unicode_dbcs.c utf8.c \
	util.c varbuf.c xio.c

# HTTPD source files
HTTPD_SOURCES = bind-opt.c favicon.c httpd-core.c httpd-io.c httpd-nodes.c

# Object files common to all 3270 emulators
COMMON_OBJECTS = actions.$(OBJ) apl.$(OBJ) asprintf.$(OBJ) charset.$(OBJ) \
	ctlr.$(OBJ) fprint_screen.$(OBJ) ft.$(OBJ) ft_cut.$(OBJ) \
	ft_dft.$(OBJ) host.$(OBJ) idle.$(OBJ) kybd.$(OBJ) lazya.$(OBJ) \
	linemode.$(OBJ) nvt.$(OBJ) print_screen.$(OBJ) proxy.$(OBJ) \
	resolver.$(OBJ) resources.$(OBJ) rpq.$(OBJ) see.$(OBJ) sf.$(OBJ) \
	tables.$(OBJ) telnet.$(OBJ) toggles.$(OBJ) trace.$(OBJ) \
	unicode.$(OBJ) unicode_dbcs.$(OBJ) utf8.$(OBJ) util.$(OBJ) \
	varbuf.$(OBJ) xio.$(OBJ)

# HTTPD object files
HTTPD_OBJECTS = bind-opt.$(OBJ) favicon.$(OBJ) httpd-core.$(OBJ) \
	httpd-io.$(OBJ) httpd-nodes.$(OBJ)

# Header files common to all 3270 emulators
COMMON_HEADERS = 3270ds.h actions.h aplc.h appres.h arpa_telnet.h asprintfc.h \
	cg.h charsetc.h childc.h ctlr.h ctlrc.h dialogc.h fallbacksc.h \
	fprint_screenc.h ft_cut_ds.h ft_cutc.h ft_dft_ds.h ft_dftc.h \
	ft_guic.h ft_private.h ftc.h globals.h hostc.h idlec.h keymapc.h \
	keypadc.h kybdc.h lazya.h linemodec.h localdefs.h macrosc.h \
	menubarc.h nvt_guic.h nvtc.h objects.h popupsc.h print_guic.h \
	print_screen.h proxy.h proxyc.h resolverc.h resources.h rpqc.h \
	savec.h screen.h screenc.h scrollc.h see.h selectc.h sfc.h \
	ssl_passwd_guic.h statusc.h tables.h telnet_private.h telnetc.h \
	tn3270e.h togglesc.h trace.h trace_gui.h unicode_dbcsc.h unicodec.h \
	utf8c.h utilc.h varbufc.h w3miscc.h xioc.h xl.h

# HTTPD header files
HTTPD_HEADERS = bind-optc.h httpd-corec.h httpd-ioc.h httpd-nodesc.h 
