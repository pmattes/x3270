PAGES = credits.html documentation-bugs.html documentation-faq.html \
	documentation-manpages.html documentation-misc.html \
	documentation-other.html documentation-relnotes.html \
	documentation-ssl.html documentation.html download.html index.html \
	license.html screenshots.html x026.html
SCREENS = c3270-thumb.png c3270.png cyrillic-thumb.png cyrillic.png \
	japanese-thumb.png japanese.png keypad-thumb.png keypad.png \
	wc3270-thumb.png wc3270.png x3270_main-thumb.png x3270_main.png
OTHERS = $(SCREENS) Brackets.html Build-c3270.html Build-pr3287.html \
	Build-s3270.html Build-tcl3270.html Build-wc3270.html \
	Build-x3270.html Charset.html HotBugs.html Keymap.html \
	SS-HCS12-1372-00.pdf Unix/c3270-man.html Unix/ibm_hosts.html \
	Unix/pr3287-man.html Unix/s3270-man.html Unix/tcl3270-man.html \
	Unix/x3270-man.html Unix/x3270-script.html Unix/x3270if.html \
	Windows/wc3270-man.html Windows/wc3270-script.html \
	Windows/wpr3287-man.html Windows/ws3270-man.html Windows/x3270if.html \
	Wishlist.html c3270-Resources.html c3270Keymap.html del.jpg delta.jpg \
	man/FAQ.html s3270-Resources.html styles.css tcl3270-Resources.html \
	wc3270-Resources.html wc3270Keymap.html ws3270-Resources.html \
	x026.gif x3270-Resources.html x3270.jpg

all: $(PAGES) webpage.tgz

# Rules for HTML files with the navigation bar.
credits.html: credits-body.html mkpages.bash
	./mkpages.bash credits
documentation.html: documentation-body.html mkpages.bash
	./mkpages.bash documentation
documentation-bugs.html: documentation-bugs-body.html mkpages.bash
	./mkpages.bash documentation-bugs
documentation-faq.html: documentation-faq-body.html mkpages.bash
	./mkpages.bash documentation-faq
documentation-manpages.html: documentation-manpages-body.html mkpages.bash
	./mkpages.bash documentation-manpages
documentation-misc.html: documentation-misc-body.html mkpages.bash
	./mkpages.bash documentation-misc
documentation-other.html: documentation-other-body.html mkpages.bash
	./mkpages.bash documentation-other
documentation-relnotes.html: documentation-relnotes-body.html mkpages.bash
	./mkpages.bash documentation-relnotes
documentation-ssl.html: documentation-ssl-body.html mkpages.bash
	./mkpages.bash documentation-ssl
download.html: download-body.html mkpages.bash
	./mkpages.bash download
index.html: index-body.html mkpages.bash
	./mkpages.bash index
license.html: license-body.html mkpages.bash
	./mkpages.bash license
screenshots.html: screenshots-body.html mkpages.bash
	./mkpages.bash screenshots
x026.html: x026-body.html mkpages.bash
	./mkpages.bash x026

webpage.tgz: $(PAGES) $(OTHERS) mkpages.bash Makefile
	tar -czhf webpage.tgz $(PAGES) $(OTHERS)
