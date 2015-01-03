# x3270-specific source files
X3270_SOURCES = Cme.c CmeBSB.c CmeLine.c CmplxMenu.c Husk.c about.c child.c \
	dialog.c display8.c ft_gui.c idle_gui.c keymap.c keypad.c \
	keysym2ucs.c macros.c menubar.c nvt_gui.c popups.c pr3287_session.c \
	print_gui.c print_window.c printer_gui.c save.c screen.c scroll.c \
	select.c ssl_passwd_gui.c status.c stmenu.c trace_gui.c x3270.c xaa.c \
	xactions.c xkybd.c xutil.c

# x3270-specific object files
X3270_OBJECTS = Cme.$(OBJ) CmeBSB.$(OBJ) CmeLine.$(OBJ) CmplxMenu.$(OBJ) \
	Husk.$(OBJ) about.$(OBJ) child.$(OBJ) dialog.$(OBJ) display8.$(OBJ) \
	ft_gui.$(OBJ) idle_gui.$(OBJ) keymap.$(OBJ) keypad.$(OBJ) \
	keysym2ucs.$(OBJ) macros.$(OBJ) menubar.$(OBJ) nvt_gui.$(OBJ) \
	popups.$(OBJ) pr3287_session.$(OBJ) print_gui.$(OBJ) \
	print_window.$(OBJ) printer_gui.$(OBJ) save.$(OBJ) screen.$(OBJ) \
	scroll.$(OBJ) select.$(OBJ) ssl_passwd_gui.$(OBJ) status.$(OBJ) \
	stmenu.$(OBJ) trace_gui.$(OBJ) x3270.$(OBJ) xaa.$(OBJ) \
	xactions.$(OBJ) xkybd.$(OBJ) xutil.$(OBJ)

# x3270-specific header files
X3270_HEADERS = Cme.h CmeBSB.h CmeBSBP.h CmeLine.h CmeLineP.h CmeP.h \
	CmplxMenu.h CmplxMenuP.h Husk.h HuskP.h aboutc.h childc.h dialogc.h \
	display8c.h idle_guic.h keysym2ucs.h pr3287_session.h print_windowc.h \
	printer_guic.h resourcesc.h stmenuc.h w3miscc.h xaac.h xactionsc.h \
	xkybdc.h
