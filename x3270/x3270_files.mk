# x3270-specific source files
X3270_SOURCES = Cme.c CmeBSB.c CmeLine.c CmplxMenu.c Husk.c about.c dialog.c \
	display8.c display_charsets.c display_charsets_dbcs.c ft_gui.c \
	host_gui.c idle_gui.c keymap.c keypad.c keysym2ucs.c menubar.c \
	nvt_gui.c popups.c print_gui.c print_window.c printer_gui.c \
	resources.c save.c screen.c select.c ssl_passwd_gui.c status.c \
	stmenu.c trace_gui.c x3270.c xaa.c xactions.c xkybd.c xtables.c \
	xutil.c

# x3270-specific object files
X3270_OBJECTS = Cme.$(OBJ) CmeBSB.$(OBJ) CmeLine.$(OBJ) CmplxMenu.$(OBJ) \
	Husk.$(OBJ) about.$(OBJ) dialog.$(OBJ) display8.$(OBJ) \
	display_charsets.$(OBJ) display_charsets_dbcs.$(OBJ) ft_gui.$(OBJ) \
	host_gui.$(OBJ) idle_gui.$(OBJ) keymap.$(OBJ) keypad.$(OBJ) \
	keysym2ucs.$(OBJ) menubar.$(OBJ) nvt_gui.$(OBJ) popups.$(OBJ) \
	print_gui.$(OBJ) print_window.$(OBJ) printer_gui.$(OBJ) \
	resources.$(OBJ) save.$(OBJ) screen.$(OBJ) select.$(OBJ) \
	ssl_passwd_gui.$(OBJ) status.$(OBJ) stmenu.$(OBJ) trace_gui.$(OBJ) \
	x3270.$(OBJ) xaa.$(OBJ) xactions.$(OBJ) xkybd.$(OBJ) xtables.$(OBJ) \
	xutil.$(OBJ)

# x3270-specific header files
X3270_HEADERS = Cme.h CmeBSB.h CmeBSBP.h CmeLine.h CmeLineP.h CmeP.h \
	CmplxMenu.h CmplxMenuP.h Husk.h HuskP.h about.h cg.h dialog.h \
	display8.h display_charsets.h display_charsets_dbcs.h idle_gui.h \
	keymap.h keysym2ucs.h objects.h print_window.h printer_gui.h \
	resourcesc.h stmenu.h xaa.h xactions.h xappres.h xglobals.h xkeypad.h \
	xkybd.h xmenubar.h xpopups.h xsave.h xscreen.h xscroll.h xselectc.h \
	xstatus.h xtables.h
