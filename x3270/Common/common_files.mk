# Source files common to all 3270 emulators
COMMON_SOURCES = actions.c apl.c charset.c ctlr.c fprint_screen.c ft.c \
	ft_cut.c ft_dft.c host.c idle.c kybd.c linemode.c nvt.c \
	print_screen.c proxy.c resolver.c resources.c rpq.c sf.c tables.c \
	telnet.c toggles.c trace.c unicode.c unicode_dbcs.c util.c xio.c

# HTTPD source files
HTTPD_SOURCES = bind-opt.c favicon.c httpd-core.c httpd-io.c httpd-nodes.c

# Object files common to all 3270 emulators
COMMON_OBJECTS = actions.$(OBJ) apl.$(OBJ) charset.$(OBJ) ctlr.$(OBJ) \
	fprint_screen.$(OBJ) ft.$(OBJ) ft_cut.$(OBJ) ft_dft.$(OBJ) \
	host.$(OBJ) idle.$(OBJ) kybd.$(OBJ) linemode.$(OBJ) nvt.$(OBJ) \
	print_screen.$(OBJ) proxy.$(OBJ) resolver.$(OBJ) resources.$(OBJ) \
	rpq.$(OBJ) sf.$(OBJ) tables.$(OBJ) telnet.$(OBJ) toggles.$(OBJ) \
	trace.$(OBJ) unicode.$(OBJ) unicode_dbcs.$(OBJ) util.$(OBJ) \
	xio.$(OBJ)

# HTTPD object files
HTTPD_OBJECTS = bind-opt.$(OBJ) favicon.$(OBJ) httpd-core.$(OBJ) \
	httpd-io.$(OBJ) httpd-nodes.$(OBJ)

# Header files common to all 3270 emulators
COMMON_HEADERS = actions.h aplc.h appres.h arpa_telnet.h cg.h charsetc.h \
	childc.h ctlr.h ctlrc.h dialogc.h fallbacksc.h fprint_screenc.h \
	ft_cut_ds.h ft_cutc.h ft_dft_ds.h ft_dftc.h ft_guic.h ft_private.h \
	ftc.h globals.h hostc.h idlec.h keymap.h keypadc.h kybdc.h \
	linemodec.h localdefs.h macrosc.h menubar.h nvt_guic.h nvtc.h \
	objects.h popupsc.h print_guic.h print_screen.h proxy.h proxyc.h \
	resolverc.h resources.h rpqc.h save.h screenc.h scroll.h selectc.h \
	sfc.h ssl_passwd_gui.h statusc.h tables.h telnet_private.h telnetc.h \
	tn3270e.h toggles.h trace.h trace_gui.h unicode_dbcsc.h unicodec.h \
	utilc.h w3misc.h xio.h xl.h

# HTTPD header files
HTTPD_HEADERS = bind-optc.h httpd-corec.h httpd-ioc.h httpd-nodesc.h 
